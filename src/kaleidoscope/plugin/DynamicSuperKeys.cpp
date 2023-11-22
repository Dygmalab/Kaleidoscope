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

#include "Kaleidoscope-DynamicMacros.h"
#include "Kaleidoscope-DynamicSuperKeys.h"
#include "Kaleidoscope-FocusSerial.h"

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
        uint16_t DynamicSuperKeys::delayed_time_;
        uint16_t DynamicSuperKeys::wait_for_ = 500;
        uint16_t DynamicSuperKeys::hold_start_ = 236;
        uint8_t DynamicSuperKeys::repeat_interval_ = 20;
        uint8_t DynamicSuperKeys::overlap_threshold_ = 80;
        uint16_t DynamicSuperKeys::time_out_ = 144;
        Key DynamicSuperKeys::last_super_key_ = Key_NoKey;
        KeyAddr DynamicSuperKeys::last_super_addr_;
        bool DynamicSuperKeys::modifier_pressed_ = false;
        bool DynamicSuperKeys::layer_shifted_ = false;
        uint8_t DynamicSuperKeys::layer_shifted_number_ = 0;
        uint8_t DynamicSuperKeys::super_key_index = 0;
        bool DynamicSuperKeys::fast_key_release = false;
        static const uint8_t MAX_SUPER_KEYS_ACTIVE = 50;
        DynamicSuperKeys::KeyValue keys[MAX_SUPER_KEYS_ACTIVE];
        uint8_t keys_index;
        uint8_t currentIndex = 0;

        void DynamicSuperKeys::updateDynamicSuperKeysCache()
        {
            uint16_t pos = storage_base_ + 8;
            uint8_t current_id = 0;
            bool previous_super_key_ended = false;
            uint8_t action_count = 0;
            super_key_count_ = 0;
            map_[0] = 0;

            uint16_t wait_for;
            uint16_t time_out;
            uint16_t hold_start;
            uint8_t repeat_interval;
            uint8_t overlap_threshold;


            Runtime.storage().get(storage_base_ + 0, wait_for);
            if (wait_for < 2000)
            {
                DynamicSuperKeys::wait_for_ = wait_for;
            }
            else
            {
                Runtime.storage().put(storage_base_ + 0, DynamicSuperKeys::wait_for_);
                Runtime.storage().commit();
                Runtime.storage().get(storage_base_ + 0, DynamicSuperKeys::wait_for_);
            }

            Runtime.storage().get(storage_base_ + 2, time_out);
            if (time_out != 65535)
            {
                DynamicSuperKeys::time_out_ = time_out;
            }
            else
            {
                Runtime.storage().put(storage_base_ + 2, DynamicSuperKeys::time_out_);
                Runtime.storage().commit();
                Runtime.storage().get(storage_base_ + 2, DynamicSuperKeys::time_out_);
            }

            Runtime.storage().get(storage_base_ + 4, hold_start);
            if (hold_start < 500)
            {
                DynamicSuperKeys::hold_start_ = hold_start;
            }
            else
            {
                Runtime.storage().put(storage_base_ + 4, DynamicSuperKeys::hold_start_);
                Runtime.storage().commit();
                Runtime.storage().get(storage_base_ + 4, DynamicSuperKeys::hold_start_);
            }

            Runtime.storage().get(storage_base_ + 6, repeat_interval);
            if (repeat_interval < 251)
            {
                DynamicSuperKeys::repeat_interval_ = repeat_interval;
            }
            else
            {
                Runtime.storage().put(storage_base_ + 6, DynamicSuperKeys::repeat_interval_);
                Runtime.storage().commit();
                Runtime.storage().get(storage_base_ + 6, DynamicSuperKeys::repeat_interval_);
            }

            Runtime.storage().get(storage_base_ + 7, overlap_threshold);
            if (overlap_threshold < 251)
            {
                DynamicSuperKeys::overlap_threshold_ = overlap_threshold;
            }
            else
            {
                Runtime.storage().put(storage_base_ + 7, DynamicSuperKeys::overlap_threshold_);
                Runtime.storage().commit();
                Runtime.storage().get(storage_base_ + 7, DynamicSuperKeys::overlap_threshold_);
            }
            last_super_key_ = Key_NoKey;
            while (pos < (storage_base_ + 8) + storage_size_)
            {
                // If the lecture of the key is == 1 we don't have an action define.
                uint16_t raw_key = Runtime.storage().read(pos);

                if (raw_key == 1)
                {
                    if (action_count == 0)
                    {
                        state_[current_id].actions.tap_set = false;
                    }
                    if (action_count == 1)
                    {
                        state_[current_id].actions.hold_set = false;
                    }
                    if (action_count == 2)
                    {
                        state_[current_id].actions.tapHold_set = false;
                    }
                    if (action_count == 3)
                    {
                        state_[current_id].actions.doubleTap_set = false;
                    }
                    if (action_count == 4)
                    {
                        state_[current_id].actions.doubleTapHold_set = false;
                    }
                }
                else
                {
                    if (action_count == 0)
                    {
                        state_[current_id].actions.tap_set = true;
                    }
                    if (action_count == 1)
                    {
                        state_[current_id].actions.hold_set = true;
                    }
                    if (action_count == 2)
                    {
                        state_[current_id].actions.tapHold_set = true;
                    }
                    if (action_count == 3)
                    {
                        state_[current_id].actions.doubleTap_set = true;
                    }
                    if (action_count == 4)
                    {
                        state_[current_id].actions.doubleTapHold_set = true;
                    }
                }
                pos += 2;
                Key key(raw_key);

                if (action_count == 5)
                {
                    state_[current_id].printonrelease = (pos - storage_base_ - 8 - map_[current_id]) == 6;
                    map_[++current_id] = pos - storage_base_ - 8;
                    if (previous_super_key_ended) return;

                    super_key_count_++;
                    action_count = 0;
                    previous_super_key_ended = true;
                }
                else
                {
                    action_count++;
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

// ****************************** actions ******************************

        bool DynamicSuperKeys::interrupt(Key key, const KeyAddr &keyAddr)
        {
            uint8_t idx = key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;
            if (!state_[idx].holded)
            {
                SuperKeys(idx, keyAddr, state_[idx].count, Interrupt);
            }

            // NRF_LOG_DEBUG("***interrupt SK %i***",idx);
            state_[idx].start_time = 0;
            state_[idx].pressed = false;
            state_[idx].triggered = false;
            state_[idx].holded = false;
            state_[idx].count = None;
            state_[idx].delayed_time = 0;
            state_[idx].released = false;
            state_[idx].has_already_send = false;
            if (state_[idx].is_layer_shifting)
            {
                // TODO: move to the previous layer.
                ::Layer.move(0);
                state_[idx].is_layer_shifting = false;
            }
            updateKey(key, state_[idx]);
            removeKey(key);
            return true;
        }

        void DynamicSuperKeys::timeout(Key key, const KeyAddr &keyAddr)
        {
            uint8_t idx = key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;
            if (state_[idx].pressed)
            {
                return;
            }
            if (!state_[idx].holded)
            {
                SuperKeys(idx, keyAddr, state_[idx].count, Timeout);
            }
            else if (state_[idx].holded && !state_[idx].released)
            {
                return;
            }
            // NRF_LOG_DEBUG("***TIMEOUT SK %i***",idx);
            state_[idx].start_time = 0;
            state_[idx].pressed = false;
            state_[idx].triggered = false;
            state_[idx].holded = false;
            state_[idx].count = None;
            state_[idx].delayed_time = 0;
            state_[idx].released = false;
            state_[idx].has_already_send = false;

            if (state_[idx].is_layer_shifting)
            {
                // TODO: move to the previous layer.
                ::Layer.move(0);
                state_[idx].is_layer_shifting = false;
            }
            updateKey(key, state_[idx]);
            removeKey(key);
        }

        void DynamicSuperKeys::release(Key key, const KeyAddr &keyAddr)
        {
            uint8_t idx = key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;
            SuperKeys(idx, keyAddr, state_[idx].count, Timeout);
            state_[idx].start_time = 0;
            state_[idx].pressed = false;
            state_[idx].triggered = false;
            state_[idx].holded = false;
            state_[idx].count = None;
            state_[idx].delayed_time = 0;
            state_[idx].released = false;
            state_[idx].has_already_send = false;
            updateKey(key, state_[idx]);
            if (state_[idx].is_layer_shifting)
            {
                ::Layer.move(0);
                state_[idx].is_layer_shifting = false;
            }
        }

        void DynamicSuperKeys::tap(Key key)
        {
            uint8_t idx = key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

            state_[idx].count = DynamicSuperKeys::ReturnType(state_[idx].count, Tap); // return the amount of tap.
            state_[idx].start_time = Runtime.millisAtCycleStart();
            updateKey(key, state_[idx]); // Update the key status.
        }

        void DynamicSuperKeys::hold(Key key, const KeyAddr &keyAddr)
        {
            uint8_t idx = key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

            if (state_[idx].holded)
            {
                SuperKeys(idx, keyAddr, state_[idx].count, Hold);
            }
            else
            {
                // NRF_LOG_DEBUG("***delayed_time_ 0 %i***",super_key_index);
                delayed_time_ = 0;
                state_[idx].delayed_time = delayed_time_;
                state_[idx].holded = true;
                state_[idx].triggered = true;
                state_[idx].count = DynamicSuperKeys::ReturnType(state_[idx].count, Hold);
                SuperKeys(idx, keyAddr, state_[idx].count, Hold);
                delayed_time_ = Runtime.millisAtCycleStart();
                state_[idx].delayed_time = delayed_time_;
                updateKey(key, state_[idx]);
            }
        }

// ****************************** api ******************************

        bool DynamicSuperKeys::SuperKeys(uint8_t super_key_index, KeyAddr key_addr, DynamicSuperKeys::SuperType tap_count, DynamicSuperKeys::ActionType super_key_action)
    {
        // TODO: move this code to a function (getKeyfromSuperkey)
        DynamicSuperKeys::SuperType corrected = tap_count;
        uint16_t pos = map_[super_key_index - offset_] + ((corrected - 1) * 2);
        uint16_t next_pos = map_[super_key_index - offset_ + 1];
        if (next_pos < pos || (super_key_index > offset_ + super_key_count_)) return false;

        Key key;
        Kaleidoscope.storage().get(storage_base_ + pos + 8, key);

        switch (super_key_action)
    {
        case DynamicSuperKeys::Tap:
        break;
        case DynamicSuperKeys::Interrupt:
        case DynamicSuperKeys::Timeout:
    {
        uint8_t modif = (key.getRaw() & 0xFF00) >> 8;
        uint8_t TEST = key.getRaw();
        // NRF_LOG_DEBUG("TEST %i",TEST);
        if (key.getRaw() == 1)
    {
        if (tap_count == DynamicSuperKeys::Tap_Twice)
    {
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
}
if (key.getRaw() >= LAYER_LOCK_FIRST && key.getRaw() <= LAYER_LOCK_LAST)
{
::Layer.move(key.getKeyCode() - LAYER_MOVE_OFFSET);
break;
}
if (key.getRaw() >= DynamicSuperKeys::LED_BUTTONS_FIRST && key.getRaw() <= DynamicSuperKeys::LED_BUTTONS_LAST)
{
handleKeyswitchEvent(key, key_addr, IS_PRESSED | INJECTED);
// NRF_LOG_DEBUG("LED_BUTTONS_FIRST");
break;
}
if (key.getRaw() >= ranges::DYNAMIC_MACRO_FIRST && key.getRaw() <= ranges::DYNAMIC_MACRO_LAST)
{
::DynamicMacros.play(key.getRaw() - ranges::DYNAMIC_MACRO_FIRST);
break;
}
if (key.getRaw() >= 256 && key.getRaw() <= 7935)
{
// NRF_LOG_DEBUG("key.getRaw() >= 256");
if (modif & 0x01)
{
// NRF_LOG_DEBUG("Key_LeftControl");
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
break;
}
/*If we press a modifier and after that a SK we have to be able to release this SK with the associated modifier*/
if (key.getRaw() < ALPHA_KEYS && modifier_pressed_)
{
// NRF_LOG_DEBUG("key.getRaw() < 256");

if (modif & 0x01)
{
// NRF_LOG_DEBUG("Key_LeftControl");
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
break;
}
if (tap_count == DynamicSuperKeys::Tap_Trice)
{
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
kaleidoscope::Runtime.hid().keyboard().sendReport();
handleKeyswitchEvent(key2, key_addr, IS_PRESSED | INJECTED);
kaleidoscope::Runtime.hid().keyboard().sendReport();
handleKeyswitchEvent(key2, key_addr, WAS_PRESSED | INJECTED);
break;
}
handleKeyswitchEvent(key, key_addr, IS_PRESSED | INJECTED);
kaleidoscope::Runtime.hid().keyboard().sendReport();
break;
}
case DynamicSuperKeys::Hold:
{

// We get the most significant bits from the keys. These bits tell us which modifiers the key has.
uint8_t modif = (key.getRaw() & 0xFF00) >> 8;
uint8_t key_id = key.getRaw() & 0x00FF;
uint8_t TEST = key.getRaw();
// NRF_LOG_DEBUG("TEST %i",TEST);
if (state_[super_key_index].delayed_time == 0) // This will execute in the first moment we hold the key and the hold_start timer expires.
{
if (key.getRaw() >= LAYER_LOCK_FIRST && key.getRaw() <= LAYER_LOCK_LAST)
{
::Layer.move(key.getKeyCode() - LAYER_MOVE_OFFSET);
break;
}
if (key.getRaw() >= LAYER_SHIFT_FIRST && key.getRaw() <= LAYER_SHIFT_LAST)
{
state_[super_key_index].is_layer_shifting = true;
layer_shifted_number_ = key.getKeyCode() - LAYER_SHIFT_OFFSET;
::Layer.activate(layer_shifted_number_);
handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
break;
}
if (key.getRaw() >= ranges::DYNAMIC_MACRO_FIRST && key.getRaw() <= ranges::DYNAMIC_MACRO_LAST)
{
::DynamicMacros.play(key.getRaw() - ranges::DYNAMIC_MACRO_FIRST);
break;
}
// superkeys with modifiers.
if (key.getRaw() >= 256 && key.getRaw() <= 7935)
{
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
handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
break;
}
if (key.getRaw() >= DynamicSuperKeys::LED_BUTTONS_FIRST && key.getRaw() <= DynamicSuperKeys::LED_BUTTONS_LAST)
{
// NRF_LOG_DEBUG("Handling LED BUTTONS %i", key.getRaw());
handleKeyswitchEvent(key, key_addr, IS_PRESSED | INJECTED);
state_[super_key_index].pressed = false;
// handleKeyswitchEvent(key, key_addr, IS_INTERNAL);
break;
}

state_[super_key_index].has_already_send = true;
updateKey(key, state_[super_key_index]);
// This is the case in which the key is only A modifier with NO other modifiers associate,
//  or the key is a modifier with other modifiers associate (e.g.SHIFT + CTRL)
if ((key.getRaw() >= 224 && key.getRaw() <= 231) || lowerKeyIsModifier(key_id))
{
state_[super_key_index].has_modifier_in_action = true;
updateKey(key, state_[super_key_index]);
}
handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
}
else // If we hold enough time
{
if (Runtime.hasTimeExpired(state_[super_key_index].delayed_time, wait_for_))
{
if (key.getRaw() >= LAYER_SHIFT_FIRST && key.getRaw() <= LAYER_SHIFT_LAST)
{
break;
}
if (key.getRaw() == 23785 || key.getRaw() == 23786)
{
// NRF_LOG_DEBUG("key.getRaw() == 23785 || key.getRaw() == 23786");
// This is to send the report only once.
if (!state_[super_key_index].has_already_send && !state_[super_key_index].has_modifier_in_action)
{
kaleidoscope::Runtime.hid().keyboard().sendReport();
release(key, key_addr);
}
state_[super_key_index].has_already_send = true;
updateKey(key, state_[super_key_index]);
}
if (key.getRaw() >= 256 && key.getRaw() <= 7935)
{
// NRF_LOG_DEBUG("key.getRaw() >= 256 && key.getRaw() <= 7935");
// This is to send the key only once, and prevent for weird repetitive sending.
/*         if (!state_[super_key_index].has_already_send && !state_[super_key_index].has_modifier_in_action){
           //NRF_LOG_DEBUG("RELEASING SK %i",super_key_index);
           kaleidoscope::Runtime.hid().keyboard().sendReport();
          // release(key,key_addr);
           state_[super_key_index].has_already_send = true;
           updateKey(key,state_[super_key_index]);
         }*/
break;
}
if (key.getRaw() >= DynamicSuperKeys::LED_BUTTONS_FIRST && key.getRaw() <= DynamicSuperKeys::LED_BUTTONS_LAST)
{
break;
}
}
handleKeyswitchEvent(key, key_addr, IS_PRESSED | WAS_PRESSED | INJECTED);
}
break;
}
case DynamicSuperKeys::Release:
{

if (key.getRaw() == 1) // no key, we don't do anything.
{
break;
}
if (key.getRaw() >= ranges::DYNAMIC_MACRO_FIRST && key.getRaw() <= ranges::DYNAMIC_MACRO_LAST) // Key it's a macro so we dont treat it here
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
handleKeyswitchEvent(key, key_addr, WAS_PRESSED | INJECTED);
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
}

return true;
}

// ****************************** hooks ******************************

EventHandlerResult DynamicSuperKeys::onKeyswitchEvent(Key &mapped_key, KeyAddr key_addr, uint8_t keyState)
{
    // If k is not a physical key, ignore it; some other plugin injected it.
    if (keyState & INJECTED)
    {
        return EventHandlerResult::OK;
    }
    // If it's not a super-key press, we treat it here.
    if (mapped_key.getRaw() < ranges::DYNAMIC_SUPER_FIRST || mapped_key.getRaw() > ranges::DYNAMIC_SUPER_LAST)
    {
        // We detect any previously pressed modifiers to be able to release the configured tap or held key when pressed
        if (mapped_key.getRaw() <= Key_RightGui.getRaw() && mapped_key.getRaw() >= Key_LeftControl.getRaw())
        {
            if (keyToggledOff(keyState))
            {
                uint8_t key_id = mapped_key.getRaw() & 0x00FF;
                // This block check if the pressed key is a modifier, If it is true, we will interrupt the super-key in order to shift it.
                // For example, If we press a SHIFT and an "a" which is a super-key, we will interrupt the SK and the result will be A.
                if (lowerKeyIsModifier(key_id) && keys_index != 0)
                {
                    for (int i = 0; i < keys_index; ++i)
                    {
                        if (!keys[i].state.has_modifier_in_action && !keys[i].state.is_layer_shifting) interrupt(keys[i].key, keys[i].keyAddr);
                    }
                }
                modifier_pressed_ = false;
            }
            if (keyToggledOn(keyState))
            {
                modifier_pressed_ = true;
            }
        }

        /*
         *  In this case, we INTERRUPT the SK if we press a regular Key.
         *  The only type of SKs that isn't interrupted is the ones who have layer changing and
         *  modifier configured.
         */
        if (keys_index == 0)
        {
            return EventHandlerResult::OK;
        }
        else
        {
            // This only executing if there was a previous super-key pressed
            if (keyToggledOn(keyState))
            {
                for (int i = 0; i < keys_index; ++i)
                {
                    if (!keys[i].state.has_modifier_in_action && !keys[i].state.is_layer_shifting)
                    {
                        interrupt(keys[i].key, keys[i].keyAddr);
                    }
                }
                return EventHandlerResult::OK;
            }
        }

        return EventHandlerResult::OK;
    }

    // If it's a super-key press, we treat it here.
    // Get the super-key index of the received super-key.
    super_key_index = mapped_key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

    // First state if the key that was triggered, was pressed or not.
    if (keyToggledOff(keyState))
    {
        state_[super_key_index].pressed = false;
        if (state_[super_key_index].holded)
        {
            state_[super_key_index].released = true;
        }
        // NRF_LOG_DEBUG("*** SUPERKEY toggle off: %i ***",super_key_index);
    }

    if (keyToggledOn(keyState))
    {
        state_[super_key_index].pressed = true;
        if (!state_[super_key_index].actions.tapHold_set && !state_[super_key_index].actions.doubleTap_set && !state_[super_key_index].actions.doubleTapHold_set)
        {
            state_[super_key_index].is_qukey = true;
            // NRF_LOG_DEBUG("is_qukey = true");
        }
        else
        {
            state_[super_key_index].is_qukey = false;
            // NRF_LOG_DEBUG("is_qukey = false");
        }
        updateKey(mapped_key, state_[super_key_index]);
    }
    /************************************************************************************************************************/
    /* This is the point of entry to the function, when pressing the super-key for the fist time of each run.
     * The parameter count tells us how many times a key has been pressed.
     * If we have zero presses, that means that this Sk has never been pressed.
     */
    if (state_[super_key_index].count == None)
    {

        // If the key is released, this shouldn't happen, so leave it as it is.
        if (keyToggledOff(keyState)) return EventHandlerResult::EVENT_CONSUMED;
        if (keyToggledOn(keyState))
        {
            // If the key is just pressed, and we don't have other SK, save the super-key.
            if (last_super_key_ != mapped_key)
            {
                last_super_key_ = mapped_key;
                last_super_addr_ = key_addr;
                // NRF_LOG_DEBUG("NEW sk %i raw key: %i",super_key_index, mapped_key.getRaw());
            }
            addKey(mapped_key, key_addr, state_[super_key_index]);
        }
    }
    /************************************************************************************************************************/
    // This IF block treats the behavior for the case in witch a different super-key is pressed after the previous one.
    if (last_super_key_ != mapped_key && last_super_key_ != Key_NoKey)
    {
        if (!keyToggledOn(keyState))
        {
            uint8_t last_super_index = last_super_key_.getRaw() - ranges::DYNAMIC_SUPER_FIRST;

            // At this point, we stop processing the previous key, and analyze the next super-key.
            updateKey(last_super_key_, state_[last_super_index]);
            KeyValue next_super_key = getNextSuperKey();
            KeyValue last_supe_key = keys[last_super_index];

            if (next_super_key.key != Key_NoKey)
            {
                last_super_key_ = next_super_key.key;
                last_super_addr_ = next_super_key.keyAddr;
                if (Runtime.hasTimeExpired(state_[super_key_index].start_time, hold_start_))
                {
                    hold(mapped_key, key_addr);
                    return EventHandlerResult::EVENT_CONSUMED;
                }
            }
            return EventHandlerResult::EVENT_CONSUMED;
        }
        return EventHandlerResult::EVENT_CONSUMED;
    }
    /************************************************************************************************************************/
    // If we receive the same SK, we treated here.
    if (last_super_key_ == mapped_key)
    {
        if (keyToggledOn(keyState))
        {
            tap(mapped_key);
            return EventHandlerResult::EVENT_CONSUMED;
        }
        if (keyToggledOff(keyState))
        {
            // NRF_LOG_DEBUG("*** keyToggledOff(keyState) toggle off: %i ***",super_key_index);

            if (keys_index >= MINIMUM_KEYS_REQUIRES_IN_QUEUE)
            {
                for (int i = 0; i < keys_index; ++i)
                {
                    auto &actual_super_key = keys[i];
                    if (i + 1 == keys_index) break;
                    auto &next_super_key_ = keys[i + 1];
                    // NRF_LOG_DEBUG("*** next_super_key.state.start_time: %i   actual_super_key.state.start_time: %i ***", next_super_key_.state.start_time,
                    // static_cast<int>(actual_super_key.state.start_time * 0.8));
                    //  Now we know the superkey has been released, but we need to check to see if
                    //  it's release should continue to be delayed during rollover -- if the
                    //  subsequent key is released soon enough after the superkey is released, it
                    //  will meet the maximum overlap requirement to make the superkey take on its
                    //  alternate state.
                    uint16_t overlap_start = next_super_key_.state.start_time;
                    uint16_t overlap_end = actual_super_key.state.start_time;
                    if (releaseDelayed(overlap_start, overlap_end))
                    {
                        // NRF_LOG_DEBUG("*** releasing: %i ***", actual_super_key.index);
                        fast_key_release = true;
                        state_[actual_super_key.index].released = true;
                        state_[actual_super_key.index].has_modifier_in_action = false;
                        for (int i = 0; i < keys_index; ++i)
                        {
                            state_[keys[i].index].holded = false;
                            timeout(keys[i].key, keys[i].keyAddr);
                        }
                        return EventHandlerResult::EVENT_CONSUMED;
                    }
                }
            }

            // If the printonrelease flag is true, or we only have tap and hold actions set it, release the key
            // modifier_pressed_ = true; This is to release the SK if we have a SHIFT pressed for example.
            // If the SK doesn't have an action on taphold double tap and double tap and hold, it will release like a normal key.
            // More quickly.
            if (modifier_pressed_ || state_[super_key_index].is_qukey)
            {
                // NRF_LOG_DEBUG("QUICK SUPER-KEY RELEASE: %i", super_key_index);
                // NRF_LOG_DEBUG("state_[super_key_index].is_qukey: %i", state_[super_key_index].is_qukey);
                fast_key_release = true;
                state_[super_key_index].released = true;
                state_[super_key_index].has_modifier_in_action = false;
                updateKey(mapped_key, state_[super_key_index]);
                return EventHandlerResult::EVENT_CONSUMED;
            }
            else
            {
                // NRF_LOG_DEBUG("NORMAL SUPERKEY RELEASE: %i", super_key_index);
                state_[super_key_index].has_modifier_in_action = false;
                fast_key_release = false;
                state_[super_key_index].released = true;
                updateKey(mapped_key, state_[super_key_index]);
            }
            return EventHandlerResult::EVENT_CONSUMED;
        }
        // IS PRESSED
        if (Runtime.hasTimeExpired(state_[super_key_index].start_time, hold_start_))
        {
            hold(mapped_key, key_addr);
            return EventHandlerResult::EVENT_CONSUMED;
        }
        return EventHandlerResult::EVENT_CONSUMED;
    }

    return EventHandlerResult::EVENT_CONSUMED;
}
/************************************************************************************************************************/
EventHandlerResult DynamicSuperKeys::beforeReportingState()
{
    if (keys_index == 0)
    {
        last_super_key_ = Key_NoKey;
        fast_key_release = false;
        return EventHandlerResult::OK;
    }
    if (!fast_key_release)
    {
        // If we tap the SK. A time-out has to expire in order to send the Key to the OS.
        for (int i = 0; i < keys_index; ++i)
        {
            if (state_[keys[i].index].start_time > 0 && Runtime.hasTimeExpired(state_[keys[i].index].start_time, time_out_))
            {
                timeout(keys[i].key, keys[i].keyAddr);
            }
        }
    }
    else
    {
        for (int i = 0; i < keys_index; ++i)
        {
            timeout(keys[i].key, keys[i].keyAddr);
        }
    }
    return EventHandlerResult::OK;
}

EventHandlerResult DynamicSuperKeys::onFocusEvent(const char *command)
{
    if (::Focus.handleHelp(command, "superkeys.map\nsuperkeys.waitfor\nsuperkeys.timeout\nsuperkeys.repeat\nsuperkeys.holdstart\nsuperkeys.overlap"))
        return EventHandlerResult::OK;

    if (strncmp_P(command, "superkeys.", 10) != 0) return EventHandlerResult::OK;

    if (strcmp_P(command + 10, "map") == 0)
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
    if (strcmp_P(command + 10, "waitfor") == 0)
    {
        if (::Focus.isEOL())
        {
            ::Focus.send(DynamicSuperKeys::wait_for_);
        }
        else
        {
            uint16_t wait = 0;
            ::Focus.read(wait);
            Runtime.storage().put(storage_base_ + 0, wait);
            Runtime.storage().commit();
            updateDynamicSuperKeysCache();
        }
    }
    if (strcmp_P(command + 10, "timeout") == 0)
    {
        if (::Focus.isEOL())
        {
            ::Focus.send(DynamicSuperKeys::time_out_);
        }
        else
        {
            uint16_t time = 0;
            ::Focus.read(time);
            Runtime.storage().put(storage_base_ + 2, time);
            Runtime.storage().commit();
            updateDynamicSuperKeysCache();
        }
    }
    if (strcmp_P(command + 10, "holdstart") == 0)
    {
        if (::Focus.isEOL())
        {
            ::Focus.send(DynamicSuperKeys::hold_start_);
        }
        else
        {
            uint16_t hold = 0;
            ::Focus.read(hold);
            Runtime.storage().put(storage_base_ + 4, hold); // Save default value 0.
            Runtime.storage().commit();
            updateDynamicSuperKeysCache();
        }
    }
    if (strcmp_P(command + 10, "repeat") == 0)
    {
        if (::Focus.isEOL())
        {
            ::Focus.send(DynamicSuperKeys::repeat_interval_);
        }
        else
        {
            uint8_t repeat = 0;
            ::Focus.read(repeat);
            Runtime.storage().put(storage_base_ + 6, repeat);
            Runtime.storage().commit();
            updateDynamicSuperKeysCache();
        }
    }
    if (strcmp_P(command + 10, "overlap") == 0)
    {
        if (::Focus.isEOL())
        {
            ::Focus.send(DynamicSuperKeys::overlap_threshold_);
        }
        else
        {
            uint8_t overlap = 0;
            ::Focus.read(overlap);
            Runtime.storage().put(storage_base_ + 7, overlap);
            Runtime.storage().commit();
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

//  ****************************** super-key handling ******************************

void DynamicSuperKeys::addKey(Key key, KeyAddr keyAddr, DynamicSuperKeys::SuperKeyState state)
{
    uint8_t index = key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;
    bool exists = false;
    for (int i = 0; i < keys_index; ++i)
    {
        if (keys[i].index == index)
        {
            exists = true;
            break;
        }
    }

    if (!exists)
    {
        KeyValue keyValue{};
        keyValue.index = index;
        keyValue.state = state;
        keyValue.key = key;
        keyValue.keyAddr = keyAddr;
        keyValue.state.start_time = Runtime.millisAtCycleStart();
        keyValue.state.has_modifier_in_action = false;
        keyValue.state.delayed_time = 0;
        keyValue.state.count = None;
        keyValue.state.has_already_send = false;
        keyValue.state.released = false;
        keyValue.state.is_layer_shifting = false;
        if (keys_index == MAX_SUPER_KEYS_ACTIVE) return;
        keys[keys_index] = keyValue;
        keys_index++;
    }
}

void DynamicSuperKeys::removeKey(Key key)
{
    /*         NRF_LOG_DEBUG("SK list antes del remove");
             NRF_LOG_DEBUG("*****************");
             for (const auto &element : keys)
            {
             NRF_LOG_DEBUG("[%i]",element.index);
            }
             NRF_LOG_DEBUG("*****************");*/
    uint8_t index = key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;
    for (int i = 0; i < keys_index; ++i)
    {
        if (keys[i].index == index)
        {
            // Now we have to remove and move the rest of them only if it is not the last element.
            keys_index--;
            memset(&keys[i], 0, sizeof(KeyValue));
            if (i != keys_index)
            {
                memmove(&keys[i], &keys[i + 1], (keys_index - i) * sizeof(KeyValue));
            }
            break;
        }
    }
    if (keys_index == 0)
    {
        last_super_key_ = Key_NoKey;
    }
    /*         NRF_LOG_DEBUG("SK list DESPUES del remove");
             NRF_LOG_DEBUG("*****************");
             for (const auto &element : keys)
            {
             NRF_LOG_DEBUG("[%i]",element.index);
            }
             NRF_LOG_DEBUG("*****************");
             NRF_LOG_FLUSH();*/
}

__attribute__((unused)) Key DynamicSuperKeys::findKey(uint8_t index)
{
    auto key_index = Key_NoKey;
    for (const auto &element : keys)
    {
        if (element.index == index)
        {
            key_index = element.key;
        }
    }
    return key_index;
}

void DynamicSuperKeys::updateKey(Key key, const DynamicSuperKeys::SuperKeyState &new_state)
{
    uint8_t index = key.getRaw() - ranges::DYNAMIC_SUPER_FIRST;
    for (int i = 0; i < keys_index; ++i)
    {
        if (keys[i].index == index)
        {
            keys[i].state = new_state;
            return;
        }
    }
}

bool DynamicSuperKeys::lowerKeyIsModifier(uint8_t key_id)
{
    switch (key_id)
    {
        case No_modifier:
        case Tab:
        case BlockMayus:
        case LCtrl:
        case LShift:
        case LAlt:
        case LWin:
        case RCrl:
        case RShift:
        case RAlt:
        case RWin:
            return true;
        default:
            return false;
    }
}

DynamicSuperKeys::KeyValue DynamicSuperKeys::getNextSuperKey()
{
    KeyValue key_value = {};

    if (keys_index != 0)
    {
        if (currentIndex < keys_index - 1)
        {
            currentIndex++;
        }
        else
        {
            currentIndex = 0;
        }

        key_value = keys[currentIndex];
    }
    else
    {
        key_value.key = Key_NoKey;
    }

    return key_value;
}

__attribute__((unused)) void DynamicSuperKeys::flush_superkeys()
{
    for (int i = 0; i < keys_index; ++i)
    {
        if (keys[i].state.is_layer_shifting && keys[i].state.has_modifier_in_action)
        {
            interrupt(keys[i].key, keys[i].keyAddr);
        }
    }
}

bool DynamicSuperKeys::releaseDelayed(uint16_t overlap_start, uint16_t overlap_end) const
{
    // We want to calculate the timeout by dividing the overlap duration by the
    // percentage required to make the superkey take on its alternate state. Since
    // we're doing integer arithmetic, we need to first multiply by 100, then
    // divide by the percentage value (as an integer). We use 32-bit integers
    // here to make sure it doesn't overflow when we multiply by 100.
    uint32_t overlap_duration = overlap_end - overlap_start;
    uint32_t release_timeout = (overlap_duration * 100) / overlap_threshold_;
    return !Runtime.hasTimeExpired(overlap_start, uint16_t(release_timeout));
}

} // namespace plugin
} //  namespace kaleidoscope

kaleidoscope::plugin::DynamicSuperKeys DynamicSuperKeys;