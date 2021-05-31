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
    uint16_t DynamicSuperKeys::delayed_time_;
    uint16_t DynamicSuperKeys::wait_for = 500;
    uint8_t DynamicSuperKeys::repeat_interval = 20;
    uint16_t DynamicSuperKeys::time_out = 200;
    Key DynamicSuperKeys::last_super_key_ = Key_NoKey;
    KeyAddr DynamicSuperKeys::last_super_addr_;

    void DynamicSuperKeys::updateDynamicSuperKeysCache()
    {
      uint16_t pos = storage_base_ + 5;
      uint8_t current_id = 0;
      bool previous_super_key_ended = false;

      super_key_count_ = 0;
      map_[0] = 0;
      Kaleidoscope.storage().get(storage_base_ + 0, wait_for);
      Kaleidoscope.storage().get(storage_base_ + 2, time_out);
      Kaleidoscope.storage().get(storage_base_ + 4, repeat_interval);

      while (pos < (storage_base_ + 5) + storage_size_)
      {
        uint16_t raw_key = Kaleidoscope.storage().read(pos);
        pos += 2;
        Key key(raw_key);

        if (key == Key_NoKey)
        {
          map_[++current_id] = pos - storage_base_ - 5;

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
        hold();
        state_[idx].triggered = true;
        Runtime.hid().keyboard().sendReport();
        return false;
      }

      SuperKeys(idx, last_super_addr_, state_[idx].count, Interrupt);
      state_[idx].triggered = true;

      last_super_key_ = Key_NoKey;

      Runtime.hid().keyboard().sendReport();
      Runtime.hid().keyboard().releaseAllKeys();

      release(idx);
      return true;
    }

    void DynamicSuperKeys::timeout(void)
    {
      uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      state_[idx].triggered = true;

      if (state_[idx].pressed)
      {
        hold();
        return;
      }

      SuperKeys(idx, last_super_addr_, state_[idx].count, Timeout);

      last_super_key_ = Key_NoKey;

      release(idx);
    }

    void DynamicSuperKeys::release(uint8_t super_key_index)
    {
      last_super_key_ = Key_NoKey;

      state_[super_key_index].pressed = false;
      state_[super_key_index].triggered = false;
      state_[super_key_index].release_next = true;
    }

    void DynamicSuperKeys::tap(void)
    {
      uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      state_[idx].count = DynamicSuperKeys::ReturnType(state_[idx].count, Tap);
      start_time_ = Runtime.millisAtCycleStart();

      SuperKeys(idx, last_super_addr_, state_[idx].count, Tap);
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
      Kaleidoscope.storage().get(storage_base_ + pos + 5, key);

      switch (super_key_action)
      {
      case DynamicSuperKeys::Tap:
        break;
      case DynamicSuperKeys::Interrupt:
      case DynamicSuperKeys::Timeout:
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
        handleKeyswitchEvent(key, key_addr, IS_PRESSED | INJECTED);
        break;
      case DynamicSuperKeys::Hold:
        if (delayed_time_ == 0)
        {
          if (key.getRaw() >= 17450 && key.getRaw() <= 17459)
          {
            ::Layer.activate(key.getKeyCode() - LAYER_SHIFT_OFFSET);
            break;
          }
          if (key.getRaw() >= ranges::DYNAMIC_MACRO_FIRST && key.getRaw() <= ranges::DYNAMIC_MACRO_LAST)
          {
            ::DynamicMacros.play(key.getRaw() - ranges::DYNAMIC_MACRO_FIRST);
            break;
          }
          handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
        }
        else
        {
          if (Runtime.hasTimeExpired(delayed_time_, wait_for))
          {
            if (key.getRaw() >= ranges::DYNAMIC_MACRO_FIRST && key.getRaw() <= ranges::DYNAMIC_MACRO_LAST)
            {
              delay(repeat_interval);
              ::DynamicMacros.play(key.getRaw() - ranges::DYNAMIC_MACRO_FIRST);
              break;
            }
            if (key.getRaw() == 23785 || key.getRaw() == 23786)
            {
              delay(repeat_interval);
              kaleidoscope::Runtime.hid().keyboard().sendReport();
            }
          }
          handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
        }
        break;
      case DynamicSuperKeys::Release:
        if (key.getRaw() >= 17450 && key.getRaw() <= 17459)
        {
          ::Layer.deactivate(key.getKeyCode() - LAYER_SHIFT_OFFSET);
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
      if (keyState & INJECTED)
        return EventHandlerResult::OK;

      if (mapped_key.getRaw() < ranges::DYNAMIC_SUPER_FIRST || mapped_key.getRaw() > ranges::DYNAMIC_SUPER_LAST)
      {
        if (last_super_key_ == Key_NoKey)
          return EventHandlerResult::OK;

        if (keyToggledOn(keyState))
        {
          if (interrupt(key_addr))
            mapped_key = Key_NoKey;
        }

        return EventHandlerResult::OK;
      }

      uint8_t super_key_index = mapped_key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      if (keyToggledOff(keyState))
        state_[super_key_index].pressed = false;

      if (last_super_key_ != mapped_key)
      {
        if (last_super_key_ == Key_NoKey)
        {
          if (state_[super_key_index].triggered)
          {
            if (keyToggledOff(keyState))
            {
              release(super_key_index);
            }

            return EventHandlerResult::EVENT_CONSUMED;
          }

          last_super_key_ = mapped_key;
          last_super_addr_ = key_addr;

          tap();

          return EventHandlerResult::EVENT_CONSUMED;
        }
        else
        {
          if (keyToggledOff(keyState))
          {
            release(super_key_index);
            return EventHandlerResult::EVENT_CONSUMED;
          }

          if (!keyToggledOn(keyState))
          {
            return EventHandlerResult::EVENT_CONSUMED;
          }

          interrupt(key_addr);
        }
      }

      // in sequence

      if (keyToggledOff(keyState))
      {
        if (state_[super_key_index].holded)
          release(super_key_index);
        return EventHandlerResult::EVENT_CONSUMED;
      }

      last_super_key_ = mapped_key;
      last_super_addr_ = key_addr;
      state_[super_key_index].pressed = true;

      if (keyToggledOn(keyState))
      {
        tap();
        return EventHandlerResult::EVENT_CONSUMED;
      }

      if (state_[super_key_index].triggered)
        hold();
      return EventHandlerResult::EVENT_CONSUMED;
    }

    EventHandlerResult DynamicSuperKeys::afterEachCycle()
    {
      for (uint8_t i = 0; i < SUPER_KEY_COUNT; i++)
      {
        if (!state_[i].release_next)
          continue;

        SuperKeys(i, last_super_addr_, state_[i].count, Release);
        state_[i].holded = false;
        state_[i].count = None;
        state_[i].release_next = false;
      }

      if (last_super_key_ == Key_NoKey)
        return EventHandlerResult::OK;

      if (Runtime.hasTimeExpired(start_time_, time_out))
        timeout();

      return EventHandlerResult::OK;
    }

    EventHandlerResult DynamicSuperKeys::onFocusEvent(const char *command)
    {
      if (::Focus.handleHelp(command, PSTR("superkeys.map\nsuperkeys.waitfor\nsuperkeys.timeout\nsuperkeys.repeat")))
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
            Kaleidoscope.storage().get(storage_base_ + i + 5, k);
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

            Kaleidoscope.storage().put(storage_base_ + pos + 5, k);
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
          ::Focus.send(DynamicSuperKeys::wait_for);
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
          ::Focus.send(DynamicSuperKeys::time_out);
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
      if (strcmp_P(command + 10, PSTR("repeat")) == 0)
      {
        if (::Focus.isEOL())
        {
          ::Focus.send(DynamicSuperKeys::repeat_interval);
        }
        else
        {
          uint8_t repeat = 0;
          ::Focus.read(repeat);
          Kaleidoscope.storage().put(storage_base_ + 4, repeat);
          Kaleidoscope.storage().commit();
          updateDynamicSuperKeysCache();
        }
      }

      return EventHandlerResult::EVENT_CONSUMED;
    }

    void DynamicSuperKeys::setup(uint8_t dynamic_offset, uint16_t size)
    {
      storage_base_ = ::EEPROMSettings.requestSlice(size + 5);
      storage_size_ = size;
      offset_ = dynamic_offset;
      updateDynamicSuperKeysCache();
    }

  } // namespace plugin
} //  namespace kaleidoscope

kaleidoscope::plugin::DynamicSuperKeys DynamicSuperKeys;
