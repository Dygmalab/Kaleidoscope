/* Kaleidoscope-MouseKeys - Mouse keys for Kaleidoscope.
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

#include <Arduino.h>

#include "kaleidoscope/Runtime.h"
#include "Kaleidoscope-MouseKeys.h"
#include "Kaleidoscope-FocusSerial.h"
#include "kaleidoscope/keyswitch_state.h"

namespace kaleidoscope {
namespace plugin {

uint8_t MouseKeys_::mouseMoveIntent;

uint8_t MouseKeys_::speed = 1;
uint16_t MouseKeys_::speedDelay = 1;

uint8_t MouseKeys_::accelSpeed = 1;
uint16_t MouseKeys_::accelDelay = 64;

uint8_t MouseKeys_::wheelSpeed = 1;
uint16_t MouseKeys_::wheelDelay = 50;

uint16_t MouseKeys_::move_start_time_;
uint16_t MouseKeys_::accel_start_time_;
uint16_t MouseKeys_::wheel_start_time_;
uint16_t MouseKeys_::storage_base_;

void MouseKeys_::setWarpGridSize(uint8_t grid_size) {
  MouseWrapper.warp_grid_size = grid_size;
}

void MouseKeys_::setSpeedLimit(uint8_t speed_limit) {
  MouseWrapper.speedLimit = speed_limit;
}

void MouseKeys_::scrollWheel(uint8_t keyCode) {
  if (!Runtime.hasTimeExpired(wheel_start_time_, wheelDelay))
    return;

  wheel_start_time_ = Runtime.millisAtCycleStart();

  if (keyCode & KEY_MOUSE_UP)
    kaleidoscope::Runtime.hid().mouse().move(0, 0, wheelSpeed);
  else if (keyCode & KEY_MOUSE_DOWN)
    kaleidoscope::Runtime.hid().mouse().move(0, 0, -wheelSpeed);
  else if (keyCode & KEY_MOUSE_LEFT)
    kaleidoscope::Runtime.hid().mouse().move(0, 0, 0, -wheelSpeed);
  else if (keyCode & KEY_MOUSE_RIGHT)
    kaleidoscope::Runtime.hid().mouse().move(0, 0, 0, wheelSpeed);
}

EventHandlerResult MouseKeys_::afterEachCycle() {
  kaleidoscope::Runtime.hid().mouse().sendReport();
  kaleidoscope::Runtime.hid().mouse().releaseAllButtons();
  mouseMoveIntent = 0;

  return EventHandlerResult::OK;
}

EventHandlerResult MouseKeys_::beforeReportingState() {
  if (mouseMoveIntent == 0) {
    MouseWrapper.accelStep = 0;
    return EventHandlerResult::OK;
  }

  if (!Runtime.hasTimeExpired(move_start_time_, speedDelay))
    return EventHandlerResult::OK;

  move_start_time_ = Runtime.millisAtCycleStart();

  int8_t moveX = 0, moveY = 0;

  if (Runtime.hasTimeExpired(accel_start_time_, accelDelay)) {
    if (MouseWrapper.accelStep < 255 - accelSpeed) {
      MouseWrapper.accelStep += accelSpeed;
    }
    accel_start_time_ = Runtime.millisAtCycleStart();
  }

  if (mouseMoveIntent & KEY_MOUSE_UP)
    moveY -= speed;
  if (mouseMoveIntent & KEY_MOUSE_DOWN)
    moveY += speed;

  if (mouseMoveIntent & KEY_MOUSE_LEFT)
    moveX -= speed;
  if (mouseMoveIntent & KEY_MOUSE_RIGHT)
    moveX += speed;

  MouseWrapper.move(moveX, moveY);

  return EventHandlerResult::OK;
}

EventHandlerResult MouseKeys_::onKeyswitchEvent(Key &mappedKey, KeyAddr key_addr, uint8_t keyState) {
  if (mappedKey.getFlags() != (SYNTHETIC | IS_MOUSE_KEY))
    return EventHandlerResult::OK;

  if (mappedKey.getKeyCode() & KEY_MOUSE_BUTTON && !(mappedKey.getKeyCode() & KEY_MOUSE_WARP)) {
    uint8_t button = mappedKey.getKeyCode() & ~KEY_MOUSE_BUTTON;

    if (keyIsPressed(keyState)) {
      // Reset warp state on initial mouse button key-down only so we can use
      // warp keys to drag-and-drop:
      if (keyToggledOn(keyState)) {
        MouseWrapper.reset_warping();
      }

      kaleidoscope::Runtime.hid().mouse().pressButtons(button);
    } else if (keyToggledOff(keyState)) {
      kaleidoscope::Runtime.hid().mouse().releaseButtons(button);
      MouseWrapper.end_warping();
    }
  } else if (!(mappedKey.getKeyCode() & KEY_MOUSE_WARP)) {
    if (keyToggledOn(keyState)) {
      move_start_time_ = Runtime.millisAtCycleStart();
      accel_start_time_ = Runtime.millisAtCycleStart();
      wheel_start_time_ = Runtime.millisAtCycleStart() - wheelDelay;
    }
    if (keyIsPressed(keyState)) {
      if (mappedKey.getKeyCode() & KEY_MOUSE_WHEEL) {
        scrollWheel(mappedKey.getKeyCode());
      } else {
        mouseMoveIntent |= mappedKey.getKeyCode();
      }
    } else if (keyToggledOff(keyState)) {
      /* If a mouse key toggles off, we want to explicitly stop moving (or
       * scrolling) in that direction. We want to do this to support use-cases
       * where we send multiple reports per cycle (such as macros), and can't
       * rely on the main loop clearing the report for us. We do not want to
       * clear the whole report either, because we want any other mouse keys
       * to still have their desired effect. Therefore, we selectively stop
       * movement or scrolling. */
      mouseMoveIntent &= ~mappedKey.getKeyCode();
      bool x = false, y = false, vWheel = false, hWheel = false;

      if (mappedKey.getKeyCode() & KEY_MOUSE_UP ||
          mappedKey.getKeyCode() & KEY_MOUSE_DOWN) {
        if (mappedKey.getKeyCode() & KEY_MOUSE_WHEEL) {
          vWheel = true;
        } else {
          y = true;
        }
      } else if (mappedKey.getKeyCode() & KEY_MOUSE_LEFT ||
                 mappedKey.getKeyCode() & KEY_MOUSE_RIGHT) {
        if (mappedKey.getKeyCode() & KEY_MOUSE_WHEEL) {
          hWheel = true;
        } else {
          x = true;
        }
      }

      kaleidoscope::Runtime.hid().mouse().stop(x, y, vWheel, hWheel);
    }
  } else if (keyToggledOn(keyState)) {
    if (mappedKey.getKeyCode() & KEY_MOUSE_WARP && mappedKey.getFlags() & IS_MOUSE_KEY) {
      MouseWrapper.warp(((mappedKey.getKeyCode() & KEY_MOUSE_WARP_END) ? WARP_END : 0x00) |
                        ((mappedKey.getKeyCode() & KEY_MOUSE_UP) ? WARP_UP : 0x00) |
                        ((mappedKey.getKeyCode() & KEY_MOUSE_DOWN) ? WARP_DOWN : 0x00) |
                        ((mappedKey.getKeyCode() & KEY_MOUSE_LEFT) ? WARP_LEFT : 0x00) |
                        ((mappedKey.getKeyCode() & KEY_MOUSE_RIGHT) ? WARP_RIGHT : 0x00));
    }
  }

  return EventHandlerResult::EVENT_CONSUMED;
}

EventHandlerResult MouseKeys_::onFocusEvent(const char *command)
{
  if (::Focus.handleHelp(command, PSTR("mouse.speed\nmouse.speedDelay\nmouse.accelSpeed\nmouse.accelDelay\nmouse.wheelSpeed\nmouse.wheelDelay\nmouse.speedLimit")))
    return EventHandlerResult::OK;

  if (strncmp_P(command, PSTR("mouse."), 6) != 0)
    return EventHandlerResult::OK;

  if (strcmp_P(command + 6, PSTR("speed")) == 0)
  {
    if (::Focus.isEOL())
    {
      ::Focus.send(speed);
    }
    else
    {
      uint8_t auxspeed;
      ::Focus.read(auxspeed);
      speed = auxspeed;

      Runtime.storage().update(storage_base_ + 0, auxspeed);
      Runtime.storage().commit();
    }
  }

  if (strcmp_P(command + 6, PSTR("speedDelay")) == 0)
  {
    if (::Focus.isEOL())
    {
      ::Focus.send(speedDelay);
    }
    else
    {
      uint16_t auxspeedDelay = 0;
      uint8_t a{0};
      uint8_t b{0};
      ::Focus.read(a);
      while (!::Focus.isEOL())
      {
        ::Focus.read(b);
      }
      auxspeedDelay = ((b << 8) | a);
      speedDelay = auxspeedDelay;

      Runtime.storage().update(storage_base_ + 1, a);
      Runtime.storage().update(storage_base_ + 2, b);
      Runtime.storage().commit();
    }
  }

  if (strcmp_P(command + 6, PSTR("accelSpeed")) == 0)
  {
    if (::Focus.isEOL())
    {
      ::Focus.send(accelSpeed);
    }
    else
    {
      uint8_t auxaccelSpeed;
      ::Focus.read(auxaccelSpeed);
      accelSpeed = auxaccelSpeed;

      Runtime.storage().update(storage_base_ + 3, auxaccelSpeed);
      Runtime.storage().commit();
    }
  }

  if (strcmp_P(command + 6, PSTR("accelDelay")) == 0)
  {
    if (::Focus.isEOL())
    {
      ::Focus.send(accelDelay);
    }
    else
    {
      uint16_t auxaccelDelay = 0;
      uint8_t a{0};
      uint8_t b{0};
      ::Focus.read(a);
      while (!::Focus.isEOL())
      {
        ::Focus.read(b);
      }
      auxaccelDelay = ((b << 8) | a);
      accelDelay = auxaccelDelay;

      Runtime.storage().update(storage_base_ + 4, a);
      Runtime.storage().update(storage_base_ + 5, b);
      Runtime.storage().commit();
    }
  }

  if (strcmp_P(command + 6, PSTR("wheelSpeed")) == 0)
  {
    if (::Focus.isEOL())
    {
      ::Focus.send(wheelSpeed);
    }
    else
    {
      uint8_t auxwheelSpeed;
      ::Focus.read(auxwheelSpeed);
      wheelSpeed = auxwheelSpeed;

      Runtime.storage().update(storage_base_ + 6, auxwheelSpeed);
      Runtime.storage().commit();
    }
  }

  if (strcmp_P(command + 6, PSTR("wheelDelay")) == 0)
  {
    if (::Focus.isEOL())
    {
      ::Focus.send(wheelDelay);
    }
    else
    {
      uint16_t auxwheelDelay = 0;
      uint8_t a{0};
      uint8_t b{0};
      ::Focus.read(a);
      while (!::Focus.isEOL())
      {
        ::Focus.read(b);
      }
      auxwheelDelay = ((b << 8) | a);
      wheelDelay = auxwheelDelay;

      Runtime.storage().update(storage_base_ + 7, a);
      Runtime.storage().update(storage_base_ + 8, b);
      Runtime.storage().commit();
    }
  }

  if (strcmp_P(command + 6, PSTR("speedLimit")) == 0)
  {
    if (::Focus.isEOL())
    {
      ::Focus.send(MouseWrapper.speedLimit);
    }
    else
    {
      uint8_t auxspeedLimit;
      ::Focus.read(auxspeedLimit);
      setSpeedLimit(auxspeedLimit);

      Runtime.storage().update(storage_base_ + 9, auxspeedLimit);
      Runtime.storage().commit();
    }
  }

  return EventHandlerResult::EVENT_CONSUMED;
}

EventHandlerResult MouseKeys_::onSetup(void) {
  kaleidoscope::Runtime.hid().mouse().setup();
  kaleidoscope::Runtime.hid().absoluteMouse().setup();

  uint8_t size = 10 * sizeof(uint8_t);
  MouseKeys_::storage_base_ = ::EEPROMSettings.requestSlice(size);
  uint8_t spd;
  uint16_t spdDelay;
  uint8_t accspd;
  uint16_t accDelay;
  uint8_t wheelspd;
  uint16_t wheelDlay;
  uint8_t spdLimit;

  Runtime.storage().get(storage_base_, spd);
  if(spd < 255){
    MouseKeys_::speed = spd;
  }else{
    Runtime.storage().update(storage_base_, speed);
  }

  Runtime.storage().get(storage_base_ + 1, spdDelay);
  if(spdDelay < 65535){
    speedDelay = spdDelay;
  }else{
    Runtime.storage().update(storage_base_ + 1, speedDelay);
  }

  Runtime.storage().get(storage_base_ + 3, accspd);
  if(accspd < 255){
    accelSpeed = accspd;
  }else{
    Runtime.storage().update(storage_base_ + 3, accelSpeed);
  }

  Runtime.storage().get(storage_base_ + 4, accDelay);
  if(accDelay < 65535){
    accelDelay = accDelay;
  }else{
    Runtime.storage().update(storage_base_ + 4, accelDelay);
  }

    Runtime.storage().get(storage_base_ + 6, wheelspd);
  if(wheelspd < 255){
    wheelSpeed = wheelspd;
  }else{
    Runtime.storage().update(storage_base_ + 6, wheelSpeed);
  }

  Runtime.storage().get(storage_base_ + 7, wheelDlay);
  if(wheelDlay != 65535){
    wheelDelay = wheelDlay;
  }else{
    Runtime.storage().update(storage_base_ + 7, wheelDelay);
  }

  Runtime.storage().get(storage_base_ + 9, spdLimit);
  if(spd < 255){
    setSpeedLimit(spdLimit);
  }else{
    Runtime.storage().update(storage_base_ + 9, MouseWrapper.speedLimit);
  }

  return EventHandlerResult::OK;
}

}
}

kaleidoscope::plugin::MouseKeys_ MouseKeys;
