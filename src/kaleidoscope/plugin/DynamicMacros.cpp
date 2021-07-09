/* DynamicMacros - Dynamic macro support for Kaleidoscope.
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

#include <Kaleidoscope-OneShot.h>

#include "Kaleidoscope-DynamicMacros.h"
#include "Kaleidoscope-FocusSerial.h"
#include "kaleidoscope/key_events.h"
#include "kaleidoscope/keyswitch_state.h"

namespace kaleidoscope
{
  namespace plugin
  {

    uint16_t DynamicMacros::storage_base_;
    uint16_t DynamicMacros::storage_size_;
    uint16_t DynamicMacros::map_[];

    static void playMacroKeyswitchEvent(Key key, uint8_t keyswitch_state,
                                        bool explicit_report)
    {
      handleKeyswitchEvent(key, UnknownKeyswitchLocation,
                           keyswitch_state | INJECTED);

      if (explicit_report)
        return;

      kaleidoscope::Runtime.hid().keyboard().sendReport();
      kaleidoscope::Runtime.hid().mouse().sendReport();
    }

    static void playKeyCode(Key key, uint8_t keyStates, bool explicit_report)
    {
      if (keyIsPressed(keyStates))
      {
        playMacroKeyswitchEvent(key, IS_PRESSED, explicit_report);
      }
      if (keyWasPressed(keyStates))
      {
        playMacroKeyswitchEvent(key, WAS_PRESSED, explicit_report);
      }
    }

    static void readKeyCodeAndPlay(uint16_t pos, uint8_t flags, uint8_t keyStates,
                                   bool explicit_report)
    {
      Key key(Runtime.storage().read(pos++), // key_code
              flags);

      playKeyCode(key, keyStates, explicit_report);
    }

    void customDelay(uint16_t time)
    {
      int c = (int)time / 1000;
      int d = time % 1000;
      while (c >= 0)
      {
        if (c == 0)
        {
          delay(d);
          c -= 1;
        }
        else
        {
          delay(1000);
          c -= 1;
        }
        if (!WDT->STATUS.bit.SYNCBUSY) // Check if the WDT registers are synchronized
        {
          REG_WDT_CLEAR = WDT_CLEAR_CLEAR_KEY; // Clear the watchdog timer
        }
      }
    }

    void DynamicMacros::updateDynamicMacroCache(void)
    {
      uint16_t pos = storage_base_;
      uint8_t current_id = 0;
      macro_t macro = MACRO_ACTION_END;
      bool previous_macro_ended = false;

      map_[0] = 0;

      while (pos < storage_base_ + storage_size_)
      {
        macro = Runtime.storage().read(pos++);
        switch (macro)
        {
        case MACRO_ACTION_STEP_EXPLICIT_REPORT:
        case MACRO_ACTION_STEP_IMPLICIT_REPORT:
        case MACRO_ACTION_STEP_SEND_REPORT:
          previous_macro_ended = false;
          break;

        case MACRO_ACTION_STEP_INTERVAL:
        case MACRO_ACTION_STEP_WAIT:
        case MACRO_ACTION_STEP_KEYCODEDOWN:
        case MACRO_ACTION_STEP_KEYCODEUP:
        case MACRO_ACTION_STEP_TAPCODE:
          previous_macro_ended = false;
          pos++;
          break;

        case MACRO_ACTION_STEP_KEYDOWN:
        case MACRO_ACTION_STEP_KEYUP:
        case MACRO_ACTION_STEP_TAP:
          previous_macro_ended = false;
          pos += 2;
          break;

        case MACRO_ACTION_STEP_TAP_SEQUENCE:
        {
          previous_macro_ended = false;
          uint8_t keyCode, flags;
          do
          {
            flags = Runtime.storage().read(pos++);
            keyCode = Runtime.storage().read(pos++);
          } while (!(flags == 0 && keyCode == 0));
          break;
        }

        case MACRO_ACTION_STEP_TAP_CODE_SEQUENCE:
        {
          previous_macro_ended = false;
          uint8_t keyCode, flags;
          do
          {
            keyCode = Runtime.storage().read(pos++);
          } while (keyCode != 0);
          break;
        }

        case MACRO_ACTION_END:
          map_[++current_id] = pos - storage_base_;

          if (previous_macro_ended)
            return;

          previous_macro_ended = true;
          break;
        }
      }
    }

    void DynamicMacros::play(uint8_t macro_id)
    {
      macro_t macro = MACRO_ACTION_END;
      uint16_t interval = 0;
      uint8_t flags;
      bool explicit_report = false;
      uint16_t pos;

      pos = storage_base_ + map_[macro_id];

      while (true)
      {
        switch (macro = Runtime.storage().read(pos++))
        {
        case MACRO_ACTION_STEP_EXPLICIT_REPORT:
          explicit_report = true;
          break;
        case MACRO_ACTION_STEP_IMPLICIT_REPORT:
          explicit_report = false;
          break;
        case MACRO_ACTION_STEP_SEND_REPORT:
          kaleidoscope::Runtime.hid().keyboard().sendReport();
          kaleidoscope::Runtime.hid().mouse().sendReport();
          break;
        case MACRO_ACTION_STEP_INTERVAL:
        {
          uint8_t inter1 = Runtime.storage().read(pos++);
          uint8_t inter2 = Runtime.storage().read(pos++);
          interval = (inter1 << 8) | inter2;
          break;
        }
        case MACRO_ACTION_STEP_WAIT:
        {
          uint8_t d2 = Runtime.storage().read(pos++);
          uint8_t d1 = Runtime.storage().read(pos++);
          uint16_t wait = (d2 << 8) | d1;
          customDelay(wait);
          break;
        }
        case MACRO_ACTION_STEP_KEYDOWN:
          flags = Runtime.storage().read(pos++);
          readKeyCodeAndPlay(pos++, flags, IS_PRESSED, explicit_report);
          break;
        case MACRO_ACTION_STEP_KEYUP:
          flags = Runtime.storage().read(pos++);
          readKeyCodeAndPlay(pos++, flags, WAS_PRESSED, explicit_report);
          break;
        case MACRO_ACTION_STEP_TAP:
          flags = Runtime.storage().read(pos++);
          readKeyCodeAndPlay(pos++, flags, IS_PRESSED | WAS_PRESSED, false);
          break;

        case MACRO_ACTION_STEP_KEYCODEDOWN:
          readKeyCodeAndPlay(pos++, 0, IS_PRESSED, explicit_report);
          break;
        case MACRO_ACTION_STEP_KEYCODEUP:
          readKeyCodeAndPlay(pos++, 0, WAS_PRESSED, explicit_report);
          break;
        case MACRO_ACTION_STEP_TAPCODE:
          readKeyCodeAndPlay(pos++, 0, IS_PRESSED | WAS_PRESSED, false);
          break;

        case MACRO_ACTION_STEP_TAP_SEQUENCE:
        {
          uint8_t keyCode;
          do
          {
            flags = Runtime.storage().read(pos++);
            keyCode = Runtime.storage().read(pos++);
            playKeyCode(Key(keyCode, flags), IS_PRESSED | WAS_PRESSED, false);
            customDelay(interval);
          } while (!(flags == 0 && keyCode == 0));
          break;
        }
        case MACRO_ACTION_STEP_TAP_CODE_SEQUENCE:
        {
          uint8_t keyCode;
          do
          {
            keyCode = Runtime.storage().read(pos++);
            playKeyCode(Key(keyCode, 0), IS_PRESSED | WAS_PRESSED, false);
            customDelay(interval);
          } while (keyCode != 0);
          break;
        }

        case MACRO_ACTION_END:
        {
          if (::OneShot.isActive() && !::OneShot.isPressed())
          {
            ::OneShot.cancel(true);
          }
          return;
        }
        default:
          return;
        }

        delay(interval);
        if (!WDT->STATUS.bit.SYNCBUSY) // Check if the WDT registers are synchronized
        {
          REG_WDT_CLEAR = WDT_CLEAR_CLEAR_KEY; // Clear the watchdog timer
        }
      }
    }

    EventHandlerResult DynamicMacros::onKeyswitchEvent(Key &mappedKey,
                                                       KeyAddr key_addr,
                                                       uint8_t keyState)
    {
      if (mappedKey.getRaw() < ranges::DYNAMIC_MACRO_FIRST || mappedKey.getRaw() > ranges::DYNAMIC_MACRO_LAST) {
        return EventHandlerResult::OK;
      }

      if (keyToggledOn(keyState))
      {
        play(mappedKey.getRaw() - ranges::DYNAMIC_MACRO_FIRST);
      }

      return EventHandlerResult::EVENT_CONSUMED;
    }

    EventHandlerResult DynamicMacros::onFocusEvent(const char *command)
    {
      if (::Focus.handleHelp(command, PSTR("macros.map\nmacros.trigger")))
        return EventHandlerResult::OK;

      if (strncmp_P(command, PSTR("macros."), 7) != 0)
        return EventHandlerResult::OK;

      if (strcmp_P(command + 7, PSTR("map")) == 0)
      {
        if (::Focus.isEOL())
        {
          for (uint16_t i = 0; i < storage_size_; i++)
          {
            uint8_t b;
            b = Runtime.storage().read(storage_base_ + i);
            ::Focus.send(b);
          }
        }
        else
        {
          uint16_t pos = 0;

          while (!::Focus.isEOL())
          {
            uint8_t b;
            ::Focus.read(b);

            Runtime.storage().update(storage_base_ + pos++, b);
          }
          Runtime.storage().commit();
          updateDynamicMacroCache();
        }
      }

      if (strcmp_P(command + 7, PSTR("trigger")) == 0)
      {
        uint8_t id = 0;
        ::Focus.read(id);
        play(id);
      }

      return EventHandlerResult::EVENT_CONSUMED;
    }

    void DynamicMacros::reserve_storage(uint16_t size)
    {
      storage_base_ = ::EEPROMSettings.requestSlice(size);
      storage_size_ = size;
      updateDynamicMacroCache();
    }

  } // namespace plugin
} //  namespace kaleidoscope

kaleidoscope::plugin::DynamicMacros DynamicMacros;
