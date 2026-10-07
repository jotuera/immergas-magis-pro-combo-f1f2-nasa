// Samsung NASA bus hub for ESPHome - MIT (c) 2026 JoTu.
// Receives and validates frames, dispatches message values to registered entities and
// (optionally) polls values that are never broadcast (e.g. FSV installer settings) with READ requests.
#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "nasa_frame.h"

namespace esphome {
namespace immergas_nasa {

class NasaHub;

// Something that wants the value of one message id from one source address.
class NasaListener {
 public:
  void set_message_id(uint16_t id) { this->message_id_ = id; }
  void set_source(uint32_t source) { this->source_ = source; }
  void set_poll(bool poll) { this->poll_ = poll; }
  uint16_t get_message_id() const { return this->message_id_; }
  uint32_t get_source() const { return this->source_; }
  bool get_poll() const { return this->poll_; }
  virtual void on_message(const Packet &packet, const Message &message) = 0;

 protected:
  uint16_t message_id_{0};
  uint32_t source_{0};
  bool poll_{false};
};

// Numeric decoding shared by sensors: sentinel -> NaN, optional high word, sign, scale.
struct ValueDecoder {
  bool is_signed{false};
  bool high_word{false};
  float multiply{1.0f};
  std::vector<uint32_t> nan_values;

  bool decode(const Message &m, float &out) const;
};

class NasaHub : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // --- configuration (from codegen) ---
  void set_address(uint32_t a) { this->address_ = Address::from_raw(a); }
  void set_read_address(uint32_t a) { this->read_address_ = Address::from_raw(a); }
  void set_transmit(bool t) { this->transmit_ = t; }
  void set_quiet_time(uint32_t ms) { this->quiet_time_ = ms; }
  void set_response_timeout(uint32_t ms) { this->response_timeout_ = ms; }
  void set_max_attempts(uint8_t n) { this->max_attempts_ = n; }
  void set_poll_interval(uint32_t ms) { this->poll_interval_ = ms; }
  void set_republish_interval(uint32_t ms) { this->republish_interval_ = ms; }
  void set_flow_control_pin(GPIOPin *pin) { this->flow_control_pin_ = pin; }
  void set_sniffer(bool on) { this->sniffer_ = on; }
  void set_indoor_address(uint32_t a) { this->indoor_address_ = a; }
  void set_outdoor_address(uint32_t a) { this->outdoor_address_ = a; }
  // Label shown in the device list for an address class (translated in codegen).
  void add_class_label(uint8_t cls, const std::string &label) { this->class_labels_[cls] = label; }

  void register_listener(NasaListener *l);

#ifdef USE_SENSOR
  void set_rx_frames_sensor(sensor::Sensor *s) { this->rx_frames_sensor_ = s; }
  void set_crc_errors_sensor(sensor::Sensor *s) { this->crc_errors_sensor_ = s; }
  void set_tx_frames_sensor(sensor::Sensor *s) { this->tx_frames_sensor_ = s; }
  void set_timeouts_sensor(sensor::Sensor *s) { this->timeouts_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_last_frame_text_sensor(text_sensor::TextSensor *s) { this->last_frame_text_ = s; }
  void set_last_change_text_sensor(text_sensor::TextSensor *s) { this->last_change_text_ = s; }
  void set_devices_text_sensor(text_sensor::TextSensor *s) { this->devices_text_ = s; }
#endif

  // --- runtime API ---
  // Queue READ requests (split into frames of <= 10 messages) to the read address.
  void request_read(const std::vector<uint16_t> &ids);
  // Re-read everything registered with poll: true (FSV etc.).
  void poll_now();
  void set_sniffer_enabled(bool on) { this->sniffer_ = on; }
  bool is_sniffer_enabled() const { return this->sniffer_; }
  uint32_t get_republish_interval() const { return this->republish_interval_; }

  // Feed raw bytes as if received from the bus (tests / replay).
  void feed(const uint8_t *data, size_t len);

 protected:
  struct PendingRequest {
    Address dst;
    std::vector<uint16_t> ids;
    uint8_t number{0};
    uint8_t attempts{0};
    uint32_t sent_at{0};
  };

  void process_byte_(uint8_t b);
  bool process_frame_(const uint8_t *buf, size_t len);
  void handle_packet_(const Packet &p);
  void try_transmit_();
  void send_request_(PendingRequest &req);
  uint8_t next_packet_number_();
  void publish_stats_();
  void sniff_(const Packet &p);
  void note_device_(const Address &a);
  std::string device_label_(const Address &a) const;
  void check_addresses_();

  Address address_{ADDR_CLASS_JIG_TESTER, 0xFF, 0x00};
  Address read_address_{0xB2, 0x00, 0x20};
  bool transmit_{true};
  uint32_t quiet_time_{100};
  uint32_t response_timeout_{1000};
  uint8_t max_attempts_{3};
  uint32_t poll_interval_{30 * 60 * 1000};
  uint32_t republish_interval_{60 * 1000};
  GPIOPin *flow_control_pin_{nullptr};
  bool sniffer_{false};

  std::map<uint16_t, std::vector<NasaListener *>> listeners_;
  std::vector<uint16_t> poll_ids_;

  std::vector<uint8_t> rx_;
  uint32_t last_rx_byte_{0};

  std::deque<PendingRequest> queue_;
  bool in_flight_{false};
  PendingRequest current_;
  uint8_t packet_number_{0};
  uint32_t last_poll_{0};
  bool first_poll_done_{false};

  std::map<uint32_t, uint32_t> sniff_last_;  // (source<<16 | id) -> last value

  // devices seen on the bus (address -> frames) for address discovery
  std::map<uint32_t, uint32_t> devices_;
  std::map<uint8_t, std::string> class_labels_;
  uint32_t indoor_address_{0x200001};
  uint32_t outdoor_address_{0x100000};
  bool addresses_checked_{false};

  uint32_t rx_frames_{0}, crc_errors_{0}, tx_frames_{0}, timeouts_{0}, nacks_{0};
  uint32_t last_stats_{0};
#ifdef USE_SENSOR
  sensor::Sensor *rx_frames_sensor_{nullptr};
  sensor::Sensor *crc_errors_sensor_{nullptr};
  sensor::Sensor *tx_frames_sensor_{nullptr};
  sensor::Sensor *timeouts_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *last_frame_text_{nullptr};
  text_sensor::TextSensor *last_change_text_{nullptr};
  text_sensor::TextSensor *devices_text_{nullptr};
#endif
};

}  // namespace immergas_nasa
}  // namespace esphome
