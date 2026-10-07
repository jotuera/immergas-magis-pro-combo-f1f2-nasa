#include "nasa_binary_sensor.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace immergas_nasa {

void NasaBinarySensor::on_message(const Packet &packet, const Message &message) {
  if (message.kind == MessageKind::STRUCTURE)
    return;
  bool v = (message.value & this->mask_) != 0;
  uint32_t now = millis();
  if (this->published_ && v == this->state && now - this->last_publish_ < this->republish_ms_)
    return;
  this->published_ = true;
  this->last_publish_ = now;
  this->publish_state(v);
}

}  // namespace immergas_nasa
}  // namespace esphome
