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

#define strcmp_P(a, b) strcmp((a), (b))
#define strncmp_P(s1, s2, n) strncmp((s1), (s2), (n))

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

            typedef enum
            {
                No_modifier,
                Tab = 43,
                BlockMayus = 57,
                LCtrl = 224,
                LShift,
                LAlt,
                LWin,
                RCrl,
                RShift,
                RAlt,
                RWin,
            } Modifiers;
            DynamicSuperKeys() {}

            typedef enum : uint16_t {
                ALPHA_KEYS = 256,
                LED_BUTTONS_FIRST = 17152,
                PREVIOUS_LED_EFFECT,
                LED_BUTTONS_LAST,
                LAYER_SHIFT_FIRST = 17450,
                LAYER_SHIFT_LAST = 17459,
                LAYER_LOCK_FIRST = 17492,
                LAYER_LOCK_LAST = 17501
            };

            static bool SuperKeys(uint8_t tap_dance_index, KeyAddr key_addr, DynamicSuperKeys::SuperType tap_count, DynamicSuperKeys::ActionType tap_dance_action);
            /**
                   * Handle a key switch event and perform SuperKeys processing.
                   *
                   * This method is responsible for handling key switch events and performing SuperKeys processing when applicable.
                   * It checks whether a key switch event corresponds to a physical key, ignores injected keys from other plugins,
                   * and handles normal key events. It also manages the release of configured tap or held keys when modifiers are pressed or released.
                   *
                   * @param mapped_key The mapped key associated with the key switch event.
                   * @param key_addr The address of the key for the key switch event.
                   * @param keyState The state of the key switch event.
                   * @return An EventHandlerResult indicating the result of the event handling.
                 */
            EventHandlerResult onKeyswitchEvent(Key &mapped_key, KeyAddr key_addr, uint8_t keyState);
            /**
                   * Handle focus events and configuration commands for DynamicSuperKeys.
                   *
                   * This method processes focus events and configuration commands related to DynamicSuperKeys.
                   * It allows you to manage and modify various settings for SuperKeys, such as key mappings, wait times, timeouts, hold durations, and overlap thresholds.
                   *
                   * @param command A character array representing the command for configuration.
                   * @return An EventHandlerResult indicating the result of the event handling.
                */
            EventHandlerResult onFocusEvent(const char *command);
            /**
                   * Prepare DynamicSuperKeys state before reporting key events.
                   *
                   * This method is responsible for managing the state of DynamicSuperKeys just before reporting key events.
                   * It handles releasing SuperKeys and applying time-outs if necessary.
                   * Additionally, it ensures that fast key releases and the state of the keys are appropriately managed.
                   *
                   * @return An EventHandlerResult indicating the result of the operation.
                 */
            EventHandlerResult beforeReportingState();
            /**
                 * Initialize DynamicSuperKeys with storage settings.
                 *
                 * This method sets up the DynamicSuperKeys by configuring storage parameters.
                 * It allocates a storage slice in EEPROM to store the DynamicSuperKeys settings, including size and offset.
                 * After setting up the storage, it updates the DynamicSuperKeys cache to ensure consistency with the stored values.
                 *
                 * @param dynamic_offset The offset for DynamicSuperKeys in the EEPROM storage.
                 * @param size The size of the storage slice required for DynamicSuperKeys settings.
                 */
            static void setup(uint8_t dynamic_offset, uint16_t size);

            struct SuperConfiguration
            {
                bool tap_set;
                bool hold_set;
                bool tapHold_set;
                bool doubleTap_set;
                bool doubleTapHold_set;
            };

            struct SuperKeyState
            {
                bool pressed : 1;
                bool triggered : 1;
                bool release_next : 1;
                bool holded : 1;
                bool printonrelease : 1;
                SuperType count;
                SuperConfiguration actions;
                bool has_modifier_in_action;
                uint16_t start_time;
                uint16_t delayed_time;
                bool has_already_send;
                bool is_layer_shifting;
                bool released;
                bool has_macros;
                bool is_qukey;
                uint32_t timeStamp;
            };

            struct KeyValue {
                uint8_t index;
                DynamicSuperKeys::SuperKeyState state;
                Key key;
                KeyAddr keyAddr;
            };
            static uint8_t overlap_threshold_;

        private:
            static constexpr uint8_t SUPER_KEY_COUNT = ranges::DYNAMIC_SUPER_LAST - ranges::DYNAMIC_SUPER_FIRST + 1;
            static constexpr uint8_t MINIMUM_KEYS_REQUIRES_IN_QUEUE = 2;
            static constexpr uint8_t SUPERKEY_MEMORY_STORAGE = 12;
            static SuperKeyState state_[SUPER_KEY_COUNT];
            static uint16_t map_[SUPER_KEY_COUNT];
            static uint16_t storage_base_;
            static uint16_t storage_size_;
            static uint8_t super_key_count_;
            static uint8_t offset_;
            static uint16_t delayed_time_;
            static KeyAddr modifier_key_addr_;
            static Key last_super_key_;
            static KeyAddr last_super_addr_;
            static bool modifier_pressed_;
            static bool layer_shifted_;
            static uint8_t layer_shifted_number_;
            static uint16_t time_out_;
            static uint16_t wait_for_;
            static uint16_t hold_start_;
            static uint8_t repeat_interval_;
            static bool fast_key_release;
            static uint8_t super_key_index;
            /**
                   * Update the Dynamic Super Keys cache.
                   *
                   * This method updates a cache of dynamic super keys. It retrieves various configuration parameters from storage and
                   * updates the internal state based on this information. It also processes key actions and manages the cache structure.
                   *
                   * The key configuration parameters (e.g., wait_for, time_out, hold_start, repeat_interval, and overlap_threshold) are
                   * retrieved from storage and used to update corresponding class variables. If certain conditions are met, these values
                   * are persisted in storage.
                   *
                   * The method iterates over stored data starting from a specified position and processes key actions. Depending on the
                   * value of the raw_key, it sets or clears specific actions in the state based on the action_count. The method also tracks
                   * super keys and their properties.
                   *
                   * @note This method assumes a specific structure and format of data in storage for proper operation.
                 */
            static void updateDynamicSuperKeysCache();
            /**
                   * Determine the next super key state based on the previous state and an action type.
                   *
                   * This method calculates the next super key state by considering the previous state and the action type (e.g., Tap or Hold).
                   * It is used to manage the transition between different super key states, facilitating multi-tap and hold key behavior.
                   *
                   * @param previous The previous super key state, represented as a DynamicSuperKeys::SuperType.
                   * @param action The action type, which can be either Tap or Hold, represented as a DynamicSuperKeys::ActionType.
                   *
                   * @return The next super key state, represented as a DynamicSuperKeys::SuperType.
                 */
            static SuperType ReturnType(DynamicSuperKeys::SuperType previous, DynamicSuperKeys::ActionType action);
            /**
                   * Perform a "tap" action on a specified key.
                   *
                   * This method simulates a "tap" action on the provided key, which is used to trigger certain key behaviors in dynamic super keys.
                   * It updates the internal state of the key, including the count of taps and the start time of the action, and then updates the key status.
                   *
                   * @param key The key on which the "tap" action is performed.
                 */
            static void tap(Key key);
            /**
                   * Perform a "hold" action on a specified key and key address.
                   *
                   * This method simulates a "hold" action on the provided key and key address, which is used to trigger certain key behaviors in dynamic super keys.
                   * It updates the internal state of the key, including the holded state, delayed time, count of holds, and trigger status. Depending on the current state of the key, it may trigger SuperKeys actions and update key status accordingly.
                   *
                   * @param key The key on which the "hold" action is performed.
                   * @param keyAddr The address of the key to which the "hold" action is applied.
                 */
            static void hold(Key key, const KeyAddr &keyAddr);
            /**
                   * @brief Handles a key interrupt event.
                   *
                   * This method is responsible for handling a key interrupt event within the dynamic super keys system.
                   * It determines whether a key press should result in a hold action, release action, or no action at all.
                   *
                   * @param key The key that generated the interrupt event.
                   *
                   * @return Returns `true` if the interrupt event was successfully handled, indicating no extended key hold;
                   *         returns `false` if an extended key hold was detected and a hold action was performed.
                 */
            static bool interrupt(Key key,const KeyAddr &keyAddr);
            /**
                   * Handle a key timeout event, which occurs when a key has been release for too long without a press event.
                   *
                   * This method is responsible for managing key timeout events. The method updates the internal state of the key, triggers SuperKeys
                   * actions if necessary, and performs cleanup tasks.
                   *
                   * @param key The key for which the timeout event is handled.
                   * @param keyAddr The address of the key to which the timeout event is associated.
                 */
            static void timeout(Key key, const KeyAddr &keyAddr);
            /**
                   * Handle a key release event.
                   *
                   * This method is responsible for managing key release events. It is called when a key is released after being held
                   * or tapped. The method updates the internal state of the key, triggers SuperKeys actions if necessary, and performs
                   * cleanup tasks such as resetting the key's state attributes.
                   *
                   * @param key The key for which the release event is handled.
                   * @param keyAddr The address of the key to which the release event is associated.
                 */
            static void release(Key key, const KeyAddr &keyAddr);
            /**
                   * Add a key to the list of tracked keys with associated state and properties.
                   *
                   * This method adds a key, along with its associated state and properties, to the list of tracked keys. It ensures that a
                   * key is not added more than once to the list, preventing duplicates. If the key is not found in the list, it is appended
                   * with the specified state, and various attributes are initialized for the key's state.
                   *
                   * @param key The key to be added to the list.
                   * @param keyAddr The address of the key.
                   * @param state The initial state associated with the key.
                 */
            static void addKey(Key key, KeyAddr keyAddr, SuperKeyState state);
            /**
                   * Remove a key from the list of tracked keys.
                   *
                   * This method removes a key from the list of tracked keys based on its key identifier (index). It ensures that the key is
                   * removed from the list and performs necessary cleanup. If the list of tracked keys becomes empty after the removal,
                   * it updates the last super key state to indicate no active super key.
                   *
                   * @param key The key to be removed from the list of tracked keys.
                 */
            static void removeKey(Key key);
            /**
                   * @brief Searches for a key in the collection and returns its index if found.
                   *
                   * This method searches for a key with the specified index in the collection
                   * and returns its index if found. If the key is not found, it returns 0.
                   *
                   * @param index The index of the key to search for.
                   * @return The index of the key if found, or 0 if not found.
                 */
            __attribute__((unused)) static Key findKey(uint8_t index);
            /**
                   * Check if a specified key ID corresponds to a modifier key.
                   *
                   * This method determines whether a given key ID corresponds to a modifier key or not. It checks the key ID against a list
                   * of known modifier key IDs and returns a boolean value indicating whether the key is a modifier key.
                   *
                   * @param key_id The key ID to be checked for being a modifier key.
                   * @return `true` if the key ID corresponds to a modifier key, `false` otherwise.
                 */
            static bool lowerKeyIsModifier(uint8_t key_id);
            /**
                   * Update the state of a tracked key with a new state.
                   *
                   * This method is used to update the state of a tracked key with a new state. It searches the list of tracked keys for the
                   * specified key, identified by its key identifier, and updates the state of the key with the provided new state.
                   *
                   * @param key The key whose state needs to be updated.
                   * @param new_state The new state to be assigned to the key.
                 */
            static void updateKey(Key key, const SuperKeyState &new_state);
            /**
                   * Retrieve the next tracked super key from the list of keys.
                   *
                   * This method retrieves the next tracked super key from the list of keys, cycling through the list in a circular manner.
                   * If there are no tracked keys, it returns a placeholder KeyValue with a Key_NoKey.
                   *
                   * @return A KeyValue struct representing the next tracked super key and its associated state.
                 */
            static KeyValue getNextSuperKey();

            static void flush_superkeys();

            bool releaseDelayed(uint16_t overlap_start, uint16_t overlap_end) const;
        };

    }
}

extern kaleidoscope::plugin::DynamicSuperKeys DynamicSuperKeys;