#pragma once
#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"
#include "../nasa_hub.h"

namespace esphome {
namespace immergas_nasa {

// Re-reads every polled value (FSV etc.) now.
class NasaPollButton : public button::Button, public Parented<NasaHub> {
 protected:
  void press_action() override { this->parent_->poll_now(); }
};

}  // namespace immergas_nasa
}  // namespace esphome
