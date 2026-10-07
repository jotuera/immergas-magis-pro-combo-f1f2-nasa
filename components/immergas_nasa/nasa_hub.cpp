// Samsung NASA bus hub for ESPHome - MIT (c) 2026 JoTu.
#include "nasa_hub.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/log.h"

namespace esphome {
namespace immergas_nasa {

static const char *const TAG = "immergas_nasa";
static const uint32_t RX_GAP_RESET_MS = 50;  // a pause inside a frame means it was truncated
static const uint32_t STATS_INTERVAL_MS = 10000;

// ---------------------------------------------------------------- ValueDecoder

bool ValueDecoder::decode(const Message &m, float &out) const {
  if (m.kind == MessageKind::STRUCTURE)
    return false;
  uint32_t v = m.value;
  for (uint32_t n : this->nan_values) {
    if (v == n) {
      out = NAN;
      return true;
    }
  }
  bool word = m.kind == MessageKind::VARIABLE;
  if (this->high_word) {
    v >>= 16;
    word = true;
  }
  double x;
  if (this->is_signed) {
    if (m.kind == MessageKind::ENUM)
      x = int8_t(v);
    else if (word)
      x = int16_t(v);
    else
      x = int32_t(v);
  } else {
    x = v;
  }
  out = float(x * this->multiply);
  return true;
}

// ---------------------------------------------------------------- setup / config

void NasaHub::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
    this->flow_control_pin_->digital_write(false);
  }
  this->rx_.reserve(256);
}

void NasaHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Samsung NASA hub:");
  ESP_LOGCONFIG(TAG, "  Own address: %s", this->address_.to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Transmit: %s", YESNO(this->transmit_));
  if (this->transmit_) {
    ESP_LOGCONFIG(TAG, "  Read address: %s", this->read_address_.to_string().c_str());
    ESP_LOGCONFIG(TAG, "  Quiet time: %u ms, response timeout: %u ms, attempts: %u", this->quiet_time_,
                  this->response_timeout_, this->max_attempts_);
    ESP_LOGCONFIG(TAG, "  Poll interval: %u s, polled messages: %u", this->poll_interval_ / 1000,
                  unsigned(this->poll_ids_.size()));
  }
  size_t n = 0;
  for (auto &kv : this->listeners_)
    n += kv.second.size();
  ESP_LOGCONFIG(TAG, "  Entities: %u (message ids: %u)", unsigned(n), unsigned(this->listeners_.size()));
  LOG_PIN("  Flow control pin: ", this->flow_control_pin_);
}

void NasaHub::register_listener(NasaListener *l) {
  this->listeners_[l->get_message_id()].push_back(l);
  if (l->get_poll() &&
      std::find(this->poll_ids_.begin(), this->poll_ids_.end(), l->get_message_id()) == this->poll_ids_.end())
    this->poll_ids_.push_back(l->get_message_id());
}

// ---------------------------------------------------------------- receive

void NasaHub::loop() {
  uint32_t now = millis();
  if (!this->rx_.empty() && now - this->last_rx_byte_ > RX_GAP_RESET_MS)
    this->rx_.clear();

  uint8_t buf[64];
  while (this->available() > 0) {
    size_t n = std::min<size_t>(this->available(), sizeof(buf));
    if (!this->read_array(buf, n))
      break;
    this->last_rx_byte_ = millis();
    this->feed(buf, n);
  }

  if (this->transmit_) {
    if (!this->poll_ids_.empty()) {
      bool due = !this->first_poll_done_ ? now > 30000 : now - this->last_poll_ >= this->poll_interval_;
      if (due)
        this->poll_now();
    }
    this->try_transmit_();
  }

  if (now - this->last_stats_ >= STATS_INTERVAL_MS) {
    this->last_stats_ = now;
    this->publish_stats_();
  }
}

void NasaHub::feed(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++)
    this->process_byte_(data[i]);
}

void NasaHub::process_byte_(uint8_t b) {
  if (this->rx_.empty() && b != NASA_START)
    return;  // hunt for a start byte
  this->rx_.push_back(b);
  if (this->rx_.size() < 3)
    return;
  size_t total = ((size_t(this->rx_[1]) << 8) | this->rx_[2]) + 2;
  if (total < NASA_MIN_FRAME || total > NASA_MAX_FRAME) {
    // Not a real start byte: drop it and resync on the next 0x32 already buffered.
    this->rx_.erase(this->rx_.begin());
    auto it = std::find(this->rx_.begin(), this->rx_.end(), NASA_START);
    this->rx_.erase(this->rx_.begin(), it);
    return;
  }
  if (this->rx_.size() < total)
    return;
  std::vector<uint8_t> frame;
  frame.swap(this->rx_);
  if (!this->process_frame_(frame.data(), frame.size())) {
    // Bad CRC / structure: the 0x32 was probably payload. Resync on the bytes after it.
    for (size_t i = 1; i < frame.size(); i++)
      this->process_byte_(frame[i]);
  }
}

bool NasaHub::process_frame_(const uint8_t *buf, size_t len) {
  Packet p;
  if (!p.decode(buf, len)) {
    this->crc_errors_++;
    ESP_LOGV(TAG, "Bad frame (%u B): %s", unsigned(len), hex_bytes(buf, len).c_str());
    return false;
  }
  this->rx_frames_++;
#ifdef USE_TEXT_SENSOR
  if (this->sniffer_ && this->last_frame_text_ != nullptr)
    this->last_frame_text_->publish_state(hex_bytes(buf, len));
#endif
  this->handle_packet_(p);
  return true;
}

void NasaHub::handle_packet_(const Packet &p) {
  if (p.src == this->address_)
    return;  // echo of our own transmission on a half-duplex bus

  if (this->sniffer_)
    this->sniff_(p);

  // Reply to our pending request?
  if (this->in_flight_ && p.dst == this->address_ && p.number == this->current_.number) {
    if (p.data_type == DataType::NACK) {
      this->nacks_++;
      ESP_LOGW(TAG, "NACK for request #%u", p.number);
      this->in_flight_ = false;
    } else if (p.data_type == DataType::RESPONSE || p.data_type == DataType::ACK) {
      this->in_flight_ = false;
    }
  }

  // Only frames that carry the sender's own values update entities.
  if (p.data_type != DataType::NOTIFICATION && p.data_type != DataType::RESPONSE)
    return;
  uint32_t src = p.src.raw();
  for (const Message &m : p.messages) {
    auto it = this->listeners_.find(m.id);
    if (it == this->listeners_.end())
      continue;
    for (NasaListener *l : it->second) {
      if (l->get_source() == src)
        l->on_message(p, m);
    }
  }
}

void NasaHub::sniff_(const Packet &p) {
  ESP_LOGD("sniff", "%s", p.to_string().c_str());
  if (p.data_type != DataType::NOTIFICATION && p.data_type != DataType::RESPONSE)
    return;
  for (const Message &m : p.messages) {
    if (m.kind == MessageKind::STRUCTURE)
      continue;
    uint32_t key = (p.src.raw() << 8) ^ m.id;  // compact key; collisions are harmless for logging
    auto it = this->sniff_last_.find(key);
    if (it != this->sniff_last_.end() && it->second == m.value)
      continue;
    char b[96];
    if (it == this->sniff_last_.end()) {
      snprintf(b, sizeof(b), "%s %04X = %u (new)", p.src.to_string().c_str(), m.id, unsigned(m.value));
    } else {
      snprintf(b, sizeof(b), "%s %04X: %u -> %u", p.src.to_string().c_str(), m.id, unsigned(it->second),
               unsigned(m.value));
    }
    this->sniff_last_[key] = m.value;
    ESP_LOGI("sniff", "%s", b);
#ifdef USE_TEXT_SENSOR
    if (this->last_change_text_ != nullptr)
      this->last_change_text_->publish_state(b);
#endif
  }
}

// ---------------------------------------------------------------- transmit

uint8_t NasaHub::next_packet_number_() {
  this->packet_number_++;
  if (this->packet_number_ == 0)
    this->packet_number_ = 1;
  return this->packet_number_;
}

void NasaHub::request_read(const std::vector<uint16_t> &ids) {
  if (!this->transmit_)
    return;
  for (size_t i = 0; i < ids.size(); i += NASA_MAX_MESSAGES_PER_FRAME) {
    PendingRequest r;
    r.dst = this->read_address_;
    size_t end = std::min(ids.size(), i + NASA_MAX_MESSAGES_PER_FRAME);
    for (size_t k = i; k < end; k++) {
      // A structure message must be alone in its frame.
      if (Message::kind_of(ids[k]) == MessageKind::STRUCTURE) {
        PendingRequest s;
        s.dst = this->read_address_;
        s.ids.push_back(ids[k]);
        this->queue_.push_back(s);
      } else {
        r.ids.push_back(ids[k]);
      }
    }
    if (!r.ids.empty())
      this->queue_.push_back(r);
  }
}

void NasaHub::poll_now() {
  this->first_poll_done_ = true;
  this->last_poll_ = millis();
  if (this->poll_ids_.empty())
    return;
  ESP_LOGD(TAG, "Polling %u messages", unsigned(this->poll_ids_.size()));
  this->request_read(this->poll_ids_);
}

void NasaHub::try_transmit_() {
  uint32_t now = millis();
  if (this->in_flight_) {
    if (now - this->current_.sent_at < this->response_timeout_)
      return;
    if (this->current_.attempts >= this->max_attempts_) {
      this->timeouts_++;
      ESP_LOGW(TAG, "No response to READ #%u (%u messages, first %04X) after %u attempts", this->current_.number,
               unsigned(this->current_.ids.size()), this->current_.ids.front(), this->current_.attempts);
      this->in_flight_ = false;
      return;
    }
    // retry below once the bus is quiet
  } else {
    if (this->queue_.empty())
      return;
  }
  // Talk only into silence and never in the middle of a frame being received.
  if (!this->rx_.empty() || now - this->last_rx_byte_ < this->quiet_time_)
    return;
  if (!this->in_flight_) {
    this->current_ = this->queue_.front();
    this->queue_.pop_front();
    this->current_.attempts = 0;
    this->current_.number = this->next_packet_number_();
    this->in_flight_ = true;
  }
  this->send_request_(this->current_);
}

void NasaHub::send_request_(PendingRequest &req) {
  Packet p;
  p.src = this->address_;
  p.dst = req.dst;
  p.packet_type = PacketType::NORMAL;
  p.data_type = DataType::READ;
  p.number = req.number;
  for (uint16_t id : req.ids) {
    Message m;
    m.id = id;  // READ carries zero payload of the size the id implies
    p.messages.push_back(m);
  }
  std::vector<uint8_t> frame = p.encode();
  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);
  this->write_array(frame);
  this->flush();
  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);
  req.attempts++;
  req.sent_at = millis();
  this->last_rx_byte_ = req.sent_at;  // our own frame occupies the bus as well
  this->tx_frames_++;
  if (this->sniffer_) {
    ESP_LOGD("sniff", "TX %s", p.to_string().c_str());
  }
}

// ---------------------------------------------------------------- stats

void NasaHub::publish_stats_() {
#ifdef USE_SENSOR
  if (this->rx_frames_sensor_ != nullptr)
    this->rx_frames_sensor_->publish_state(this->rx_frames_);
  if (this->crc_errors_sensor_ != nullptr)
    this->crc_errors_sensor_->publish_state(this->crc_errors_);
  if (this->tx_frames_sensor_ != nullptr)
    this->tx_frames_sensor_->publish_state(this->tx_frames_);
  if (this->timeouts_sensor_ != nullptr)
    this->timeouts_sensor_->publish_state(this->timeouts_);
#endif
}

}  // namespace immergas_nasa
}  // namespace esphome
