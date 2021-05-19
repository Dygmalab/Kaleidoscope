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
#include "Kaleidoscope-FocusSerial.h"
#include <Kaleidoscope-EEPROM-Settings.h>
#include "kaleidoscope/keyswitch_state.h"

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
    uint16_t DynamicSuperKeys::time_out = 200;
    Key DynamicSuperKeys::last_super_key_ = Key_NoKey;
    KeyAddr DynamicSuperKeys::last_super_addr_;

    void DynamicSuperKeys::updateDynamicSuperKeysCache()
    {
      uint16_t pos = storage_base_;
      uint8_t current_id = 0;
      bool previous_super_key_ended = false;

      super_key_count_ = 0;
      map_[0] = 0;

      while (pos < storage_base_ + storage_size_)
      {
        uint16_t raw_key = Kaleidoscope.storage().read(pos);
        pos += 2;
        Key key(raw_key);

        if (key == Key_NoKey)
        {
          map_[++current_id] = pos - storage_base_;

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
          result = DynamicSuperKeys::Tap_Thrice;
          break;
        default:
          result = DynamicSuperKeys::None;
        }
      }
      if (action == Hold)
      {
        switch (previous)
        {
        case DynamicSuperKeys::None:
          result = DynamicSuperKeys::Tap_Thrice_Hold;
          break;
        case DynamicSuperKeys::Tap_Once:
          result = DynamicSuperKeys::Hold_Once;
          break;
        case DynamicSuperKeys::Tap_Twice:
          result = DynamicSuperKeys::Tap_Hold;
          break;
        case DynamicSuperKeys::Tap_Thrice:
          result = DynamicSuperKeys::Tap_Twice_Hold;
          break;
        default:
          result = DynamicSuperKeys::None;
        }
      }
      return result;
    }

    // --- actions ---

    void DynamicSuperKeys::interrupt(KeyAddr key_addr)
    {
      uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

      SuperKeys(idx, last_super_addr_, state_[idx].count, Interrupt);
      state_[idx].triggered = true;

      last_super_key_ = Key_NoKey;

      Runtime.hid().keyboard().sendReport();
      Runtime.hid().keyboard().releaseAllKeys();

      if (state_[idx].pressed)
        hold();
      return;

      release(idx);
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
      state_[super_key_index].holded = false;
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
        state_[idx].holded = true;
        state_[idx].count = DynamicSuperKeys::ReturnType(state_[idx].count, Hold);
        uint8_t idx = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;
        SuperKeys(idx, last_super_addr_, state_[idx].count, Hold);
      }
    }

    // --- api ---

    bool DynamicSuperKeys::SuperKeys(uint8_t super_key_index, KeyAddr key_addr,
                                     DynamicSuperKeys::SuperType tap_count, DynamicSuperKeys::ActionType super_key_action)
    {
      uint16_t pos = map_[super_key_index - offset_] + ((tap_count - 1) * 2);
      uint16_t next_pos = map_[super_key_index - offset_ + 1];
      if (next_pos <= pos || (super_key_index > offset_ + super_key_count_))
        return false;

      Key key;
      Kaleidoscope.storage().get(storage_base_ + pos, key);

      switch (super_key_action)
      {
      case DynamicSuperKeys::Tap:
        break;
      case DynamicSuperKeys::Interrupt:
      case DynamicSuperKeys::Timeout:
        handleKeyswitchEvent(key, key_addr, IS_PRESSED | INJECTED);
        break;
      case DynamicSuperKeys::Hold:
        handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
        delay(10);
        break;
      case DynamicSuperKeys::Release:
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
          interrupt(key_addr);
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
          if (keyToggledOff(keyState) && static_cast<uint8_t>(state_[super_key_index].count))
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
      if (::Focus.handleHelp(command, PSTR("superkeys.map")))
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
            Kaleidoscope.storage().get(storage_base_ + i, k);
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

            Kaleidoscope.storage().put(storage_base_ + pos, k);
            pos += 2;
          }
          Kaleidoscope.storage().commit();
          updateDynamicSuperKeysCache();
        }
      }

      return EventHandlerResult::EVENT_CONSUMED;
    }

    void DynamicSuperKeys::setup(uint8_t dynamic_offset, uint16_t size)
    {
      storage_base_ = ::EEPROMSettings.requestSlice(size);
      storage_size_ = size;
      offset_ = dynamic_offset;
      updateDynamicSuperKeysCache();
    }

  } // namespace plugin
} //  namespace kaleidoscope

kaleidoscope::plugin::DynamicSuperKeys DynamicSuperKeys;
