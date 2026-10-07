#pragma once
#include <map>
#include <string>
#include "esphome/components/text_sensor/text_sensor.h"
#include "../nasa_hub.h"

namespace esphome {
namespace immergas_nasa {

enum class TextMode : uint8_t { MAP = 0, FIRMWARE = 1, HEX = 2 };

class NasaTextSensor : public text_sensor::TextSensor, public NasaListener {
 public:
  void set_mode(TextMode m) { this->mode_ = m; }
  void add_option(uint32_t value, const std::string &text) { this->options_[value] = text; }
  // printf format with one %u for values that have no option text
  void set_unknown_format(const std::string &f) { this->unknown_format_ = f; }
  void set_republish_interval(uint32_t ms) { this->republish_ms_ = ms; }
  void on_message(const Packet &packet, const Message &message) override;

 protected:
  std::string format_(const Message &message) const;
  TextMode mode_{TextMode::MAP};
  std::map<uint32_t, std::string> options_;
  std::string unknown_format_{"%u"};
  uint32_t republish_ms_{60000};
  uint32_t last_publish_{0};
  bool published_{false};
};

}  // namespace immergas_nasa
}  // namespace esphome
