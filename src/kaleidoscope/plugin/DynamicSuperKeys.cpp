/* DynamicSuperKeys - Dynamic macro support for Kaleidoscope.
 * Copyright (C) 2019  Keyboard.io, Inc.
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

#include "Kaleidoscope-DynamicSuperKeys.h"
#include "Kaleidoscope-DynamicMacros.h"
#include "Kaleidoscope-FocusSerial.h"
#include <Kaleidoscope-EEPROM-Settings.h>
#include "kaleidoscope/keyswitch_state.h"
#include "kaleidoscope/layers.h"

namespace kaleidoscope
{
  namespace plugin
  {

    // --- state ---
    uint16_t DynamicSuperKeys::storage_base_;
    uint16_t DynamicSuperKeys::storage_size_;
    DynamicSuperKeys::SuperKeyState DynamicSuperKeys::state_[DynamicSuperKeys::SUPER_KEY_COUNT];
    uint16_t DynamicSuperKeys::map_[];
    uint8_t DynamicSuperKeys::offset_;
    uint8_t DynamicSuperKeys::super_key_count_;
    constexpr uint8_t DynamicSuperKeys::SUPER_KEY_COUNT;
    uint16_t DynamicSuperKeys::start_time_;
    uint16_t DynamicSuperKeys::last_start_time_;
    uint16_t DynamicSuperKeys::delayed_time_;
    uint16_t DynamicSuperKeys::wait_for_ = 500;
    uint16_t DynamicSuperKeys::hold_start_ = 200;
    uint8_t DynamicSuperKeys::repeat_interval_ = 20;
    uint8_t DynamicSuperKeys::overlap_threshold_ = 20;
    uint16_t DynamicSuperKeys::time_out_ = 250;
    Key DynamicSuperKeys::last_super_key_ = Key_NoKey;
    KeyAddr DynamicSuperKeys::last_super_addr_;
    bool DynamicSuperKeys::modifier_pressed_ = false;
    bool DynamicSuperKeys::layer_shifted_ = false;
    uint8_t DynamicSuperKeys::layer_shifted_number_ = 0;

    void DynamicSuperKeys::updateDynamicSuperKeysCache()
    {
      uint16_t pos = storage_base_ + 8;
      uint8_t current_id = 0;
      bool previous_super_key_ended = false;

      super_key_count_ = 0;
      map_[0] = 0;

      uint16_t wait_for;
      uint16_t time_out;
      uint16_t hold_start;
      uint8_t repeat_interval;
      uint8_t overlap_threshold;

      Kaleidoscope.storage().get(storage_base_ + 0, wait_for);
      if (wait_for < 2000)
      {
        DynamicSuperKeys::wait_for_ = wait_for;
      }
      else
      {
        Kaleidoscope.storage().update(storage_base_, DynamicSuperKeys::wait_for_);
      }
      Kaleidoscope.storage().get(storage_base_ + 2, time_out);
      if (time_out != 65535)
      {
        DynamicSuperKeys::time_out_ = time_out;
      }
      else
      {
        Kaleidoscope.storage().update(storage_base_, DynamicSuperKeys::time_out_);
      }
      Kaleidoscope.storage().get(storage_base_ + 4, hold_start);
      if (hold_start != 65535)
      {
        DynamicSuperKeys::hold_start_ = hold_start;
      }
      else
      {
        Kaleidoscope.storage().update(storage_base_, DynamicSuperKeys::hold_start_);
      }
      Kaleidoscope.storage().get(storage_base_ + 6, repeat_interval);
      if (repeat_interval < 251)
      {
        DynamicSuperKeys::repeat_interval_ = repeat_interval;
      }
      else
      {
        Kaleidoscope.storage().update(storage_base_, DynamicSuperKeys::repeat_interval_);
      }
      Kaleidoscope.storage().get(storage_base_ + 7, overlap_threshold);
      if (overlap_threshold < 251)
      {
        DynamicSuperKeys::overlap_threshold_ = overlap_threshold;
      }
      else
      {
        Kaleidoscope.storage().update(storage_base_, DynamicSuperKeys::overlap_threshold_);
      }

      while (pos < (storage_base_ + 8) + storage_size_)
      {
        uint16_t raw_key = Kaleidoscope.storage().read(pos);
        pos += 2;
        Key key(raw_key);

        if (key == Key_NoKey)
        {
          state_[current_id].printonrelease = (pos - storage_base_ - 8 - map_[current_id]) == 6;
          map_[++current_id] = pos - storage_base_ - 8;
          if (previous_super_key_ended)
            return;

          super_key_count_++;
          previous_super_key_ended = true;
        }
        else
        {
          previous_super_key_ended = false;
        }
      }
    }

    DynamicSuperKeys::SuperType DynamicSuperKeys::ReturnType(DynamicSuperKeys::SuperType previous, DynamicSuperKeys::ActionType action)
    {
      DynamicSuperKeys::SuperType result;
      if (action == Tap)
      {
        switch (previous)
        {
        case DynamicSuperKeys::None:
          result = DynamicSuperKeys::Tap_Once;
          break;
        case DynamicSuperKeys::Tap_Once:
          result = DynamicSuperKeys::Tap_Twice;
          break;
        case DynamicSuperKeys::Tap_Twice:
          result = DynamicSuperKeys::Tap_Trice;
          break;
        default:
          result = DynamicSuperKeys::Tap_Trice;
        }
      }
      if (action == Hold)
      {
        switch (previous)
        {
        case DynamicSuperKeys::None:
          result = DynamicSuperKeys::None;
          break;
        case DynamicSuperKeys::Tap_Once:
          result = DynamicSuperKeys::Hold_Once;
          break;
        case DynamicSuperKeys::Tap_Twice:
          result = DynamicSuperKeys::Tap_Hold;
          break;
        case DynamicSuperKeys::Tap_Trice:
          result = DynamicSuperKeys::Tap_Twice_Hold;
          break;
        default:
          result = DynamicSuperKeys::Tap_Twice_Hold;
        }
      }
      return result;
    }

    // --- actions ---

    bool DynamicSuperKeys::interrupt(KeyAddr key_addr)
    {
      uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      if (state_[idx].pressed)
      {
        // if ((Runtime.hasTimeExpired(start_time_, hold_start_) && releaseDelayed(last_start_time_, start_time_)) || state_[idx].printonrelease)
        if ((Runtime.hasTimeExpired(start_time_, hold_start_)))
        {
          hold();
          // kaleidoscope::Runtime.hid().keyboard().sendReport();
          return false;
        }
        // releaseDelayed(start_time, Runtime.millisAtCycleStart())
        // if(Runtime.hasTimeExpired(start_time_, 100)){
        //   hold();
        //   return false;
        // } else {
        //   SuperKeys(idx, last_super_addr_, state_[idx].count, Interrupt);
        //   state_[idx].triggered = true;
        //   return false;
        // }
        // } else {
      }
      last_super_key_ = Key_NoKey;
      if (!state_[idx].holded)
      {
        SuperKeys(idx, last_super_addr_, state_[idx].count, Interrupt);
      }
      // SuperKeys(idx, last_super_addr_, state_[idx].count, Release);
      start_time_ = 0;
      state_[idx].pressed = false;
      state_[idx].triggered = false;
      state_[idx].holded = false;
      state_[idx].count = None;
      state_[idx].release_next = false;
      // }
    }

    void DynamicSuperKeys::timeout(void)
    {
      uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      if (state_[idx].pressed)
      {
        return;
      }
      last_super_key_ = Key_NoKey;
      if (!state_[idx].holded)
      {
        SuperKeys(idx, last_super_addr_, state_[idx].count, Timeout);
      }
      start_time_ = 0;
      state_[idx].pressed = false;
      state_[idx].triggered = false;
      state_[idx].holded = false;
      state_[idx].count = None;
      state_[idx].release_next = false;
    }

    void DynamicSuperKeys::release(uint8_t super_key_index)
    {
      uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;
      SuperKeys(idx, last_super_addr_, state_[idx].count, Release);
      state_[idx].pressed = false;
      state_[idx].triggered = false;
      state_[idx].holded = false;
      state_[idx].count = None;
      state_[idx].release_next = false;
      start_time_ = 0;
      last_super_key_ = Key_NoKey;
    }

    void DynamicSuperKeys::tap(void)
    {
      uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      state_[idx].count = DynamicSuperKeys::ReturnType(state_[idx].count, Tap);
      start_time_ = Runtime.millisAtCycleStart();

      // SuperKeys(idx, last_super_addr_, state_[idx].count, Tap);
    }

    void DynamicSuperKeys::hold(void)
    {
      uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      if (state_[idx].holded)
      {
        SuperKeys(idx, last_super_addr_, state_[idx].count, Hold);
      }
      else
      {
        delayed_time_ = 0;
        state_[idx].holded = true;
        state_[idx].triggered = true;
        state_[idx].count = DynamicSuperKeys::ReturnType(state_[idx].count, Hold);
        SuperKeys(idx, last_super_addr_, state_[idx].count, Hold);
        delayed_time_ = Runtime.millisAtCycleStart();
      }
    }

    // --- api ---

    bool DynamicSuperKeys::SuperKeys(uint8_t super_key_index, KeyAddr key_addr,
                                     DynamicSuperKeys::SuperType tap_count, DynamicSuperKeys::ActionType super_key_action)
    {
      DynamicSuperKeys::SuperType corrected = tap_count;
      if (corrected == DynamicSuperKeys::Tap_Trice)
        corrected = DynamicSuperKeys::Tap_Twice;
      uint16_t pos = map_[super_key_index - offset_] + ((corrected - 1) * 2);
      uint16_t next_pos = map_[super_key_index - offset_ + 1];
      if (next_pos <= pos || (super_key_index > offset_ + super_key_count_))
        return false;

      Key key;
      Kaleidoscope.storage().get(storage_base_ + pos + 8, key);

      switch (super_key_action)
      {
      case DynamicSuperKeys::Tap:
        break;
      case DynamicSuperKeys::Interrupt:
      case DynamicSuperKeys::Timeout:
        if (key.getRaw() == 1)
        {
          if(tap_count == DynamicSuperKeys::Tap_Twice) {
            Key key2;
            uint16_t pos2 = map_[super_key_index - offset_];
            Kaleidoscope.storage().get(storage_base_ + pos2 + 8, key2);
            handleKeyswitchEvent(key2, key_addr, IS_PRESSED | INJECTED);
            kaleidoscope::Runtime.hid().keyboard().sendReport();
            handleKeyswitchEvent(key2, key_addr, WAS_PRESSED | INJECTED);
            kaleidoscope::Runtime.hid().keyboard().sendReport();
            handleKeyswitchEvent(key2, key_addr, IS_PRESSED | INJECTED);
            kaleidoscope::Runtime.hid().keyboard().sendReport();
            handleKeyswitchEvent(key2, key_addr, WAS_PRESSED | INJECTED);
          }
          break;
        }
        if (key.getRaw() >= 17492 && key.getRaw() <= 17501)
        {
          ::Layer.move(key.getKeyCode() - LAYER_MOVE_OFFSET);
          break;
        }
        if (key.getRaw() >= ranges::DYNAMIC_MACRO_FIRST && key.getRaw() <= ranges::DYNAMIC_MACRO_LAST)
        {
          ::DynamicMacros.play(key.getRaw() - ranges::DYNAMIC_MACRO_FIRST);
          break;
        }
        if (key.getRaw() >= 256 && key.getRaw() <= 7935)
        {
          uint8_t modif = (key.getRaw() & 0xFF00) >> 8;
          if (modif & 0x01)
          {
            handleKeyswitchEvent(Key_LeftControl, key_addr, IS_PRESSED | INJECTED);
          }
          if (modif & 0x02)
          {
            handleKeyswitchEvent(Key_LeftAlt, key_addr, IS_PRESSED | INJECTED);
          }
          if (modif & 0x04)
          {
            handleKeyswitchEvent(Key_RightAlt, key_addr, IS_PRESSED | INJECTED);
          }
          if (modif & 0x08)
          {
            handleKeyswitchEvent(Key_LeftShift, key_addr, IS_PRESSED | INJECTED);
          }
          if (modif & 0x10)
          {
            handleKeyswitchEvent(Key_LeftGui, key_addr, IS_PRESSED | INJECTED);
          }
          handleKeyswitchEvent(key, key_addr, IS_PRESSED | INJECTED);
          // Runtime.hid().keyboard().pressKey(key, false);
          // kaleidoscope::Runtime.hid().keyboard().sendReport();
          // kaleidoscope::Runtime.hid().keyboard().releaseAllKeys();
          break;
        }
        handleKeyswitchEvent(key, key_addr, IS_PRESSED | INJECTED);
        // Runtime.hid().keyboard().pressKey(key, false);
        kaleidoscope::Runtime.hid().keyboard().sendReport();
        break;
      case DynamicSuperKeys::Hold:
        if (delayed_time_ == 0)
        {
          if (key.getRaw() >= 17492 && key.getRaw() <= 17501)
          {
            ::Layer.move(key.getKeyCode() - LAYER_MOVE_OFFSET);
            break;
          }
          if (key.getRaw() >= 17450 && key.getRaw() <= 17459)
          {
            layer_shifted_ = true;
            layer_shifted_number_ = key.getKeyCode() - LAYER_SHIFT_OFFSET;
            // ::Layer.activate(key.getKeyCode() - LAYER_SHIFT_OFFSET);
            handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
            break;
          }
          if (key.getRaw() >= ranges::DYNAMIC_MACRO_FIRST && key.getRaw() <= ranges::DYNAMIC_MACRO_LAST)
          {
            ::DynamicMacros.play(key.getRaw() - ranges::DYNAMIC_MACRO_FIRST);
            break;
          }
          if (key.getRaw() >= 256 && key.getRaw() <= 7935)
          {
            uint8_t modif = (key.getRaw() & 0xFF00) >> 8;
            if (modif & 0x01)
            {
              handleKeyswitchEvent(Key_LeftControl, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
            }
            if (modif & 0x02)
            {
              handleKeyswitchEvent(Key_LeftAlt, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
            }
            if (modif & 0x04)
            {
              handleKeyswitchEvent(Key_RightAlt, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
            }
            if (modif & 0x08)
            {
              handleKeyswitchEvent(Key_LeftShift, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
            }
            if (modif & 0x10)
            {
              handleKeyswitchEvent(Key_LeftGui, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
            }
            // handleKeyswitchEvent(mod, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
            handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
            // kaleidoscope::Runtime.hid().keyboard().sendReport();
            // kaleidoscope::Runtime.hid().keyboard().releaseAllKeys();
            break;
          }
          handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
        }
        else
        {
          if (Runtime.hasTimeExpired(delayed_time_, wait_for_))
          {
            // if (key.getRaw() >= ranges::DYNAMIC_MACRO_FIRST && key.getRaw() <= ranges::DYNAMIC_MACRO_LAST)
            // {
            //   delay(repeat_interval_);
            //   ::DynamicMacros.play(key.getRaw() - ranges::DYNAMIC_MACRO_FIRST);
            //   break;
            // }
            if (key.getRaw() >= 17450 && key.getRaw() <= 17459)
            {
              break;
            }
            if (key.getRaw() == 23785 || key.getRaw() == 23786)
            {
              delay(repeat_interval_);
              kaleidoscope::Runtime.hid().keyboard().sendReport();
            }
            if (key.getRaw() >= 256 && key.getRaw() <= 7935)
            {
              kaleidoscope::Runtime.hid().keyboard().sendReport();
              break;
            }
          }
          handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
        }
        break;
      case DynamicSuperKeys::Release:
        if (key.getRaw() == 1)
        {
          // if(tap_count == DynamicSuperKeys::Tap_Twice) {
          //   Key key2;
          //   uint16_t pos2 = map_[super_key_index - offset_];
          //   Kaleidoscope.storage().get(storage_base_ + pos2 + 8, key2);
          //   handleKeyswitchEvent(key2, key_addr, IS_PRESSED | INJECTED);
          //   kaleidoscope::Runtime.hid().keyboard().sendReport();
          //   handleKeyswitchEvent(key2, key_addr, WAS_PRESSED | INJECTED);
          //   kaleidoscope::Runtime.hid().keyboard().sendReport();
          //   handleKeyswitchEvent(key2, key_addr, IS_PRESSED | INJECTED);
          //   kaleidoscope::Runtime.hid().keyboard().sendReport();
          //   handleKeyswitchEvent(key2, key_addr, WAS_PRESSED | INJECTED);
          // }
          break;
        }
        if (key.getRaw() >= ranges::DYNAMIC_MACRO_FIRST && key.getRaw() <= ranges::DYNAMIC_MACRO_LAST)
        {
          break;
        }
        if (key.getRaw() >= 17450 && key.getRaw() <= 17459)
        {
          ::Layer.deactivate(key.getKeyCode() - LAYER_SHIFT_OFFSET);
          layer_shifted_ = false;
          break;
        }
        if (key.getRaw() >= 256 && key.getRaw() <= 7935)
        {
          // handleKeyswitchEvent(mod, key_addr, WAS_PRESSED | INJECTED);
          handleKeyswitchEvent(key, key_addr, WAS_PRESSED | INJECTED);
          // kaleidoscope::Runtime.hid().keyboard().sendReport();
          // kaleidoscope::Runtime.hid().keyboard().releaseAllKeys();
          uint8_t modif = (key.getRaw() & 0xFF00) >> 8;
          if (modif & 0x01)
          {
            handleKeyswitchEvent(Key_LeftControl, key_addr, WAS_PRESSED | INJECTED);
          }
          if (modif & 0x02)
          {
            handleKeyswitchEvent(Key_LeftAlt, key_addr, WAS_PRESSED | INJECTED);
          }
          if (modif & 0x04)
          {
            handleKeyswitchEvent(Key_RightAlt, key_addr, WAS_PRESSED | INJECTED);
          }
          if (modif & 0x08)
          {
            handleKeyswitchEvent(Key_LeftShift, key_addr, WAS_PRESSED | INJECTED);
          }
          if (modif & 0x10)
          {
            handleKeyswitchEvent(Key_LeftGui, key_addr, WAS_PRESSED | INJECTED);
          }
          break;
        }

        kaleidoscope::Runtime.hid().keyboard().sendReport();
        handleKeyswitchEvent(key, key_addr, WAS_PRESSED | INJECTED);
        break;
      }

      return true;
    }

    // --- hooks ---

    EventHandlerResult DynamicSuperKeys::onKeyswitchEvent(Key &mapped_key, KeyAddr key_addr, uint8_t keyState)
    {
      // If k is not a physical key, ignore it; some other plugin injected it.
      if (keyState & INJECTED)
      {
        return EventHandlerResult::OK;
      }

      // If it's not a superkey press, we treat it here
      if (mapped_key.getRaw() < ranges::DYNAMIC_SUPER_FIRST || mapped_key.getRaw() > ranges::DYNAMIC_SUPER_LAST)
      {
        // We detect any previously pressed modifiers to be able to release the configured tap or held key when pressed
        if (mapped_key.getRaw() <= Key_RightGui.getRaw() && mapped_key.getRaw() >= Key_LeftControl.getRaw())
        {
          if (keyToggledOff(keyState))
          {
            modifier_pressed_ = false;
          }
          if (keyToggledOn(keyState))
          {
            modifier_pressed_ = true;
          }
        }

        // This is the way out when no superkey was pressed before this one.
        if (last_super_key_ == Key_NoKey)
        {
          last_start_time_ = Runtime.millisAtCycleStart();
          return EventHandlerResult::OK;
        }

        // This only executes if there was a previous superkey pressed and stored in last_super_key
        if (keyToggledOn(keyState))
        {
          // if (layer_shifted_)
          // {
          //   // mapped_key == keyFromKeymap(layer_shifted_number_, key_addr);
          //   return EventHandlerResult::EVENT_CONSUMED;
          // }
          if (interrupt(key_addr))
          {
            mapped_key = Key_NoKey;
          }
          return EventHandlerResult::OK;
          // A press of a foreign key interrupts the stacking of superkey taps, thus making the key collapse, depending on the time spent on it,
          // it will turn into it's corresponding hold, or remain as a tap if (interrupt(key_addr)) mapped_key = Key_NoKey;
        }
        // if (layer_shifted_)
        // {
        //   // uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;
        //   // if (state_[idx].printonrelease)
        //   // {
        //   mapped_key = keyFromKeymap(layer_shifted_number_, key_addr);
        //   // mapped_key = ::Layers.getKeyFromPROGMEM(layer_shifted_number_, key_addr);
        //   // }
        // }
        return EventHandlerResult::OK;
      }

      // get the superkey index of the received superkey
      uint8_t super_key_index = mapped_key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      // First stte if the key tht ws triggered, ws pressed or not
      if (keyToggledOff(keyState))
      {
        state_[super_key_index].pressed = false;
      }
      if (keyToggledOn(keyState))
      {
        state_[super_key_index].pressed = true;
      }

      //  This is the point of entry to the function, when pressing the superkey for the fist time of each run.
      if (last_super_key_ == Key_NoKey)
      {
        // If the key is released, this shouldn't happen, so leave it as it is.
        if (keyToggledOff(keyState))
          return EventHandlerResult::EVENT_CONSUMED;
        if (keyToggledOn(keyState))
        {
          // If the key is just pressed, activate the tap function and save the superkey
          last_super_key_ = mapped_key;
          last_super_addr_ = key_addr;

          tap();
        }
        return EventHandlerResult::EVENT_CONSUMED;
      }

      // This IF block treats the behaviour for the case in witch a different superkey is pressed after the previous one.
      if (last_super_key_ != mapped_key)
      {
        if (keyToggledOff(keyState))
        {
          release(super_key_index);
          return EventHandlerResult::EVENT_CONSUMED;
        }

        if (!keyToggledOn(keyState))
        {
          interrupt(key_addr);
          last_super_key_ = mapped_key;
          last_super_addr_ = key_addr;

          tap();
          return EventHandlerResult::EVENT_CONSUMED;
        }
        return EventHandlerResult::EVENT_CONSUMED;
      }

      if (last_super_key_ == mapped_key)
      {
        if (keyToggledOn(keyState))
        {
          tap();
          return EventHandlerResult::EVENT_CONSUMED;
        }
        if (keyToggledOff(keyState))
        {
          // if (layer_shifted_ == true || state_[super_key_index].triggered)
          // {
          //   release(super_key_index);
          //   return EventHandlerResult::EVENT_CONSUMED;
          // }
          // if the printonrelease flag is true, release the key
          if (state_[super_key_index].printonrelease || modifier_pressed_)
          {
            interrupt(key_addr);
            release(super_key_index);
            return EventHandlerResult::EVENT_CONSUMED;
          }
          // if it's already triggered (so it emmited) release, if not, wait
          return EventHandlerResult::EVENT_CONSUMED;
        }
        // if (Runtime.hasTimeExpired(start_time_, hold_start_) && releaseDelayed(last_start_time_, start_time_))
        if (Runtime.hasTimeExpired(start_time_, hold_start_))
        {
          hold();
          return EventHandlerResult::EVENT_CONSUMED;
        }
        return EventHandlerResult::EVENT_CONSUMED;
      }

      return EventHandlerResult::EVENT_CONSUMED;
    }

    EventHandlerResult DynamicSuperKeys::beforeReportingState()
    {
      if (last_super_key_ == Key_NoKey)
        return EventHandlerResult::OK;

      uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      // if (state_[idx].release_next)
      // {
      //   state_[idx].pressed = false;
      //   state_[idx].triggered = false;
      //   state_[idx].holded = false;
      //   state_[idx].release_next = false;
      //   state_[idx].count = None;
      //   last_super_key_ = Key_NoKey;
      //   return EventHandlerResult::OK;
      // }

      if (start_time_ > 0 && Runtime.hasTimeExpired(start_time_, time_out_))
        timeout();
      return EventHandlerResult::OK;
    }

    // Return true if the release of the super key still needs to be delayed due to
    // rollover. This is called when a super key is released before a subsequent key,
    // and that key is still being held. It checks to see if the subsequent key has
    // been held long enough that the super key should be flushed in its primary state
    // (in which case we return `false`).
    bool DynamicSuperKeys::releaseDelayed(uint16_t overlap_start,
                                          uint16_t overlap_end)
    {
      // We want to calculate the timeout by dividing the overlap duration by the
      // percentage required to make the super key take on its alternate state. Since
      // we're doing integer arithmetic, we need to first multiply by 100, then
      // divide by the percentage value (as an integer). We use 32-bit integers
      // here to make sure it doesn't overflow when we multiply by 100.
      uint32_t overlap_duration = overlap_end - overlap_start;
      uint32_t release_timeout = (overlap_duration * 100) / overlap_threshold_;
      return Runtime.hasTimeExpired(overlap_start, uint16_t(release_timeout));
    }

    EventHandlerResult DynamicSuperKeys::onFocusEvent(const char *command)
    {
      if (::Focus.handleHelp(command, PSTR("superkeys.map\nsuperkeys.waitfor\nsuperkeys.timeout\nsuperkeys.repeat\nsuperkeys.holdstart\nsuperkeys.overlap")))
        return EventHandlerResult::OK;

      if (strncmp_P(command, PSTR("superkeys."), 10) != 0)
        return EventHandlerResult::OK;

      if (strcmp_P(command + 10, PSTR("map")) == 0)
      {
        if (::Focus.isEOL())
        {
          for (uint16_t i = 0; i < storage_size_; i += 2)
          {
            Key k;
            Kaleidoscope.storage().get(storage_base_ + i + 8, k);
            ::Focus.send(k);
          }
        }
        else
        {
          uint16_t pos = 0;

          while (!::Focus.isEOL())
          {
            Key k;
            ::Focus.read(k);

            Kaleidoscope.storage().put(storage_base_ + pos + 8, k);
            pos += 2;
          }
          Kaleidoscope.storage().commit();
          updateDynamicSuperKeysCache();
        }
      }
      if (strcmp_P(command + 10, PSTR("waitfor")) == 0)
      {
        if (::Focus.isEOL())
        {
          ::Focus.send(DynamicSuperKeys::wait_for_);
        }
        else
        {
          uint16_t wait = 0;
          ::Focus.read(wait);
          Kaleidoscope.storage().put(storage_base_ + 0, wait);
          Kaleidoscope.storage().commit();
          updateDynamicSuperKeysCache();
        }
      }
      if (strcmp_P(command + 10, PSTR("timeout")) == 0)
      {
        if (::Focus.isEOL())
        {
          ::Focus.send(DynamicSuperKeys::time_out_);
        }
        else
        {
          uint16_t time = 0;
          ::Focus.read(time);
          Kaleidoscope.storage().put(storage_base_ + 2, time);
          Kaleidoscope.storage().commit();
          updateDynamicSuperKeysCache();
        }
      }
      if (strcmp_P(command + 10, PSTR("holdstart")) == 0)
      {
        if (::Focus.isEOL())
        {
          ::Focus.send(DynamicSuperKeys::hold_start_);
        }
        else
        {
          uint16_t hold = 0;
          ::Focus.read(hold);
          Kaleidoscope.storage().put(storage_base_ + 4, hold);
          Kaleidoscope.storage().commit();
          updateDynamicSuperKeysCache();
        }
      }
      if (strcmp_P(command + 10, PSTR("repeat")) == 0)
      {
        if (::Focus.isEOL())
        {
          ::Focus.send(DynamicSuperKeys::repeat_interval_);
        }
        else
        {
          uint8_t repeat = 0;
          ::Focus.read(repeat);
          Kaleidoscope.storage().put(storage_base_ + 6, repeat);
          Kaleidoscope.storage().commit();
          updateDynamicSuperKeysCache();
        }
      }
      if (strcmp_P(command + 10, PSTR("overlap")) == 0)
      {
        if (::Focus.isEOL())
        {
          ::Focus.send(DynamicSuperKeys::overlap_threshold_);
        }
        else
        {
          uint8_t overlap = 0;
          ::Focus.read(overlap);
          Kaleidoscope.storage().put(storage_base_ + 7, overlap);
          Kaleidoscope.storage().commit();
          updateDynamicSuperKeysCache();
        }
      }

      return EventHandlerResult::EVENT_CONSUMED;
    }

    void DynamicSuperKeys::setup(uint8_t dynamic_offset, uint16_t size)
    {
      storage_base_ = ::EEPROMSettings.requestSlice(size + 8);
      storage_size_ = size;
      offset_ = dynamic_offset;
      updateDynamicSuperKeysCache();
    }

  } // namespace plugin
} //  namespace kaleidoscope

kaleidoscope::plugin::DynamicSuperKeys DynamicSuperKeys;
