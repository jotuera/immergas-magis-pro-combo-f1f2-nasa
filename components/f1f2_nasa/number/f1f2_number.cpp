#include "f1f2_number.h"
#include <cmath>

namespace esphome {
namespace f1f2_nasa {

void NasaNumber::on_message(const Packet &packet, const Message &message) {
  float v;
  if (!this->decoder_.decode(message, v))
    return;
  if (std::isnan(v))
    return;
  this->publish_state(v);
}

void NasaNumber::control(float value) {
  if (this->hub_ == nullptr)
    return;
  float mul = this->decoder_.multiply != 0.0f ? this->decoder_.multiply : 1.0f;
  int32_t raw = static_cast<int32_t>(lroundf(value / mul));  // inverse of the read scale
  this->hub_->write_message(this->get_message_id(), static_cast<uint32_t>(raw));
  this->publish_state(value);  // optimistic; the real value is confirmed on the next read
}

}  // namespace f1f2_nasa
}  // namespace esphome
