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

#pragma once

#include <Kaleidoscope.h>
#include <Kaleidoscope-Ranges.h>

#define DS(n) Key(kaleidoscope::ranges::DYNAMIC_SUPER_FIRST + n)

namespace kaleidoscope
{
  namespace plugin
  {

    class DynamicSuperKeys : public kaleidoscope::Plugin
    {
    public:
      typedef enum
      {
        Tap,
        Hold,
        Interrupt,
        Timeout,
        Release,
      } ActionType;

      typedef enum
      {
        None,
        Tap_Once,
        Hold_Once,
        Tap_Hold,
        Tap_Twice,
        Tap_Twice_Hold,
        Tap_Trice,
      } SuperType;

      DynamicSuperKeys() {}

      static bool SuperKeys(uint8_t tap_dance_index, KeyAddr key_addr, DynamicSuperKeys::SuperType tap_count, DynamicSuperKeys::ActionType tap_dance_action);
      EventHandlerResult onKeyswitchEvent(Key &mapped_key, KeyAddr key_addr, uint8_t keyState);

      EventHandlerResult onFocusEvent(const char *command);
      EventHandlerResult beforeReportingState();

      static void setup(uint8_t dynamic_offset, uint16_t size);

    private:
      static constexpr uint8_t SUPER_KEY_COUNT = ranges::DYNAMIC_SUPER_LAST - ranges::DYNAMIC_SUPER_FIRST + 1;
      struct SuperKeyState
      {
        bool pressed : 1;
        bool triggered : 1;
        bool release_next : 1;
        bool holded : 1;
        bool printonrelease : 1;
        SuperType count;
      };
      static SuperKeyState state_[SUPER_KEY_COUNT];
      static uint16_t map_[SUPER_KEY_COUNT];
      static uint16_t storage_base_;
      static uint16_t storage_size_;
      static uint8_t super_key_count_;
      static uint8_t offset_;
      static uint16_t start_time_;
      static uint16_t last_start_time_;
      static uint16_t delayed_time_;
      static Key last_super_key_;
      static KeyAddr last_super_addr_;
      static bool modifier_pressed_;
      static bool layer_shifted_;
      static uint8_t layer_shifted_number_;
      static uint16_t time_out_;
      static uint16_t wait_for_;
      static uint16_t hold_start_;
      static uint8_t repeat_interval_;
      static uint8_t overlap_threshold_;

      static void updateDynamicSuperKeysCache();
      static SuperType ReturnType(DynamicSuperKeys::SuperType previous, DynamicSuperKeys::ActionType action);
      static void tap(void);
      static void hold(void);
      static bool interrupt(KeyAddr key_addr);
      static void timeout(void);
      static void release(uint8_t super_key_index);
      static bool releaseDelayed(uint16_t overlap_start, uint16_t overlap_end);
    };

  }
}

extern kaleidoscope::plugin::DynamicSuperKeys DynamicSuperKeys;
