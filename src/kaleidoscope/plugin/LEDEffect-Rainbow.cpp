/* Kaleidoscope-LEDEffect-Rainbow - Rainbow LED effects for Kaleidoscope.
 * Copyright (C) 2017-2018  Keyboard.io, Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Kaleidoscope-LEDEffect-Rainbow.h"

namespace kaleidoscope {
namespace plugin {

void LEDRainbowEffect::TransientLEDMode::update(void) {
  if (!Runtime.has_leds)
    return;

  if (!Runtime.hasTimeExpired(rainbow_last_update,
                              rainbow_update_delay)) {
    return;
  }

  cRGB rainbow = hsvToRgb360(rainbow_hue, rainbow_saturation, parent_->rainbow_value);
  ::LEDControl.set_all_leds_to(rainbow);

  rainbow_hue = (rainbow_hue + rainbow_steps) % 360;
  rainbow_last_update += rainbow_update_delay;
}

void LEDRainbowEffect::brightness(byte brightness) {
  rainbow_value = brightness;
}


// ---------

void LEDRainbowWaveEffect::TransientLEDMode::update(void) {
  if (!Runtime.has_leds)
    return;

  if (!Runtime.hasTimeExpired(rainbow_last_update,
                              rainbow_update_delay)) {
    return;
  }

  for (auto led_index : Runtime.device().LEDs().all()) {
    uint16_t led_hue = (rainbow_hue + 16 * (led_index.offset() / 4)) % 360;
    cRGB rainbow = hsvToRgb360(led_hue, rainbow_saturation, parent_->rainbow_value);
    ::LEDControl.setCrgbAt(led_index.offset(), rainbow);
  }

  uint8_t hue_steps = max((rainbow_update_delay / 45), 1);
  rainbow_hue = (rainbow_hue + hue_steps) % 360;
  rainbow_last_update += rainbow_update_delay;
}

void LEDRainbowWaveEffect::TransientLEDMode::setTargetFPS(uint8_t fps) {
  rainbow_update_delay = (1000 / fps) * 3;
}

void LEDRainbowWaveEffect::brightness(byte brightness) {
  rainbow_value = brightness;
}

}
}

kaleidoscope::plugin::LEDRainbowEffect LEDRainbowEffect;
kaleidoscope::plugin::LEDRainbowWaveEffect LEDRainbowWaveEffect;
