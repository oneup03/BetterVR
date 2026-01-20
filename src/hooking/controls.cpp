#include "cemu_hooks.h"
#include "../instance.h"
#include "openxr_motion_bridge.h"

constexpr float AXIS_THRESHOLD = 0.5f;
constexpr float HOLD_THRESHOLD = 0.1f;

Direction getJoystickDirection(const XrVector2f& stick)
{
    if (stick.y >= AXIS_THRESHOLD)       return Direction::Up;
    if (stick.y <= -AXIS_THRESHOLD)      return Direction::Down;
    if (stick.x <= -AXIS_THRESHOLD)      return Direction::Left;
    if (stick.x >= AXIS_THRESHOLD)       return Direction::Right;

    return Direction::None;
}


struct HandGestureState {
    bool isBehindHead;
    bool isBehindHeadWithWaistOffset;
    bool isCloseToHead;
    bool isCloseToMouth;
    bool isCloseToWaist;
    bool isNearChestHeight;
    bool isOnLeftSide;  // true = left side of body, false = right side
    bool isFarEnoughFromStoredPosition;
    float magnesisForwardAmount;
    float magnesisVerticalAmount;
};

int GetMagnesisForwardFrameInterval(float v)
{
    if (v <= 0.0f)    return 0;
    if (v <= 0.25f) return 10;
    if (v < 0.5f)   return 5;
    if (v < 0.75f)  return 2;
    return 1;
}

// Gesture detection functions
HandGestureState calculateHandGesture(
    OpenXR::GameState& gameState,
    const glm::fvec3& handPos,
    const glm::fmat4& headsetMatrix,
    const glm::fvec3& headsetPos,
    const bool ProcessStoredPositionDistanceCheck,
    const glm::fvec3& storedHandPos
) {
    HandGestureState gesture = {};
    
    // Calculate directional vectors
    glm::fvec3 headsetForward = -glm::normalize(glm::fvec3(headsetMatrix[2]));
    headsetForward.y = 0.0f;
    headsetForward = glm::normalize(headsetForward);
    
    glm::fvec3 headsetRight = glm::normalize(glm::fvec3(headsetMatrix[0]));
    glm::fvec3 headToHand = handPos - headsetPos;
    
    // Check if hand is behind head (for shoulder slots)
    float forwardDot = glm::dot(headsetForward, headToHand);
    gesture.isBehindHead = (forwardDot < 0.0f);
    
    // Check if hand is behind head (for waist slot) - uses flattened vectors
    glm::vec3 flatForward = glm::normalize(glm::vec3(headsetForward.x, 0.0f, headsetForward.z));
    glm::vec3 flatHandOffset = glm::vec3(headToHand.x, 0.0f, headToHand.z);
    
    constexpr float WAIST_BEHIND_OFFSET = 0.0f; //0.15f
    float flatForwardDot = glm::dot(flatForward, flatHandOffset) + WAIST_BEHIND_OFFSET;
    gesture.isBehindHeadWithWaistOffset = (flatForwardDot < 0.0f);
    
    // Check which side of body the hand is on
    float rightDot = glm::dot(headsetRight, headToHand);
    gesture.isOnLeftSide = (rightDot < 0.0f);
    
    // Check distance from head (for shoulder slots)
    constexpr float SHOULDER_RADIUS = 0.35f;
    constexpr float SHOULDER_RADIUS_SQ = SHOULDER_RADIUS * SHOULDER_RADIUS;
    gesture.isCloseToHead = (glm::length2(headToHand) < SHOULDER_RADIUS_SQ);

    // Check distance from head (for mouth slot)
    constexpr float MOUTH_RADIUS = 0.2f;
    constexpr float MOUTH_RADIUS_SQ = MOUTH_RADIUS * MOUTH_RADIUS;
    gesture.isCloseToMouth = (glm::length2(headToHand) < MOUTH_RADIUS_SQ);
    
    // Check distance from waist (rough estimate)
    glm::fvec3 waistPos = headsetPos - glm::fvec3(0.0f, 0.5f, 0.0f);
    gesture.isCloseToWaist = (handPos.y < waistPos.y);

    // Check hand height for shield (rough estimate)
    glm::fvec3 chestPos = headsetPos - glm::fvec3(0.0f, 0.3f, 0.0f);
    gesture.isNearChestHeight = (handPos.y > chestPos.y);

    // Check distance from stored position
    if (ProcessStoredPositionDistanceCheck) {
        constexpr float DISTANCE_THRESHOLD = 0.025f;
        constexpr float MAX_HAND_DISTANCE = 0.15f;
        auto delta = handPos - storedHandPos;
        auto distance = glm::length(delta);
        gesture.isFarEnoughFromStoredPosition = distance > DISTANCE_THRESHOLD;

        // Process magnesis gesture
        if (!gesture.isFarEnoughFromStoredPosition) {
            gesture.magnesisForwardAmount = gesture.magnesisVerticalAmount = 0.0f;
        }
        else {
            const glm::vec3 headsetUp(0.0f, 1.0f, 0.0f);
            auto forwardAmount = glm::dot(delta, headsetForward);
            auto verticalAmount = glm::dot(delta, headsetUp);

            auto remapSigned = [&](float value) {
                float sign = glm::sign(value);
                float absValue = glm::abs(value);

                float t = (absValue - DISTANCE_THRESHOLD) /
                          (MAX_HAND_DISTANCE - DISTANCE_THRESHOLD);

                t = glm::clamp(t, 0.0f, 1.0f);
                return t * sign;
            };

            gesture.magnesisForwardAmount = remapSigned(forwardAmount);
            gesture.magnesisVerticalAmount = remapSigned(verticalAmount);
            Log::print<INFO>("magnesisForwardAmount frames skip : {}", gameState.magnesisForwardFramesInterval);
            if (gameState.magnesisForwardFramesInterval > 0) {
                gesture.magnesisForwardAmount = 0.0f;
            }
            else if (gameState.magnesisForwardFramesInterval <= -1)
            {
                gameState.magnesisForwardFramesInterval = GetMagnesisForwardFrameInterval(glm::abs(gesture.magnesisForwardAmount));
            }
            
            gameState.magnesisForwardFramesInterval--;
            Log::print<INFO>("gesture.magnesisForwardAmount  : {}", gesture.magnesisForwardAmount);
            //
            //Log::print<INFO>("gesture.magnesisVerticalAmount  : {}", gesture.magnesisVerticalAmount);
        }
    }

    return gesture;
}

bool isHandOverLeftShoulderSlot(const HandGestureState& gesture) {
    return gesture.isBehindHead && gesture.isCloseToHead && gesture.isOnLeftSide;
}

bool isHandOverRightShoulderSlot(const HandGestureState& gesture) {
    return gesture.isBehindHead && gesture.isCloseToHead && !gesture.isOnLeftSide;
}

bool isHandOverLeftWaistSlot(const HandGestureState& gesture) {
    return gesture.isBehindHeadWithWaistOffset && gesture.isCloseToWaist && gesture.isOnLeftSide;
}

bool isHandOverRightWaistSlot(const HandGestureState& gesture) {
    return gesture.isBehindHeadWithWaistOffset && gesture.isCloseToWaist && !gesture.isOnLeftSide;
}

bool isHandOverMouthSlot(const HandGestureState& gesture) {
    return gesture.isCloseToMouth && !gesture.isBehindHead;
}

bool isHandFarEnoughFromStoredPosition(const HandGestureState& gesture) {
    return gesture.isFarEnoughFromStoredPosition;
}

bool handleDpadMenu(ButtonState::Event lastEvent, HandGestureState handGesture, uint32_t& buttonHold, OpenXR::GameState& gameState ) {
    if (lastEvent == ButtonState::Event::LongPress) {
        //Inverse the menu side for bows, so the menu corresponds to the hand side holding the weapon
        //eg : left hands hold bow, open bow menu with left shoulder, arrow menu with right shoulder
        bool isBowEquipped = gameState.last_item_held == EquipType::Bow;
        // Force a bow equip so the correct menu spawns even when the bow broke.
        bool doesBowJustBroke = isBowEquipped && gameState.left_equip_type != EquipType::Bow;
        if (doesBowJustBroke)
            buttonHold |= VPAD_BUTTON_ZR;
        if (isHandOverRightShoulderSlot(handGesture)) {
            buttonHold |= isBowEquipped ? VPAD_BUTTON_LEFT : VPAD_BUTTON_RIGHT;
            gameState.last_dpad_menu_open = isBowEquipped ? Direction::Left : Direction::Right;
        }
        else if (isHandOverLeftShoulderSlot(handGesture)) {
            buttonHold |= isBowEquipped ? VPAD_BUTTON_RIGHT : VPAD_BUTTON_LEFT;
            gameState.last_dpad_menu_open = isBowEquipped ? Direction::Right : Direction::Left;
        }
        // if not over shoulders slots, then it's over waist
        else {
            buttonHold |= VPAD_BUTTON_UP;
            gameState.last_dpad_menu_open = Direction::Up;
        }
        gameState.dpad_menu_open = true;
        return true;
    }
    return false;
}

// Input handling functions
void handleLeftHandInGameInput(
    uint32_t& buttonHold,
    OpenXR::InputState& inputs,
    OpenXR::GameState& gameState,
    const HandGestureState& leftGesture,
    Direction leftStickDir,
    XrActionStateVector2f& leftStickSource,
    XrActionStateVector2f& rightStickSource,
    const std::chrono::steady_clock::time_point& now
) {
    constexpr std::chrono::milliseconds INPUT_DELAY(400);
    
    RumbleParameters overSlotRumble = { true, 0, RumbleType::OscillationFallingSawtoothWave, 0.5f, true, 1.0, 0.25f, 0.25f };
    RumbleParameters grabSlotRumble = { true, 0, RumbleType::OscillationRaisingSawtoothWave, 0.5f, false, 0.25, 0.3f, 0.3f };
    
    auto* rumbleMgr = VRManager::instance().XR->GetRumbleManager();
    bool isGrabPressed = inputs.inGame.grabState[0].lastEvent == ButtonState::Event::ShortPress;
    bool isGrabPressedLong = inputs.inGame.grabState[0].lastEvent == ButtonState::Event::LongPress;
    bool isCurrentGrabPressed = inputs.inGame.grabState[0].wasDownLastFrame;
    
    // Handle shield
    // if shield with lock on isn't already being used with left trigger, use gesture to guard without lock on instead.
    // Gesture enabled only when both melee weapon and shield are in hands to prevent 2 handed weapons and quick drawing shield 
    // alone with Left Trigger to trigger it. So people can still move hands freely without the shield appearing when not wanted.
    if (!inputs.inGame.useLeftItem.currentState && gameState.left_equip_type == EquipType::Shield && gameState.right_equip_type == EquipType::Melee && (leftGesture.isNearChestHeight)) {
        buttonHold |= VPAD_BUTTON_ZL;
        rightStickSource.currentState.y = 0.2f; // Force disable the lock on view when holding shield
        gameState.is_shield_guarding = true;
        if ((gameState.previous_button_hold & VPAD_BUTTON_ZL) == 0)
            rumbleMgr->enqueueInputsRumbleCommand(grabSlotRumble);
    }
    else if (!inputs.inGame.useLeftItem.currentState)
        gameState.is_shield_guarding = false;

    if (!gameState.is_shield_guarding && gameState.previous_button_hold & VPAD_BUTTON_ZL)
        rumbleMgr->enqueueInputsRumbleCommand(grabSlotRumble);

    // Handle Parry gesture
    auto handVelocity = glm::length(ToGLM(inputs.inGame.poseVelocity[0].linearVelocity));
    if (handVelocity > 4.0f && gameState.left_equip_type == EquipType::Shield && leftGesture.isNearChestHeight) {
        buttonHold |= VPAD_BUTTON_A;
        rumbleMgr->enqueueInputsRumbleCommand(grabSlotRumble);
    }

    // Handle shoulder slot interactions
    if (isHandOverLeftShoulderSlot(leftGesture) || isHandOverRightShoulderSlot(leftGesture)) {
        // Provide haptic feedback when hand is over equipped slot
        if (isHandOverRightShoulderSlot(leftGesture) && gameState.right_equip_type == EquipType::Melee) {
            rumbleMgr->enqueueInputsRumbleCommand(overSlotRumble);
        }
        else if (isHandOverLeftShoulderSlot(leftGesture) && gameState.left_equip_type == EquipType::Bow) {
            rumbleMgr->enqueueInputsRumbleCommand(overSlotRumble);
        }
        if (handleDpadMenu(inputs.inGame.grabState[0].lastEvent, leftGesture, buttonHold, gameState))
            // Don't process normal input when opening dpad menu
            return;

        // Handle equip/unequip
        if (!gameState.prevent_grab_inputs && isGrabPressed) {
            rumbleMgr->enqueueInputsRumbleCommand(grabSlotRumble);
            
            if (isHandOverLeftShoulderSlot(leftGesture)) {
                // Left shoulder = Bow
                if (gameState.left_equip_type != EquipType::Bow) {
                    buttonHold |= VPAD_BUTTON_ZR;
                    gameState.last_item_held = EquipType::Bow;
                } else {
                    buttonHold |= VPAD_BUTTON_B;  // Unequip
                }
            }
            else {
                // Right shoulder = Melee weapon
                if (gameState.right_equip_type != EquipType::Melee) {
                    buttonHold |= VPAD_BUTTON_Y;
                    gameState.last_item_held = EquipType::Melee;
                } else {
                    buttonHold |= VPAD_BUTTON_B;  // Unequip
                }
            }
            
            gameState.prevent_grab_inputs = true;
            gameState.prevent_grab_time = now;
        }
        return;  // Don't process normal input when over slots
    }
    
    // Handle waist slot interaction (Rune)
    if (isHandOverLeftWaistSlot(leftGesture)) {
        if (gameState.left_equip_type == EquipType::Rune) {
            rumbleMgr->enqueueInputsRumbleCommand(overSlotRumble);
        }
        
        // Handle dpad menu
        if (handleDpadMenu(inputs.inGame.grabState[0].lastEvent, leftGesture, buttonHold, gameState))
            // Don't process normal input when opening dpad menu
            return;

        if (!gameState.prevent_grab_inputs && isGrabPressed) {
            rumbleMgr->enqueueInputsRumbleCommand(grabSlotRumble);
            
            if (gameState.left_equip_type != EquipType::Rune) {
                buttonHold |= VPAD_BUTTON_L;
                gameState.last_item_held = EquipType::Rune;
            } else {
                buttonHold |= VPAD_BUTTON_B;  // Unequip
            }
            
            gameState.prevent_grab_inputs = true;
            gameState.prevent_grab_time = now;
        }
        return;
    }

    if (isHandOverRightWaistSlot(leftGesture))
    {
        if (isCurrentGrabPressed)
            rumbleMgr->enqueueInputsRumbleCommand(grabSlotRumble);
        //Handle drop action
        if (isGrabPressedLong) {
            inputs.inGame.drop_weapon[0] = true;
            gameState.prevent_grab_inputs = true;
            gameState.prevent_grab_time = now;
        }
        return;
    }
    
    // Normal hand input (cancels over slots rumble)
    rumbleMgr->stopInputsRumble(0, RumbleType::OscillationFallingSawtoothWave);
    
    if (isGrabPressed) {
        // Handle grab action
        if (!gameState.prevent_grab_inputs) {
            buttonHold |= VPAD_BUTTON_A;
        }
    }
}

void handleRightHandInGameInput(
    uint32_t& buttonHold,
    OpenXR::InputState& inputs,
    OpenXR::GameState& gameState,
    const HandGestureState& rightGesture,
    XrActionStateVector2f& rightStickSource,
    Direction rightStickDir,
    const std::chrono::steady_clock::time_point& now
) {
    constexpr std::chrono::milliseconds INPUT_DELAY(400);
    
    const static RumbleParameters overSlotRumble = { true, 1, RumbleType::OscillationFallingSawtoothWave, 0.5f, true, 1.0, 0.25f, 0.25f };
    const static RumbleParameters grabSlotRumble = { true, 1, RumbleType::OscillationRaisingSawtoothWave, 0.5f, false, 0.25, 0.3f, 0.3f };
    const static RumbleParameters raiseRumble = { true, 1, RumbleType::Raising, 0.5f, false, 1.0, 0.1f, 0.1f };

    auto* rumbleMgr = VRManager::instance().XR->GetRumbleManager();
    bool isGrabPressedShort = inputs.inGame.grabState[1].lastEvent == ButtonState::Event::ShortPress;
    bool isGrabPressedLong = inputs.inGame.grabState[1].lastEvent == ButtonState::Event::LongPress;
    bool isCurrentGrabPressed = inputs.inGame.grabState[1].wasDownLastFrame;
    bool isTriggerPressed = inputs.inGame.useRightItem.currentState;
    
    // Handle shoulder slot interactions
    if (isHandOverLeftShoulderSlot(rightGesture) || isHandOverRightShoulderSlot(rightGesture)) {
        // Haptic feedback when over equipped slot
        if (isHandOverRightShoulderSlot(rightGesture) && gameState.right_equip_type == EquipType::Melee) {
            rumbleMgr->enqueueInputsRumbleCommand(overSlotRumble);
        }
        else if (isHandOverLeftShoulderSlot(rightGesture) && gameState.left_equip_type == EquipType::Bow) {
            rumbleMgr->enqueueInputsRumbleCommand(overSlotRumble);
        }

        // Handle dpad menu
        if (handleDpadMenu(inputs.inGame.grabState[1].lastEvent, rightGesture, buttonHold, gameState))
            // Don't process normal input when opening dpad menu
            return;

        // Handle equip/unequip
        if (!gameState.prevent_grab_inputs && isGrabPressedShort) {
            rumbleMgr->enqueueInputsRumbleCommand(grabSlotRumble);
            
            if (isHandOverRightShoulderSlot(rightGesture)) {
                // Right shoulder = Melee weapon
                if (gameState.right_equip_type != EquipType::Melee) {
                    buttonHold |= VPAD_BUTTON_Y;
                    gameState.last_item_held = EquipType::Melee;
                } else {
                    buttonHold |= VPAD_BUTTON_B;
                }
            }
            else {
                // Left shoulder = Bow
                if (gameState.left_equip_type != EquipType::Bow) {
                    buttonHold |= VPAD_BUTTON_ZR;
                    gameState.last_item_held = EquipType::Bow;
                } else {
                    buttonHold |= VPAD_BUTTON_B;
                }
            }
            
            gameState.prevent_grab_inputs = true;
            gameState.prevent_grab_time = now;
        }
        
        // Handle weapon throw when over shoulder slot
        if (isTriggerPressed) {
            rumbleMgr->stopInputsRumble(1, RumbleType::OscillationFallingSawtoothWave);
            rumbleMgr->enqueueInputsRumbleCommand(raiseRumble);
            buttonHold |= VPAD_BUTTON_R;
        }
        
        return;  // Don't process normal input when over slots
    }

    // Handle waist slot interaction (Rune)
    if (isHandOverLeftWaistSlot(rightGesture)) {
        if (gameState.left_equip_type == EquipType::Rune) {
            rumbleMgr->enqueueInputsRumbleCommand(overSlotRumble);
        }
        
        // Handle dpad menu
        if (handleDpadMenu(inputs.inGame.grabState[1].lastEvent, rightGesture, buttonHold, gameState))
            // Don't process normal input when opening dpad menu
            return;

        if (!gameState.prevent_grab_inputs && isGrabPressedShort) {
            rumbleMgr->enqueueInputsRumbleCommand(grabSlotRumble);
            
            if (gameState.left_equip_type != EquipType::Rune) {
                buttonHold |= VPAD_BUTTON_L;
                gameState.last_item_held = EquipType::Rune;
            } else {
                buttonHold |= VPAD_BUTTON_B;  // Unequip
            }
            
            gameState.prevent_grab_inputs = true;
            gameState.prevent_grab_time = now;
        }
        return;
    }

    if (isHandOverRightWaistSlot(rightGesture))
    {
        if (isCurrentGrabPressed)
            rumbleMgr->enqueueInputsRumbleCommand(grabSlotRumble);
        //Handle drop action
        if (isGrabPressedLong) {
            inputs.inGame.drop_weapon[1] = true;
            gameState.prevent_grab_inputs = true;
            gameState.prevent_grab_time = now;
        }
        return;
    }
    
    // Normal hand input (Cancel over slots rumbles)
    rumbleMgr->stopInputsRumble(1, RumbleType::OscillationFallingSawtoothWave);
    
    if (isCurrentGrabPressed) {
        if (gameState.left_equip_type == EquipType::Rune) {
            if (!gameState.right_hand_position_stored) {
                gameState.stored_right_hand_position = ToGLM(inputs.inGame.poseLocation[1].pose.position);
                gameState.right_hand_position_stored = true;
            }

            if (gameState.right_hand_position_stored) {
                rightStickSource.currentState.y = rightGesture.magnesisVerticalAmount;
                if (rightGesture.magnesisForwardAmount > 0.0f)
                    buttonHold |= VPAD_BUTTON_UP;
                else if (rightGesture.magnesisForwardAmount < 0.0f)
                    buttonHold |= VPAD_BUTTON_DOWN;
            }
        }
    }
    else
        gameState.right_hand_position_stored = false;

    if (isGrabPressedShort) {
        // Handle grab action
        if (!gameState.prevent_grab_inputs) {
            buttonHold |= VPAD_BUTTON_A;
        }
    }
}

void handleLeftTriggerBindings(
    uint32_t& buttonHold,
    OpenXR::InputState& inputs,
    OpenXR::GameState& gameState,
    HandGestureState leftGesture
) {
    if (!inputs.inGame.useLeftItem.currentState) {
        // reset lock on state when trigger is released
        gameState.is_locking_on_target = false;
        return;
    }

    RumbleParameters raiseRumble = { true, 0, RumbleType::Raising, 0.5f, false, 0.25, 0.3f, 0.3f };
    RumbleParameters fallRumble = { true, 0, RumbleType::Falling, 0.5f, false, 0.25, 0.3f, 0.3f };

    auto* rumbleMgr = VRManager::instance().XR->GetRumbleManager();

    //Guard + lock on
    // Reset the guard state to trigger again the lock on camera
    if (!gameState.is_locking_on_target && gameState.previous_button_hold & VPAD_BUTTON_ZL) {
        // cancel rune use to let the shield guard happen
        if (gameState.left_equip_type == EquipType::Rune) {
            rumbleMgr->enqueueInputsRumbleCommand(raiseRumble);
            buttonHold |= VPAD_BUTTON_B; // Cancel rune
        }
        buttonHold &= ~VPAD_BUTTON_ZL;
    }

    else {
        if (!gameState.is_locking_on_target)
            rumbleMgr->enqueueInputsRumbleCommand(raiseRumble);
        buttonHold |= VPAD_BUTTON_ZL;
        gameState.is_locking_on_target = true;
        gameState.is_shield_guarding = true;
    }    
}

void handleRightTriggerBindings(
    uint32_t& buttonHold,
    OpenXR::InputState& inputs,
    OpenXR::GameState& gameState
) {
    if (!inputs.inGame.useRightItem.currentState) {
        return;
    }
    
    const static RumbleParameters rightRumbleFixed = { true, 1, RumbleType::Fixed, 0.5f, false, 0.25, 0.3f, 0.3f };
    RumbleParameters rightRumbleRaise = { true, 1, RumbleType::Raising, 0.5f, false, 2.0, 0.01f, 0.01f };
    const static RumbleParameters leftRumbleFixed = { true, 0, RumbleType::Fixed, 0.5f, false, 0.25, 0.3f, 0.3f };
    auto* rumbleMgr = VRManager::instance().XR->GetRumbleManager();
    
    if (gameState.has_something_in_hand) {
        if (gameState.is_throwable_object_held) {
            rumbleMgr->enqueueInputsRumbleCommand(rightRumbleFixed);
            buttonHold |= VPAD_BUTTON_R;  // Throw object
        }
        else if (gameState.left_equip_type == EquipType::Bow) {
            rumbleMgr->enqueueInputsRumbleCommand(rightRumbleRaise);
            buttonHold |= VPAD_BUTTON_ZR;  // Shoot bow
        }
        else if (gameState.left_equip_type == EquipType::Rune) {
            buttonHold |= VPAD_BUTTON_A; // Use rune
            rumbleMgr->enqueueInputsRumbleCommand(leftRumbleFixed);
        }
        else {
            rumbleMgr->enqueueInputsRumbleCommand(rightRumbleRaise);
            buttonHold |= VPAD_BUTTON_Y;  // Melee attack
        }
    }
    else {
        // Re-equip last held weapon/item when empty-handed
        if (gameState.last_item_held == EquipType::Melee) {
            buttonHold |= VPAD_BUTTON_Y;
            rumbleMgr->enqueueInputsRumbleCommand(rightRumbleFixed);
        }
        else if (gameState.last_item_held == EquipType::Bow) {
            buttonHold |= VPAD_BUTTON_ZR;
            rumbleMgr->enqueueInputsRumbleCommand(rightRumbleFixed);
        }
        else if (gameState.last_item_held == EquipType::Rune) {
            buttonHold |= VPAD_BUTTON_L;
            if ((gameState.previous_button_hold & VPAD_BUTTON_L) == 0) {
                rumbleMgr->enqueueInputsRumbleCommand(leftRumbleFixed);
                rumbleMgr->enqueueInputsRumbleCommand(rightRumbleFixed);
            }
        }
    }
}

void handleMenuInput(
    uint32_t& buttonHold,
    OpenXR::InputState& inputs,
    OpenXR::GameState& gameState,
    Direction leftStickDir
) {
    auto mapButton = [](XrActionStateBoolean& state, VPADButtons btn) -> uint32_t {
        return state.currentState ? btn : 0;
    };
    
    if (!gameState.prevent_inputs) {
        buttonHold |= mapButton(inputs.inMenu.back, VPAD_BUTTON_B);
        if (gameState.map_open)
            buttonHold |= mapButton(inputs.inMenu.map, VPAD_BUTTON_MINUS);
        else
            buttonHold |= mapButton(inputs.inMenu.inventory, VPAD_BUTTON_PLUS);
    }

    buttonHold |= mapButton(inputs.inMenu.select, VPAD_BUTTON_A);
    buttonHold |= mapButton(inputs.inMenu.hold, VPAD_BUTTON_X);
    buttonHold |= mapButton(inputs.inMenu.leftTrigger, VPAD_BUTTON_L);
    buttonHold |= mapButton(inputs.inMenu.rightTrigger, VPAD_BUTTON_R);

    // handle optional quick rune menu
    if (gameState.rune_menu_open) {
        if (inputs.inMenu.sort.currentState)
            buttonHold |= VPAD_BUTTON_UP;
        else
            gameState.rune_menu_open = false;
    }
    else
        buttonHold |= mapButton(inputs.inMenu.sort, VPAD_BUTTON_Y);

}

void CemuHooks::hook_InjectXRInput(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    auto mapXRButtonToVpad = [](XrActionStateBoolean& state, VPADButtons mapping) -> uint32_t {
        return state.currentState ? mapping : 0;
    };

    // read existing vpad as to not overwrite it
    uint32_t vpadStatusOffset = hCPU->gpr[4];
    VPADStatus vpadStatus = {};

    auto* renderer = VRManager::instance().XR->GetRenderer();
    // todo: revert this to unblock gamepad input
    if (!(renderer && renderer->m_imguiOverlay && renderer->m_imguiOverlay->ShouldBlockGameInput())) {
        readMemory(vpadStatusOffset, &vpadStatus);
    }

    // fetch input state
    OpenXR::InputState inputs = VRManager::instance().XR->m_input.load();
    inputs.inGame.drop_weapon[0] = inputs.inGame.drop_weapon[1] = false;

    // fetch game state
    auto gameState = VRManager::instance().XR->m_gameState.load(); 
    gameState.in_game = inputs.inGame.in_game;

    // use the previous values if no new values are written from hook_ChangeWeaponMtx this frame
    if (gameState.has_something_in_hand)
    {
        if (!gameState.left_equip_type_set_this_frame) gameState.left_equip_type = gameState.previous_left_equip_type;
        if (!gameState.right_equip_type_set_this_frame) gameState.right_equip_type = gameState.previous_right_equip_type;
    }
  
    // buttons
    static uint32_t oldCombinedHold = 0; 
    uint32_t newXRBtnHold = 0;

    // fetching stick inputs
    XrActionStateVector2f& leftStickSource = gameState.in_game ? inputs.inGame.move : inputs.inMenu.navigate;
    XrActionStateVector2f& rightStickSource = gameState.in_game ? inputs.inGame.camera : inputs.inMenu.scroll;

    Direction leftJoystickDir = getJoystickDirection(leftStickSource.currentState);
    Direction rightJoystickDir = getJoystickDirection(rightStickSource.currentState);

    //Delay to wait before allowing specific inputs again
    constexpr std::chrono::milliseconds delay{ 400 };
    const auto now = std::chrono::steady_clock::now();

    // check if we need to prevent inputs from happening
    if (gameState.in_game != gameState.was_in_game) {
        gameState.prevent_inputs = true;
        gameState.prevent_inputs_time = now;
    }

    if (gameState.prevent_inputs && now >= gameState.prevent_inputs_time + delay)
        gameState.prevent_inputs = false;

    if (gameState.prevent_grab_inputs && now >= gameState.prevent_grab_time + delay)
        gameState.prevent_grab_inputs = false;

    //Log::print<INFO>("last_weapon_held_hand : {}", gameState.last_weapon_held_hand);
    //Log::print<INFO>("Is weapon held : {}", gameState.is_weapon_or_object_held);
    //Log::print<INFO>("Weapon Type : {}", (int)gameState.left_weapon_type);
    //Log::print<INFO>("Weapon Type : {}", (int)gameState.right_weapon_type);

    // Calculate hand gestures
    HandGestureState leftGesture = {};
    HandGestureState rightGesture = {};

    auto headsetPose = VRManager::instance().XR->GetRenderer()->GetMiddlePose();
    if (headsetPose.has_value()) {
        const auto headsetMtx = headsetPose.value();
        const glm::fvec3 headsetPos(headsetMtx[3]);
        
        const auto leftHandPos = ToGLM(inputs.inGame.poseLocation[0].pose.position);
        const auto rightHandPos = ToGLM(inputs.inGame.poseLocation[1].pose.position);
        
        leftGesture = calculateHandGesture(gameState, leftHandPos, headsetMtx, headsetPos, gameState.left_hand_position_stored, gameState.stored_left_hand_position);
        rightGesture = calculateHandGesture(gameState, rightHandPos, headsetMtx, headsetPos, gameState.right_hand_position_stored, gameState.stored_right_hand_position);
    }
    
    //dpad menu toggle
    if (gameState.dpad_menu_open)
    {
        switch (gameState.last_dpad_menu_open) {
            case Direction::Left:  newXRBtnHold |= VPAD_BUTTON_LEFT; break;
            case Direction::Right: newXRBtnHold |= VPAD_BUTTON_RIGHT; break;
            case Direction::Up: newXRBtnHold |= VPAD_BUTTON_UP; break;
            default: break;
        }

        if (inputs.inMenu.select.currentState || inputs.inMenu.back.currentState) // need to add a way to quit dpad menu by pressing again grips
        {
            gameState.dpad_menu_open = false;
            gameState.last_dpad_menu_open = Direction::None;
            //// prevents the arrow shoot on menu quit from the force equip bow input (see handleDpadMenu())
            //newXRBtnHold |= VPAD_BUTTON_B;
        }
    }

    // Process input
    if (gameState.in_game) {
        // null right joystick Y to let the magnesis motion controls handle it.
        rightStickSource.currentState.y = 0.0f;

        if (!gameState.prevent_inputs) {
            // prevent jump when exiting menus with B button
            newXRBtnHold |= mapXRButtonToVpad(inputs.inGame.jump, VPAD_BUTTON_X);

            // Scope
            if (inputs.inGame.map_scopeState.lastEvent == ButtonState::Event::LongPress) {
                newXRBtnHold |= VPAD_BUTTON_STICK_R;
            }

            // Handle map and inventory menu toggle
            if (inputs.inGame.map_scopeState.lastEvent == ButtonState::Event::ShortPress) {
                newXRBtnHold |= VPAD_BUTTON_MINUS;
                gameState.map_open = true;
            }
            if (inputs.inGame.inventory.currentState) {
                newXRBtnHold |= VPAD_BUTTON_PLUS;
                gameState.map_open = false;
            }
        }

        newXRBtnHold |= mapXRButtonToVpad(inputs.inGame.crouch, VPAD_BUTTON_STICK_L);
        
        // Optional rune inputs (for seated players)
        if (inputs.inGame.useRune_runeMenuState.lastEvent == ButtonState::Event::LongPress) {
            gameState.rune_menu_open = true;
            newXRBtnHold |= VPAD_BUTTON_UP;  // Rune quick menu
        }
        if (inputs.inGame.useRune_runeMenuState.lastEvent == ButtonState::Event::ShortPress) {
            newXRBtnHold |= VPAD_BUTTON_L;  // Use rune
            gameState.last_item_held = EquipType::Rune;
        }
        
        if (inputs.inGame.runState.lastEvent == ButtonState::Event::LongPress) {
            newXRBtnHold |= VPAD_BUTTON_B;  // Run
        }
        else
            newXRBtnHold |= mapXRButtonToVpad(inputs.inGame.run_interact_cancel, VPAD_BUTTON_A);
        
        // Wistle gesture
        if (isHandOverMouthSlot(leftGesture) && isHandOverMouthSlot(rightGesture)) {
            if (inputs.inGame.grabState[0].wasDownLastFrame && inputs.inGame.grabState[1].wasDownLastFrame) {
                RumbleParameters rumble = { true, 0, RumbleType::OscillationRaisingSawtoothWave, 1.0f, false, 0.25, 0.2f, 0.2f };
                VRManager::instance().XR->GetRumbleManager()->enqueueInputsRumbleCommand(rumble);
                newXRBtnHold |= VPAD_BUTTON_DOWN;
            }
        }
        
        // Hand-specific input
        handleLeftHandInGameInput(newXRBtnHold, inputs, gameState, leftGesture, 
                                   leftJoystickDir, leftStickSource, rightStickSource, now);
        handleRightHandInGameInput(newXRBtnHold, inputs, gameState, rightGesture, rightStickSource,
                                    rightJoystickDir, now);
        
        // Trigger handling
        handleLeftTriggerBindings(newXRBtnHold, inputs, gameState, leftGesture);
        handleRightTriggerBindings(newXRBtnHold, inputs, gameState);
        
        //// Rune cancel with left trigger
        //if (inputs.inGame.leftTrigger.currentState && 
        //    gameState.left_equip_type == EquipType::Rune) {
        //    RumbleParameters rumble = { true, 1, 0.5f, false, 0.25, 0.3f, 0.3f };
        //    VRManager::instance().XR->GetRumbleManager()->enqueueInputsRumbleCommand(rumble);
        //    newXRBtnHold |= VPAD_BUTTON_B;
        //}
    }
    else {
        handleMenuInput(newXRBtnHold, inputs, gameState, leftJoystickDir);
    }
    
    // Update rumble/haptics
    VRManager::instance().XR->GetRumbleManager()->updateHaptics();



    // sticks
    static uint32_t oldXRStickHold = 0;
    uint32_t newXRStickHold = 0;

    // movement/navigation stick
    if (inputs.inGame.in_game) {
        auto isolateYaw = [](const glm::fquat& q) -> glm::fquat {
            glm::vec3 euler = glm::eulerAngles(q);
            euler.x = 0.0f;
            euler.z = 0.0f;
            return glm::angleAxis(euler.y, glm::vec3(0, 1, 0));
        };

        glm::fquat controllerRotation = ToGLM(inputs.inGame.poseLocation[OpenXR::EyeSide::LEFT].pose.orientation);
        glm::fquat controllerYawRotation = isolateYaw(controllerRotation);

        glm::fquat moveRotation = inputs.inGame.pose[OpenXR::EyeSide::LEFT].isActive ? glm::inverse(VRManager::instance().XR->m_inputCameraRotation.load() * controllerYawRotation) : glm::identity<glm::fquat>();

        glm::vec3 localMoveVec(leftStickSource.currentState.x, 0.0f, leftStickSource.currentState.y);

        glm::vec3 worldMoveVec = moveRotation * localMoveVec;

        float inputLen = glm::length(glm::vec2(leftStickSource.currentState.x, leftStickSource.currentState.y));
        if (inputLen > 1e-3f) {
            worldMoveVec = glm::normalize(worldMoveVec) * inputLen;
        }
        else {
            worldMoveVec = glm::vec3(0.0f);
        }

        worldMoveVec = {0, 0, 0};

        vpadStatus.leftStick = { worldMoveVec.x + leftStickSource.currentState.x + vpadStatus.leftStick.x.getLE(), worldMoveVec.z + leftStickSource.currentState.y + vpadStatus.leftStick.y.getLE() };
    }
    else {
        vpadStatus.leftStick = { leftStickSource.currentState.x + vpadStatus.leftStick.x.getLE(), leftStickSource.currentState.y + vpadStatus.leftStick.y.getLE() };
    }

    if (leftStickSource.currentState.x <= -AXIS_THRESHOLD || (HAS_FLAG(oldXRStickHold, VPAD_STICK_L_EMULATION_LEFT) && leftStickSource.currentState.x <= -HOLD_THRESHOLD))
        newXRStickHold |= VPAD_STICK_L_EMULATION_LEFT;
    else if (leftStickSource.currentState.x >= AXIS_THRESHOLD || (HAS_FLAG(oldXRStickHold, VPAD_STICK_L_EMULATION_RIGHT) && leftStickSource.currentState.x >= HOLD_THRESHOLD))
        newXRStickHold |= VPAD_STICK_L_EMULATION_RIGHT;

    if (leftStickSource.currentState.y <= -AXIS_THRESHOLD || (HAS_FLAG(oldXRStickHold, VPAD_STICK_L_EMULATION_DOWN) && leftStickSource.currentState.y <= -HOLD_THRESHOLD))
        newXRStickHold |= VPAD_STICK_L_EMULATION_DOWN;
    else if (leftStickSource.currentState.y >= AXIS_THRESHOLD || (HAS_FLAG(oldXRStickHold, VPAD_STICK_L_EMULATION_UP) && leftStickSource.currentState.y >= HOLD_THRESHOLD))
        newXRStickHold |= VPAD_STICK_L_EMULATION_UP;

    vpadStatus.rightStick = {rightStickSource.currentState.x + vpadStatus.rightStick.x.getLE(), rightStickSource.currentState.y + vpadStatus.rightStick.y.getLE()};

    if (rightStickSource.currentState.x <= -AXIS_THRESHOLD || (HAS_FLAG(oldXRStickHold, VPAD_STICK_R_EMULATION_LEFT) && rightStickSource.currentState.x <= -HOLD_THRESHOLD))
        newXRStickHold |= VPAD_STICK_R_EMULATION_LEFT;
    else if (rightStickSource.currentState.x >= AXIS_THRESHOLD || (HAS_FLAG(oldXRStickHold, VPAD_STICK_R_EMULATION_RIGHT) && rightStickSource.currentState.x >= HOLD_THRESHOLD))
        newXRStickHold |= VPAD_STICK_R_EMULATION_RIGHT;

    if (rightStickSource.currentState.y <= -AXIS_THRESHOLD || (HAS_FLAG(oldXRStickHold, VPAD_STICK_R_EMULATION_DOWN) && rightStickSource.currentState.y <= -HOLD_THRESHOLD))
        newXRStickHold |= VPAD_STICK_R_EMULATION_DOWN;
    else if (rightStickSource.currentState.y >= AXIS_THRESHOLD || (HAS_FLAG(oldXRStickHold, VPAD_STICK_R_EMULATION_UP) && rightStickSource.currentState.y >= HOLD_THRESHOLD))
        newXRStickHold |= VPAD_STICK_R_EMULATION_UP;

    oldXRStickHold = newXRStickHold;

    // calculate new hold, trigger and release
    uint32_t combinedHold = (vpadStatus.hold.getLE() | (newXRBtnHold | newXRStickHold));
    vpadStatus.hold = combinedHold;
    vpadStatus.trig = (combinedHold & ~oldCombinedHold);
    vpadStatus.release = (~combinedHold & oldCombinedHold);
    oldCombinedHold = combinedHold;

    // misc
    vpadStatus.vpadErr = 0;
    vpadStatus.batteryLevel = 0xC0;

    // touch
    vpadStatus.tpData.touch = 0;
    vpadStatus.tpData.validity = 3;

    // motion
    OpenXRMotionBridge::UpdateVPADStatus(inputs, vpadStatus);

    // write the input back to VPADStatus
    writeMemory(vpadStatusOffset, &vpadStatus);

    // set r3 to 1 for hooked VPADRead function to return success
    hCPU->gpr[3] = 1;

    // (re)set values for next frame
    gameState.previous_button_hold = newXRBtnHold;
    gameState.was_in_game = gameState.in_game;
    gameState.has_something_in_hand = false; // updated in hook_ChangeWeaponMtx
    gameState.is_throwable_object_held = false; // updated in hook_ChangeWeaponMtx
    gameState.previous_left_equip_type = gameState.left_equip_type; // use the previous values if no new values are written from hook_ChangeWeaponMtx this frame
    gameState.previous_right_equip_type = gameState.right_equip_type; // use the previous values if no new values are written from hook_ChangeWeaponMtx this frame
    gameState.left_equip_type = EquipType::None; // updated in hook_ChangeWeaponMtx
    gameState.right_equip_type = EquipType::None; // updated in hook_ChangeWeaponMtx
    gameState.left_equip_type_set_this_frame = false; // updated in hook_ChangeWeaponMtx
    gameState.right_equip_type_set_this_frame = false; // updated in hook_ChangeWeaponMtx

    VRManager::instance().XR->m_gameState.store(gameState);
    VRManager::instance().XR->m_input.store(inputs);
}


// some ideas:
// - quickly pressing the grip button without a weapon while there's a nearby weapon and there's enough slots = pick up weapon
// - holding the grip button without a weapon while there's a nearby weapon = temporarily hold weapon
// - holding the grip button a weapon equipped = opens weapon dpad menu
// - quickly press the grip button while holding a weapon = drops current weapon

void CemuHooks::hook_CreateNewActor(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    // if (VRManager::instance().XR->GetRenderer() == nullptr || VRManager::instance().XR->GetRenderer()->m_layer3D.GetStatus() == RND_Renderer::Layer3D::Status3D::UNINITIALIZED) {
    //     hCPU->gpr[3] = 0;
    //     return;
    // }
    hCPU->gpr[3] = 0;

    // OpenXR::InputState inputs = VRManager::instance().XR->m_input.load();
    // if (!inputs.inGame.in_game) {
    //     hCPU->gpr[3] = 0;
    //     return;
    // }
    //
    // // test if controller is connected
    // if (inputs.inGame.grab[OpenXR::EyeSide::LEFT].currentState == XR_TRUE && inputs.inGame.grab[OpenXR::EyeSide::LEFT].changedSinceLastSync == XR_TRUE) {
    //     Log::print("Trying to spawn new thing!");
    //     hCPU->gpr[3] = 1;
    // }
    // else if (inputs.inGame.grab[OpenXR::EyeSide::RIGHT].currentState == XR_TRUE && inputs.inGame.grab[OpenXR::EyeSide::RIGHT].changedSinceLastSync == XR_TRUE) {
    //     Log::print("Trying to spawn new thing!");
    //     hCPU->gpr[3] = 1;
    // }
    // else {
    //     hCPU->gpr[3] = 0;
    // }
}