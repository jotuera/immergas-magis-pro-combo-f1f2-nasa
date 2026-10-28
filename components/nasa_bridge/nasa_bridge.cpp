#include "nasa_bridge.h"

#include <algorithm>  // std::min
#include <cmath>      // NAN, std::isnan, lroundf
#include <cstdio>     // snprintf

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"  // format_hex_pretty

namespace esphome {
namespace nasa_bridge {

static const char *const TAG = "nasa_bridge";

// ===========================================================================
//  Samsung NASA framing — PUBLIC reference summary
//  ---------------------------------------------------------------------------
//  Sourced from the public Samsung NASA reverse-engineering references
//  (MIM-B19N Modbus manual "Table 3 MessageSet", community NASA code lists).
//  All concrete numbers below marked [VERIFY] must be confirmed by sniffing
//  the real bus before trusting them. None of them are copied from any other
//  session's work — they are the publicly documented protocol constants.
//
//  Wire frame:
//    [0x32]                         start byte
//    [size_hi size_lo]              big-endian length of (src .. crc)      [VERIFY convention]
//    [src0 src1 src2]               source address (class, channel, addr)
//    [dst0 dst1 dst2]               destination address
//    [packet_info]                  bit7 = isInfo, low bits proto/retry
//    [type]                         high nibble = packet type,
//                                   low nibble  = data type (2 = WRITE)
//    [packet_number]                rolling counter
//    [capacity]                     number of message sets that follow
//    [ message sets ... ]           each: [msg_hi msg_lo][value...]
//    [crc_hi crc_lo]                checksum over (src .. last value)      [VERIFY poly]
//    [0x34]                         end byte
//
//  Message-set value size is encoded in the top two bits of msg_hi:
//    0b00 -> ENUM      1 byte
//    0b01 -> VARIABLE  2 bytes (uint16, big-endian)
//    0b10 -> LONG      4 bytes
//    0b11 -> STRUCTURE variable (not used by this bridge)
// ===========================================================================

static const uint8_t NASA_START = 0x32;
static const uint8_t NASA_END = 0x34;

// ---------------------------------------------------------------------------
//  Whitelist of the ONLY message(s) this component may ever transmit.
//
//  Corrected against the F1/F2 session's live captures (>11k frames):
//  0x428D is the message the MIM broadcasts to drive the ODU into quiet mode -
//  the ODU obeys it (sets 0x80B1=1, drops the Hz limit 0x8239 64->19). It is a
//  VARIABLE (2 bytes, big-endian): 0x011E = enabled + 30 %, 0x0000 = off.
//  (My earlier 0x4011/0x4087/0x4247 were wrong placeholders - 0x4087 is the
//  tank booster heater, 0x4247 is the Immergas-computed target temp.)
//
//  Reducing the compressor frequency ceiling is a conservative, reversible
//  comfort setting - not a safety-interlock override - and every write is
//  undone by a fail-safe auto-revert.
// ---------------------------------------------------------------------------
// FSV write target is configurable (fsv_register_), so we can step through the
// candidates F1/F2 vetted as safe: 0x424E (FSV 1031 water-out MAX heating),
// then the ODU derating ones. FSV temps are VARIABLE 2B = degC x 10.
// The indoor/MIM accepts WRITEs at 20.00.01; FSV READs are requested at B2.00.20.
static const uint8_t ADDR_INDOOR[3] = {0x20, 0x00, 0x01};
static const uint8_t ADDR_FSV_READ[3] = {0xB2, 0x00, 0x20};
static const uint8_t NASA_TYPE_WRITE = 0x12;
static const uint8_t NASA_TYPE_READ = 0x11;

// FR control (compressor frequency ratio / FSV 5051). On the RAW NASA bus these
// are TWO separate message numbers (the MIM Modbus register packs them into one,
// but we speak native NASA):
//   0x40A7  FSV 5051 FR control ON/OFF    - ENUM, 1 byte  (0 = off, 1 = on)
//   0x42F1  frequency ratio value         - VAR,  2 bytes (0-150 %, we cap <=100)
// Confirmed against the F1/F2 reference code (samsung-nasa-mqtt: 0x40A7 switch
// "FSV5051 FR control", 0x42F1 number "FR limit" 0-150 step 10) and live captures
// (42f1 0000, 40a7 00).
static const uint16_t MSG_FR_ENABLE = 0x40A7;  // FSV 5051 on/off (enum)
static const uint16_t MSG_FR_RATIO = 0x42F1;   // frequency ratio value (var)
static const uint16_t MSG_OUTING = 0x406D;     // ENUM_IN_OUTING_MODE (outing/away activation)
static const uint16_t MSG_SILENCE = 0x4046;    // ENUM_IN_SILENCE (Samsung native quiet mode)

bool NasaBridge::is_whitelisted_(uint16_t message_number) {
  // The FR-control registers, the outing-mode activation, and the legacy single
  // configured FSV, plus any catalog FSV flagged writable. The read-only group
  // (derating/forced-off/FR/PV/Smart-Grid) is never writable here.
  if (message_number == MSG_FR_ENABLE || message_number == MSG_FR_RATIO ||
      message_number == MSG_OUTING || message_number == MSG_SILENCE ||
      message_number == this->fsv_register_)
    return true;
  auto it = this->fsv_index_.find(message_number);
  return it != this->fsv_index_.end() && this->fsv_catalog_[it->second].writable;
}

// ---------------------------------------------------------------------------
//  CRC
//  [VERIFY] The Samsung NASA bus uses a 16-bit CRC. The exact polynomial/seed
//  is not 100% settled across public captures, so this is CRC-16/CCITT
//  (XMODEM: poly 0x1021, init 0x0000) as a starting point. If injected writes
//  are NACKed on real hardware, this is the first thing to correct. Because a
//  wrong CRC only causes the ODU to reject our frame, getting it wrong can
//  never corrupt or alter the pass-through Immergas traffic.
// ---------------------------------------------------------------------------
uint16_t NasaBridge::nasa_crc_(const uint8_t *data, size_t len) {
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; b++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }
  }
  return crc;
}

void NasaBridge::setup() {
  this->last_activity_ms_ = millis();
  if (this->mim_de_pin_ != nullptr) {
    this->mim_de_pin_->setup();
    this->mim_de_pin_->digital_write(false);  // idle = receive
  }
  // DISABLED (slim-down): the GET /fsv.csv dump is no longer registered, so the endpoint is not served.
  // canHandle()/handleRequest() stay as dead code (the class still derives AsyncWebHandler) - uncomment the
  // block below to restore the CSV dump.
  // if (this->web_server_base_ != nullptr) {
  //   this->web_server_base_->init();
  //   this->web_server_base_->add_handler(this);
  // }
  ESP_LOGCONFIG(TAG, "NASA bridge starting (write gate OFF by default; %u FSVs in catalog)",
                (unsigned) this->fsv_catalog_.size());
}

void NasaBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "NASA Bridge:");
  ESP_LOGCONFIG(TAG, "  Role: %s", this->relay_enabled_
                 ? "in-line relay (cut-through)" : "parallel TAP + inject");
  ESP_LOGCONFIG(TAG, "  Our addr (src): %02X.%02X.%02X -> ODU (dst): %02X.%02X.%02X",
                src_[0], src_[1], src_[2], dst_[0], dst_[1], dst_[2]);
  ESP_LOGCONFIG(TAG, "  WRITE GATE allow_write: %s", this->allow_write_ ? "ARMED" : "OFF (safe)");
  ESP_LOGCONFIG(TAG, "  Whitelist: 0x%04X FR-enable, 0x%04X FR-ratio, 0x%04X outing, 0x%04X silence, 0x%04X FSV",
                MSG_FR_ENABLE, MSG_FR_RATIO, MSG_OUTING, MSG_SILENCE, this->fsv_register_);
  ESP_LOGCONFIG(TAG, "  Frame type byte: 0x%02X (0x13 REQUEST / 0x12 WRITE / 0x14 NOTIFY)", this->type_byte_);
  ESP_LOGCONFIG(TAG, "  Fail-safe auto-revert: %u ms", (unsigned) revert_after_ms_);
  ESP_LOGCONFIG(TAG, "  Inject only in silence >= %u ms", (unsigned) inject_gap_ms_);
  ESP_LOGCONFIG(TAG, "  MIM/tap DE pin: %s", this->mim_de_pin_ != nullptr ? "configured" : "none (auto-direction)");
  ESP_LOGCONFIG(TAG, "  Diagnostic log every: %u ms", (unsigned) diag_interval_ms_);
  if (this->mim_uart_ == nullptr && this->odu_uart_ == nullptr)
    ESP_LOGE(TAG, "  At least one UART (the bus tap) MUST be configured!");
}

// ---------------------------------------------------------------------------
//  Cut-through, direction-latched forwarding.
//
//  Store-and-forward (buffer a whole frame, then send) was too slow: the extra
//  frame-time of latency per hop tripped the indoor's response timeout (A101).
//  Here we forward bytes the instant they arrive, but only one direction at a
//  time. While relaying A->B we never read B - the only thing on B's RX is the
//  echo of what we are driving onto it. When A stays silent for
//  turnaround_gap_ms_, we drain exactly that echo (burst_tx_ bytes) and release
//  the latch so the reply can flow back. Low latency, echo never propagates.
// ---------------------------------------------------------------------------
void NasaBridge::drain_(uart::UARTComponent *uart, uint32_t max_bytes) {
  if (uart == nullptr)
    return;
  uint32_t n = 0;
  while (n < max_bytes && uart->available()) {
    uint8_t b;
    if (!uart->read_byte(&b))
      break;
    n++;
  }
}

uint8_t NasaBridge::nasa_value_len_(uint16_t message_number) {
  // Bits 9-10 of the message number encode its value width.
  switch ((message_number & 0x0600) >> 9) {
    case 0: return 1;  // ENUM
    case 1: return 2;  // VARIABLE
    case 2: return 4;  // LONG
    default: return 0; // STRUCTURE: variable length, not parsed here
  }
}

void NasaBridge::parse_readback_(const uint8_t *f, size_t len) {
  // f: [0x32][size(2)][src(3)][dst(3)][info][type][pktnum][capacity][sets..][crc(2)][0x34]
  if (len < 15)
    return;
  uint8_t capacity = f[12];       // first message-set starts at index 13
  size_t i = 13;
  size_t crc_pos = len - 3;       // message-sets end before the CRC
  for (uint8_t m = 0; m < capacity && i + 2 <= crc_pos; m++) {
    uint16_t msg = (static_cast<uint16_t>(f[i]) << 8) | f[i + 1];
    uint8_t vlen = nasa_value_len_(msg);
    if (vlen == 0 || i + 2 + vlen > crc_pos)
      break;  // structure or overrun: stop parsing this frame
    uint32_t val = 0;
    for (uint8_t b = 0; b < vlen; b++)
      val = (val << 8) | f[i + 2 + b];
    // Record the raw value of EVERY message: catalog FSVs for fsv_value()/the dump,
    // plus non-catalog mode codes (e.g. 0x406D outing) for read_value().
    this->fsv_raw_[msg] = static_cast<int32_t>(val);
    if (msg == 0x8239 && this->hz_limit_sensor_ != nullptr)
      this->hz_limit_sensor_->publish_state(static_cast<float>(val));
    else if (msg == 0x80B1 && this->silence_sensor_ != nullptr)
      this->silence_sensor_->publish_state(static_cast<float>(val));
    else if (msg == this->fsv_register_) {
      ESP_LOGI(TAG, "FSV 0x%04X readback: src=%02X.%02X.%02X type=0x%02X raw=%u (%.1f degC)",
               this->fsv_register_, f[3], f[4], f[5], f[10], (unsigned) val,
               static_cast<float>(val) / 10.0f);
      if (this->fsv_sensor_ != nullptr)
        this->fsv_sensor_->publish_state(static_cast<float>(val) / 10.0f);
    } else if (msg == 0x4247 && this->setpoint_sensor_ != nullptr) {
      this->setpoint_sensor_->publish_state(static_cast<float>(val) / 10.0f);  // active setpoint
    } else if (msg == 0x4238 && this->flow_temp_sensor_ != nullptr) {
      this->flow_temp_sensor_->publish_state(static_cast<float>(val) / 10.0f);  // supply/flow temp
    } else if (msg == 0x8238 && this->comp_freq_sensor_ != nullptr) {
      this->comp_freq_sensor_->publish_state(static_cast<float>(val));  // compressor Hz (raw)
    } else if (msg == MSG_FR_ENABLE) {
      ESP_LOGI(TAG, "FR control enable 0x40A7 readback: %u (src=%02X.%02X.%02X type=0x%02X)",
               (unsigned) val, f[3], f[4], f[5], f[10]);
    } else if (msg == MSG_FR_RATIO) {
      ESP_LOGI(TAG, "FR ratio 0x42F1 readback: %u%% (src=%02X.%02X.%02X type=0x%02X)",
               (unsigned) val, f[3], f[4], f[5], f[10]);
      if (this->fr_sensor_ != nullptr)
        this->fr_sensor_->publish_state(static_cast<float>(val));
    }
    i += 2 + vlen;
  }
}

void NasaBridge::tap_read_(uart::UARTComponent *uart) {
  if (uart == nullptr)
    return;
  int available = uart->available();
  while (available-- > 0) {
    uint8_t b;
    if (!uart->read_byte(&b))
      break;
    this->mim_rx_bytes_++;
    this->last_activity_ms_ = millis();  // feeds silence detection for injection
    if (this->mim_preview_.size() < 32)
      this->mim_preview_.push_back(b);
    this->parse_buf_.push_back(b);
  }
  // Extract complete frames and parse their readback message-sets.
  while (true) {
    while (!this->parse_buf_.empty() && this->parse_buf_[0] != NASA_START)
      this->parse_buf_.erase(this->parse_buf_.begin());
    if (this->parse_buf_.size() < 3)
      break;
    uint16_t size = (static_cast<uint16_t>(this->parse_buf_[1]) << 8) | this->parse_buf_[2];
    if (size < 14 || size > 1500) {
      this->parse_buf_.erase(this->parse_buf_.begin());
      continue;
    }
    size_t total_len = static_cast<size_t>(size) + 2;
    if (this->parse_buf_.size() < total_len)
      break;
    if (this->parse_buf_[total_len - 1] == NASA_END) {
      this->mim_starts_++;
      this->mim_ends_++;
      const uint8_t *f = this->parse_buf_.data();
      // A frame from our own address is our own TX echoed back on the bus
      // (tap). It proves TX works, but it must NOT feed readback - otherwise the
      // sensor would just mirror what we wrote. Real readback = frames from the
      // devices (indoor 20.00.01 / ODU 10.00.00), never from us.
      if (f[3] == this->src_[0] && f[4] == this->src_[1] && f[5] == this->src_[2]) {
        ESP_LOGI(TAG, "SELF-TX seen on bus: our frame is physically on the wire (%u bytes)",
                 (unsigned) total_len);
      } else {
        this->parse_readback_(f, total_len);
      }
    }
    this->parse_buf_.erase(this->parse_buf_.begin(), this->parse_buf_.begin() + total_len);
  }
  if (this->parse_buf_.size() > 2048)
    this->parse_buf_.clear();
}

void NasaBridge::sniff_(uart::UARTComponent *uart, bool is_mim) {
  if (uart == nullptr)
    return;
  uint32_t &rx_count = is_mim ? this->mim_rx_bytes_ : this->odu_rx_bytes_;
  uint32_t &starts = is_mim ? this->mim_starts_ : this->odu_starts_;
  uint32_t &ends = is_mim ? this->mim_ends_ : this->odu_ends_;
  std::vector<uint8_t> &preview = is_mim ? this->mim_preview_ : this->odu_preview_;
  int available = uart->available();
  while (available-- > 0) {
    uint8_t b;
    if (!uart->read_byte(&b))
      break;
    rx_count++;
    if (preview.size() < 32)
      preview.push_back(b);
    if (b == NASA_START)
      starts++;
    else if (b == NASA_END)
      ends++;
  }
}

size_t NasaBridge::forward_(uart::UARTComponent *from, uart::UARTComponent *to, bool from_is_mim) {
  if (from == nullptr || to == nullptr)
    return 0;
  int available = from->available();
  if (available <= 0)
    return 0;

  uint32_t &rx_count = from_is_mim ? this->mim_rx_bytes_ : this->odu_rx_bytes_;
  uint32_t &tx_count = from_is_mim ? this->odu_tx_bytes_ : this->mim_tx_bytes_;
  uint32_t &starts = from_is_mim ? this->mim_starts_ : this->odu_starts_;
  uint32_t &ends = from_is_mim ? this->mim_ends_ : this->odu_ends_;
  std::vector<uint8_t> &preview = from_is_mim ? this->mim_preview_ : this->odu_preview_;
  std::vector<uint8_t> &capframe = from_is_mim ? this->mim_frame_ : this->odu_frame_;
  bool &capturing = from_is_mim ? this->mim_capturing_ : this->odu_capturing_;
  bool &frame_done = from_is_mim ? this->mim_frame_done_ : this->odu_frame_done_;

  std::vector<uint8_t> buf;
  buf.reserve(available);
  while (available-- > 0) {
    uint8_t b;
    if (!from->read_byte(&b))
      break;
    rx_count++;
    if (preview.size() < 32)
      preview.push_back(b);
    if (b == NASA_START) {
      starts++;
      if (!frame_done) { capframe.clear(); capturing = true; }
    } else if (b == NASA_END) {
      ends++;
    }
    if (capturing && capframe.size() < 64)
      capframe.push_back(b);
    if (b == NASA_END && capturing) { capturing = false; frame_done = true; }
    buf.push_back(b);
  }
  if (buf.empty())
    return 0;

  to->write_array(buf.data(), buf.size());  // non-blocking: forwarded as it flows
  tx_count += buf.size();
  this->last_activity_ms_ = millis();
  return buf.size();
}

bool NasaBridge::can_inject_now_(uint32_t now) const {
  if (this->inject_queue_.empty())
    return false;
  // NB: allow_write is enforced inside inject_write_ (WRITE only). READs are
  // benign and may go without arming, so we do not gate the whole queue here.
  if (this->dir_ != Dir::IDLE)
    return false;  // a transfer is latched
  return (now - this->last_activity_ms_) >= this->inject_gap_ms_;
}

void NasaBridge::loop() {
  uint32_t now = millis();

  if (!this->relay_enabled_) {
    // TAP mode (parallel, not in-line): read the bus (reassemble frames + parse
    // readback 0x8239/0x80B1/0x424B), and inject a queued op only in a silence
    // window (WRITE requires allow_write; READ is benign).
    this->tap_read_(this->mim_uart_);
    this->sniff_(this->odu_uart_, /*is_mim=*/false);

    // Background FSV poll: when nothing else is queued and the bus is quiet, read
    // the next catalog FSV (round-robin). One read per silence window keeps all
    // values fresh without overflowing the queue. User writes/reads take priority
    // (they fill the queue first, so no poll read is added until it drains).
    size_t poll_total = this->fsv_catalog_.size() + this->extra_poll_.size();
    if (this->auto_poll_ && this->inject_queue_.empty() && poll_total > 0 &&
        this->dir_ == Dir::IDLE && (now - this->last_activity_ms_) >= this->inject_gap_ms_) {
      // Alternate between an extra-poll code (live panel/status/error) and a catalog FSV, so the live
      // entities refresh ~2x per extra-list cycle while the static catalog still fills in the background.
      bool do_extra = !this->extra_poll_.empty() &&
                      (this->poll_extra_turn_ || this->fsv_catalog_.empty());
      this->poll_extra_turn_ = !this->poll_extra_turn_;
      uint16_t code;
      if (do_extra) {
        code = this->extra_poll_[this->extra_cursor_];
        this->extra_cursor_ = (this->extra_cursor_ + 1) % this->extra_poll_.size();
      } else {
        code = this->fsv_catalog_[this->poll_cursor_].code;
        this->poll_cursor_ = (this->poll_cursor_ + 1) % this->fsv_catalog_.size();
      }
      this->read_reg(code);
    }

    if (this->can_inject_now_(now)) {
      NasaWrite w = this->inject_queue_.front();
      this->inject_write_(w);
      this->inject_queue_.erase(this->inject_queue_.begin());
    }
    this->log_diagnostics_(now);
    return;
  }

  switch (this->dir_) {
    case Dir::IDLE:
      if (this->mim_uart_ != nullptr && this->mim_uart_->available()) {
        this->dir_ = Dir::MIM_TO_ODU;  // ODU side is auto-direction, no DE
        this->burst_tx_ = 0;
        this->last_fwd_ms_ = now;
      } else if (this->odu_uart_ != nullptr && this->odu_uart_->available()) {
        if (this->mim_de_pin_ != nullptr)
          this->mim_de_pin_->digital_write(true);  // enable the MIM driver
        this->dir_ = Dir::ODU_TO_MIM;
        this->burst_tx_ = 0;
        this->last_fwd_ms_ = now;
      } else {
        // Fully idle: diagnostics + optional injection.
        this->log_diagnostics_(now);
        if (this->can_inject_now_(now)) {
          NasaWrite w = this->inject_queue_.front();
          this->inject_write_(w);
          this->inject_queue_.erase(this->inject_queue_.begin());
        }
        return;
      }
      break;

    case Dir::MIM_TO_ODU: {
      size_t moved = this->forward_(this->mim_uart_, this->odu_uart_, /*from_is_mim=*/true);
      if (moved > 0) {
        this->burst_tx_ += moved;
        this->last_fwd_ms_ = now;
      } else if ((now - this->last_fwd_ms_) >= this->turnaround_gap_ms_) {
        this->odu_uart_->flush();                 // finish driving the burst
        this->drain_(this->odu_uart_, this->burst_tx_);  // discard its echo only
        this->dir_ = Dir::IDLE;
      }
      break;
    }

    case Dir::ODU_TO_MIM: {
      size_t moved = this->forward_(this->odu_uart_, this->mim_uart_, /*from_is_mim=*/false);
      if (moved > 0) {
        this->burst_tx_ += moved;
        this->last_fwd_ms_ = now;
      } else if ((now - this->last_fwd_ms_) >= this->turnaround_gap_ms_) {
        this->mim_uart_->flush();
        if (this->mim_de_pin_ != nullptr)
          this->mim_de_pin_->digital_write(false);  // release the MIM driver
        this->drain_(this->mim_uart_, this->burst_tx_);  // discard echo on MIM
        this->dir_ = Dir::IDLE;
      }
      break;
    }
  }

  this->log_diagnostics_(now);
}

void NasaBridge::log_diagnostics_(uint32_t now) {
  if (this->diag_interval_ms_ == 0)
    return;
  if ((now - this->last_diag_ms_) < this->diag_interval_ms_)
    return;
  this->last_diag_ms_ = now;

  ESP_LOGI(TAG, "DIAG relay=%s | MIM rx=%u tx=%u | ODU rx=%u tx=%u",
           this->relay_enabled_ ? "ON" : "OFF (listen-only)",
           (unsigned) this->mim_rx_bytes_, (unsigned) this->mim_tx_bytes_,
           (unsigned) this->odu_rx_bytes_, (unsigned) this->odu_tx_bytes_);
  // Frames reassembled and forwarded per side (fwd = extracted, ok = ended 0x34).
  ESP_LOGI(TAG, "DIAG frames | MIM fwd=%u ok=%u | ODU fwd=%u ok=%u",
           (unsigned) this->mim_starts_, (unsigned) this->mim_ends_,
           (unsigned) this->odu_starts_, (unsigned) this->odu_ends_);
  if (this->mim_frame_done_ && !this->mim_frame_logged_) {
    ESP_LOGI(TAG, "DIAG MIM first frame: %s", format_hex_pretty(this->mim_frame_).c_str());
    this->mim_frame_logged_ = true;
  }
  if (this->odu_frame_done_ && !this->odu_frame_logged_) {
    ESP_LOGI(TAG, "DIAG ODU first frame: %s", format_hex_pretty(this->odu_frame_).c_str());
    this->odu_frame_logged_ = true;
  }
  if (!this->mim_preview_.empty()) {
    ESP_LOGI(TAG, "DIAG MIM last bytes: %s", format_hex_pretty(this->mim_preview_).c_str());
    this->mim_preview_.clear();
  }
  if (!this->odu_preview_.empty()) {
    ESP_LOGI(TAG, "DIAG ODU last bytes: %s", format_hex_pretty(this->odu_preview_).c_str());
    this->odu_preview_.clear();
  }
}

bool NasaBridge::inject_write_(const NasaWrite &w) {
  bool is_write = (w.type_byte == NASA_TYPE_WRITE);
  // MASTER GATE: a WRITE never leaves the device unless armed. A READ is benign
  // (only asks for a value) so it is allowed without arming.
  if (is_write && !this->allow_write_) {
    ESP_LOGW(TAG, "WRITE BLOCKED (allow_write OFF): 0x%04X not sent", w.message_number);
    return false;
  }
  // Whitelist guards WRITES only. READs are benign (ask for a value, change
  // nothing), so any register may be read for diagnostics.
  if (is_write && !is_whitelisted_(w.message_number)) {
    ESP_LOGE(TAG, "REFUSED WRITE to non-whitelisted message 0x%04X", w.message_number);
    return false;
  }
  // The transmit transceiver is the tap: prefer the DE-capable MIM side.
  uart::UARTComponent *tx = this->mim_uart_ != nullptr ? this->mim_uart_ : this->odu_uart_;
  if (tx == nullptr || w.value_len == 0 || w.value_len > 4)
    return false;

  // Body = everything the CRC covers: src .. last value  (frame bytes [3 .. len-3)).
  std::vector<uint8_t> body;
  body.reserve(16);
  for (int i = 0; i < 3; i++) body.push_back(this->src_[i]);
  for (int i = 0; i < 3; i++) body.push_back(w.dst[i]);
  body.push_back(0xC0);                 // packet_info (per F1/F2 captures)
  body.push_back(w.type_byte);          // packetType<<4 | dataType (per operation)
  body.push_back(this->packet_number_++);
  body.push_back(0x01);                 // capacity: 1 message set
  body.push_back(static_cast<uint8_t>(w.message_number >> 8));
  body.push_back(static_cast<uint8_t>(w.message_number & 0xFF));
  for (uint8_t i = 0; i < w.value_len; i++)
    body.push_back(w.value[i]);

  uint16_t crc = nasa_crc_(body.data(), body.size());

  // Full frame = start(1)+size(2)+body+crc(2)+end(1). Decoder: total_len = size+2,
  // so size = total_len - 2 = body.size() + 4.
  uint16_t size = static_cast<uint16_t>(body.size() + 4);

  std::vector<uint8_t> frame;
  frame.reserve(body.size() + 6);
  frame.push_back(NASA_START);
  frame.push_back(static_cast<uint8_t>(size >> 8));
  frame.push_back(static_cast<uint8_t>(size & 0xFF));
  frame.insert(frame.end(), body.begin(), body.end());
  frame.push_back(static_cast<uint8_t>(crc >> 8));
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(NASA_END);

  // One burst, DE-gated if the tap transceiver needs it. We deliberately do NOT
  // drain our own frame here: on a parallel tap our transmission IS real bus
  // traffic, so reading it back (tap_read_ logs "SELF-TX") proves the bytes
  // actually reached the wire.
  bool use_de = (tx == this->mim_uart_) && this->mim_de_pin_ != nullptr;
  if (use_de) this->mim_de_pin_->digital_write(true);
  tx->write_array(frame.data(), frame.size());
  tx->flush();
  if (use_de) this->mim_de_pin_->digital_write(false);
  this->mim_tx_bytes_ += frame.size();
  this->last_activity_ms_ = millis();

  // Writes stay at INFO; benign READs (incl. the background poll) go to DEBUG so
  // continuous polling does not flood the log.
  if (w.type_byte == NASA_TYPE_READ) {
    ESP_LOGD(TAG, "SENT READ 0x%04X type=0x%02X dst=%02X.%02X.%02X (%u bytes)",
             w.message_number, w.type_byte, w.dst[0], w.dst[1], w.dst[2], (unsigned) frame.size());
  } else {
    ESP_LOGI(TAG, "SENT 0x%04X = 0x%02X%02X type=0x%02X dst=%02X.%02X.%02X (%u bytes)",
             w.message_number, w.value[0], w.value_len > 1 ? w.value[1] : 0,
             w.type_byte, w.dst[0], w.dst[1], w.dst[2], (unsigned) frame.size());
  }
  return true;
}

// ---------------------------------------------------------------------------
//  FSV write-capability test entry points (0x424B, cooling min-temp, inert).
//  Only queue the op; it leaves the device from loop() in a bus-silence window.
// ---------------------------------------------------------------------------
bool NasaBridge::write_fsv(float celsius) {
  if (celsius < 20.0f) celsius = 20.0f;   // sane heating-temp envelope
  if (celsius > 65.0f) celsius = 65.0f;
  uint16_t raw = static_cast<uint16_t>(celsius * 10.0f + 0.5f);
  NasaWrite w{};
  w.message_number = this->fsv_register_;
  w.value_len = 2;  // VARIABLE, big-endian, degC x10
  w.value[0] = static_cast<uint8_t>(raw >> 8);
  w.value[1] = static_cast<uint8_t>(raw & 0xFF);
  for (int i = 0; i < 3; i++) w.dst[i] = ADDR_INDOOR[i];   // write -> indoor/MIM
  w.type_byte = NASA_TYPE_WRITE;
  this->inject_queue_.push_back(w);
  ESP_LOGI(TAG, "Queued WRITE FSV 0x%04X = %.1f degC (raw 0x%04X)", this->fsv_register_, celsius, raw);
  return true;
}

bool NasaBridge::read_fsv() {
  NasaWrite w{};
  w.message_number = this->fsv_register_;
  w.value_len = 2;  // VARIABLE placeholder (zeros) for the read request
  w.value[0] = 0x00;
  w.value[1] = 0x00;
  for (int i = 0; i < 3; i++) w.dst[i] = ADDR_FSV_READ[i];  // FSV read -> B2.00.20
  w.type_byte = NASA_TYPE_READ;
  this->inject_queue_.push_back(w);
  ESP_LOGI(TAG, "Queued READ FSV 0x%04X", this->fsv_register_);
  return true;
}

bool NasaBridge::read_reg(int msg) {
  NasaWrite w{};
  w.message_number = static_cast<uint16_t>(msg);
  uint8_t vlen = nasa_value_len_(w.message_number);
  w.value_len = (vlen == 0) ? 2 : vlen;  // placeholder of the register's width
  for (uint8_t i = 0; i < w.value_len; i++)
    w.value[i] = 0x00;
  for (int i = 0; i < 3; i++) w.dst[i] = ADDR_FSV_READ[i];  // FSV read -> B2.00.20
  w.type_byte = NASA_TYPE_READ;
  this->inject_queue_.push_back(w);
  ESP_LOGD(TAG, "Queued READ 0x%04X", w.message_number);  // DEBUG: background poll floods this
  return true;
}

// ---------------------------------------------------------------------------
//  FR control (compressor frequency ratio / FSV 5051). Two native-NASA writes:
//  the enable enum 0x40A7 and the ratio value 0x42F1. Both are allow_write-gated
//  and whitelisted; the ratio is hard-capped at 100 % so this can only derate.
// ---------------------------------------------------------------------------
void NasaBridge::fr_set_enable(bool on) {
  NasaWrite w{};
  w.message_number = MSG_FR_ENABLE;       // 0x40A7
  w.value_len = 1;                        // ENUM, 1 byte
  w.value[0] = on ? 0x01 : 0x00;
  for (int i = 0; i < 3; i++) w.dst[i] = ADDR_INDOOR[i];  // R/W on indoor/MIM
  w.type_byte = NASA_TYPE_WRITE;
  this->fsv_raw_[MSG_FR_ENABLE] = on ? 1 : 0;  // optimistic (bus readback confirms / reverts)
  this->inject_queue_.push_back(w);
  ESP_LOGI(TAG, "Queued WRITE FR control enable 0x40A7 = %d", on);
}

void NasaBridge::fr_set_ratio(float ratio_pct) {
  if (ratio_pct < 50.0f) ratio_pct = 50.0f;    // manual minimum
  if (ratio_pct > 100.0f) ratio_pct = 100.0f;  // HARD CAP: derate only, never boost
  uint16_t raw = static_cast<uint16_t>(ratio_pct + 0.5f);
  NasaWrite w{};
  w.message_number = MSG_FR_RATIO;        // 0x42F1
  w.value_len = 2;                        // VARIABLE, big-endian, percent
  w.value[0] = static_cast<uint8_t>(raw >> 8);
  w.value[1] = static_cast<uint8_t>(raw & 0xFF);
  for (int i = 0; i < 3; i++) w.dst[i] = ADDR_INDOOR[i];  // R/W on indoor/MIM
  w.type_byte = NASA_TYPE_WRITE;
  this->fsv_raw_[MSG_FR_RATIO] = raw;  // optimistic (the Magis rejects it, so the poll reverts to 0xFFFF)
  this->inject_queue_.push_back(w);
  ESP_LOGI(TAG, "Queued WRITE FR ratio 0x42F1 = %u%% (raw 0x%04X)", (unsigned) raw, raw);
}

// ---------------------------------------------------------------------------
//  FSV catalog (all 94 installer settings). Loaded from bridge_fsv_catalog.yaml
//  by __init__.py. Read-only values feed the per-FSV template sensors; the
//  writable subset is editable from the Configuration section (gated).
// ---------------------------------------------------------------------------
void NasaBridge::add_fsv(uint16_t number, uint16_t code, float mul, bool is_signed, bool writable,
                         const std::string &name, const std::string &unit) {
  this->fsv_index_[code] = this->fsv_catalog_.size();
  this->fsv_catalog_.push_back(FsvEntry{number, code, mul, is_signed, writable, name, unit});
}

float NasaBridge::fsv_value(uint16_t code) {
  auto idx = this->fsv_index_.find(code);
  if (idx == this->fsv_index_.end())
    return NAN;
  auto raw_it = this->fsv_raw_.find(code);
  if (raw_it == this->fsv_raw_.end())
    return NAN;  // never read from the bus yet
  const FsvEntry &e = this->fsv_catalog_[idx->second];
  int32_t raw = raw_it->second;
  uint8_t vlen = nasa_value_len_(code);
  if (vlen == 1) {
    if ((raw & 0xFF) == 0xFF)
      return NAN;  // enum "not available" sentinel
    return static_cast<float>(raw & 0xFF) * e.mul;
  }
  uint16_t r16 = static_cast<uint16_t>(raw & 0xFFFF);
  if (r16 == 0xFFFF || (e.is_signed && r16 == 0xFE0C))
    return NAN;  // variable / temperature "not available" sentinels
  if (e.is_signed)
    return static_cast<float>(static_cast<int16_t>(r16)) * e.mul;
  return static_cast<float>(r16) * e.mul;
}

void NasaBridge::read_all() {
  for (auto &e : this->fsv_catalog_)
    this->read_reg(e.code);  // benign READ, drained one at a time in bus silence
  ESP_LOGI(TAG, "Queued READ of all %u catalog FSVs", (unsigned) this->fsv_catalog_.size());
}

bool NasaBridge::fsv_write(uint16_t code, float value) {
  auto idx = this->fsv_index_.find(code);
  if (idx == this->fsv_index_.end()) {
    ESP_LOGW(TAG, "fsv_write: 0x%04X not in catalog, ignored", code);
    return false;
  }
  const FsvEntry &e = this->fsv_catalog_[idx->second];
  if (!e.writable) {
    ESP_LOGW(TAG, "fsv_write: FSV %u (0x%04X) is read-only, refused", e.number, code);
    return false;
  }
  int32_t raw = static_cast<int32_t>(lroundf(value / e.mul));
  NasaWrite w{};
  w.message_number = code;
  uint8_t vlen = nasa_value_len_(code);
  w.value_len = (vlen == 0) ? 2 : vlen;
  if (w.value_len == 1) {
    w.value[0] = static_cast<uint8_t>(raw & 0xFF);
  } else {
    uint16_t r16 = static_cast<uint16_t>(raw & 0xFFFF);
    w.value[0] = static_cast<uint8_t>(r16 >> 8);
    w.value[1] = static_cast<uint8_t>(r16 & 0xFF);
  }
  for (int i = 0; i < 3; i++) w.dst[i] = ADDR_INDOOR[i];  // FSVs live on the indoor/MIM
  w.type_byte = NASA_TYPE_WRITE;
  // Optimistic local cache so the entity shows the new value on the FIRST click
  // (otherwise the lambda re-reads the pre-write bus value and snaps back, needing a
  // second click). The bus readback overwrites this, so a real revert still shows.
  this->fsv_raw_[code] = (w.value_len == 1) ? (raw & 0xFF) : (raw & 0xFFFF);
  this->inject_queue_.push_back(w);
  ESP_LOGI(TAG, "Queued WRITE FSV %u (0x%04X) = %.2f (raw %d)", e.number, code, value, (int) raw);
  return true;
}

// ---------------------------------------------------------------------------
//  Non-FSV operational modes (outing/away). read_value exposes the last raw value
//  of any code; outing_set writes ENUM_IN_OUTING_MODE 0x406D (gated + whitelisted).
// ---------------------------------------------------------------------------
float NasaBridge::read_value(uint16_t code) {
  auto it = this->fsv_raw_.find(code);
  if (it == this->fsv_raw_.end())
    return NAN;
  int32_t raw = it->second;
  uint8_t vlen = nasa_value_len_(code);
  if (vlen == 1) {
    if ((raw & 0xFF) == 0xFF)
      return NAN;
    return static_cast<float>(raw & 0xFF);
  }
  uint16_t r16 = static_cast<uint16_t>(raw & 0xFFFF);
  if (r16 == 0xFFFF)
    return NAN;
  return static_cast<float>(r16);
}

void NasaBridge::outing_set(bool on) {
  NasaWrite w{};
  w.message_number = MSG_OUTING;  // 0x406D, ENUM 1 byte
  w.value_len = 1;
  w.value[0] = on ? 0x01 : 0x00;
  for (int i = 0; i < 3; i++) w.dst[i] = ADDR_INDOOR[i];
  w.type_byte = NASA_TYPE_WRITE;
  this->fsv_raw_[MSG_OUTING] = on ? 1 : 0;  // optimistic (bus readback confirms / reverts)
  this->inject_queue_.push_back(w);
  ESP_LOGI(TAG, "Queued WRITE outing mode 0x406D = %d", on);
}

// Samsung-native quiet mode (ENUM_IN_SILENCE 0x4046). On a Magis the quiet mode is normally driven by
// Immergas via U11 (D+/D- -> 0x428D broadcast -> Hz limit 0x8239 64->19); this writes 0x4046 directly to
// see whether the Samsung side honours it independently. Watch 0x8239 (Hz limit) and 0x80B1 (running-under-
// limit flag) for the effect. Needs "Arm write". Readback confirms / reverts.
void NasaBridge::silence_set(bool on) {
  NasaWrite w{};
  w.message_number = MSG_SILENCE;  // 0x4046, ENUM 1 byte
  w.value_len = 1;
  w.value[0] = on ? 0x01 : 0x00;
  for (int i = 0; i < 3; i++) w.dst[i] = ADDR_INDOOR[i];
  w.type_byte = NASA_TYPE_WRITE;
  this->fsv_raw_[MSG_SILENCE] = on ? 1 : 0;  // optimistic (bus readback confirms / reverts)
  this->inject_queue_.push_back(w);
  ESP_LOGI(TAG, "Queued WRITE silence mode 0x4046 = %d", on);
}

// ---------------------------------------------------------------------------
//  GET /fsv.csv  - a CSV dump of every catalog FSV and its last-read value, so
//  the user can `curl http://<device>/fsv.csv > fsv.csv`. Reads existing state
//  only; never transmits. Body is a few KB, fine for the buffered IDF response.
// ---------------------------------------------------------------------------
bool NasaBridge::canHandle(AsyncWebServerRequest *request) const {
  // web_server_idf (ESP-IDF) has no url(); it decodes the path into a caller buffer.
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  return request->url_to(buf) == "/fsv.csv";
}

void NasaBridge::handleRequest(AsyncWebServerRequest *request) {
  AsyncResponseStream *stream = request->beginResponseStream("text/csv");
  stream->addHeader("Content-Disposition", "attachment; filename=\"fsv.csv\"");
  stream->print("number,name,code,raw,value,unit\n");
  for (auto &e : this->fsv_catalog_) {
    auto raw_it = this->fsv_raw_.find(e.code);
    if (raw_it == this->fsv_raw_.end()) {
      stream->printf("%u,%s,0x%04X,,,%s\n", (unsigned) e.number, e.name.c_str(), e.code,
                     e.unit.c_str());
      continue;
    }
    float v = this->fsv_value(e.code);
    char vbuf[16];
    if (std::isnan(v))
      vbuf[0] = '\0';
    else
      snprintf(vbuf, sizeof(vbuf), "%.2f", v);
    stream->printf("%u,%s,0x%04X,%d,%s,%s\n", (unsigned) e.number, e.name.c_str(), e.code,
                   (int) raw_it->second, vbuf, e.unit.c_str());
  }
  request->send(stream);
}

}  // namespace nasa_bridge
}  // namespace esphome
