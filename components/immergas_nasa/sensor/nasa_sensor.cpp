#include "nasa_sensor.h"
#include "esphome/core/hal.h"
#include <cmath>

namespace esphome {
namespace immergas_nasa {

void NasaSensor::on_message(const Packet &packet, const Message &message) {
  float v;
  if (!this->decoder_.decode(message, v))
    return;
  uint32_t now = millis();
  bool same = this->published_ && ((std::isnan(v) && std::isnan(this->state)) || v == this->state);
  if (same && now - this->last_publish_ < this->republish_ms_)
    return;
  this->published_ = true;
  this->last_publish_ = now;
  this->publish_state(v);
}

}  // namespace immergas_nasa
}  // namespace esphome
