#pragma once
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"
#include "../nasa_hub.h"

namespace esphome {
namespace immergas_nasa {

// Turns the bus sniffer (frame + value-change logging) on and off.
class NasaSnifferSwitch : public switch_::Switch, public Parented<NasaHub> {
 protected:
  void write_state(bool state) override {
    this->parent_->set_sniffer_enabled(state);
    this->publish_state(state);
  }
};

}  // namespace immergas_nasa
}  // namespace esphome
