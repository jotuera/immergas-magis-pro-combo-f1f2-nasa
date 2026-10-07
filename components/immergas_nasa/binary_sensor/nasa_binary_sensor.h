#pragma once
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../nasa_hub.h"

namespace esphome {
namespace immergas_nasa {

class NasaBinarySensor : public binary_sensor::BinarySensor, public NasaListener {
 public:
  void set_mask(uint32_t m) { this->mask_ = m; }
  void set_republish_interval(uint32_t ms) { this->republish_ms_ = ms; }
  void on_message(const Packet &packet, const Message &message) override;

 protected:
  uint32_t mask_{0xFFFFFFFF};
  uint32_t republish_ms_{60000};
  uint32_t last_publish_{0};
  bool published_{false};
};

}  // namespace immergas_nasa
}  // namespace esphome
