#include "../instance.h"
#include "cemu_hooks.h"
#include "hands.h"

std::array<WeaponMotionAnalyser, 2> CemuHooks::m_motionAnalyzers = {};

static void ModifyWeaponMtxToVRPose(OpenXR::EyeSide side, BEMatrix34& toBeAdjustedMtx, glm::fquat cameraRotation, glm::fvec3 cameraPosition) {
    OpenXR::InputState inputs = VRManager::instance().XR->m_input.load();
    //auto views = VRManager::instance().XR->GetRenderer()->GetMiddlePosePos();
    //if (!views.has_value()) {
    //    return;
    //}

    glm::fvec3 views = glm::fvec3(0, 0, 0);

    glm::fvec3 controllerPos = glm::fvec3(0.0f);
    glm::fquat controllerQuat = glm::identity<glm::fquat>();

    if (inputs.inGame.in_game && inputs.inGame.pose[side].isActive) {
        auto& handPose = inputs.inGame.poseLocation[side];

        if (handPose.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
            controllerPos = ToGLM(handPose.pose.position);
        }
        if (handPose.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
            controllerQuat = ToGLM(handPose.pose.orientation);
        }
    }

    // Calculate the rotation
    glm::fquat rotatedControllerQuat = glm::normalize(cameraRotation * controllerQuat);

    glm::fvec3 v_controller = cameraRotation * controllerPos;
    glm::fvec3 v_cam = cameraRotation * views;
    glm::fvec3 v_controller_local = v_controller - v_cam;

    rotatedControllerQuat = glm::rotate(rotatedControllerQuat, glm::radians(180.0f), glm::fvec3(1.0f, 0.0f, 0.0f));
    rotatedControllerQuat = glm::rotate(rotatedControllerQuat, glm::radians(180.0f), glm::fvec3(0.0f, 0.0f, 1.0f));
    glm::fmat3 finalMtx = glm::mat3_cast(glm::inverse(rotatedControllerQuat) * glm::inverse(VRManager::instance().XR->m_inputCameraRotation.load()));

    toBeAdjustedMtx.x_x = finalMtx[0][0];
    toBeAdjustedMtx.y_x = finalMtx[0][1];
    toBeAdjustedMtx.z_x = finalMtx[0][2];

    toBeAdjustedMtx.x_y = finalMtx[1][0];
    toBeAdjustedMtx.y_y = finalMtx[1][1];
    toBeAdjustedMtx.z_y = finalMtx[1][2];

    toBeAdjustedMtx.x_z = finalMtx[2][0];
    toBeAdjustedMtx.y_z = finalMtx[2][1];
    toBeAdjustedMtx.z_z = finalMtx[2][2];

    glm::fvec3 rotatedControllerPos = v_controller_local * glm::inverse(VRManager::instance().XR->m_inputCameraRotation.load()) + v_cam;
    glm::fvec3 finalPos = cameraPosition + rotatedControllerPos;

    //Log::print("!! camera pos = {} rotation = {}", finalPos, views);

    toBeAdjustedMtx.pos_x = finalPos.x;
    toBeAdjustedMtx.pos_y = finalPos.y;
    toBeAdjustedMtx.pos_z = finalPos.z;
}

void CemuHooks::hook_ChangeWeaponMtx(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    //if (CemuHooks::GetSettings().IsThirdPersonMode()) {
    //    return;
    //}

    // r3 holds the source actor pointer
    // r4 holds the bone name
    // r5 holds the matrix that is to be set
    // r6 holds the extra matrix that is to be set
    // r7 holds the ModelBindInfo->mtx
    // r8 holds the target actor pointer
    // r9 returns 1 if the weapon is modified
    // r10 holds the camera pointer

    hCPU->gpr[9] = 0;

    uint32_t actorPtr = hCPU->gpr[3];
    uint32_t boneNamePtr = hCPU->gpr[4];
    uint32_t weaponMtxPtr = hCPU->gpr[5];
    uint32_t playerMtxPtr = hCPU->gpr[6];
    uint32_t modelBindInfoMtxPtr = hCPU->gpr[7];
    uint32_t targetActorPtr = hCPU->gpr[8];
    uint32_t cameraPtr = hCPU->gpr[10];

    uint32_t actorLinkPtr = actorPtr + offsetof(ActorWiiU, baseProcPtr);
    uint32_t actorNamePtr = 0;
    readMemoryBE(actorLinkPtr, &actorNamePtr);
    if (actorNamePtr == 0)
        return;
    char* actorName = (char*)s_memoryBaseAddress + actorNamePtr;

    BESeadLookAtCamera camera = {};
    readMemory(cameraPtr, &camera);
    glm::fvec3 cameraPos = camera.pos.getLE();
    glm::fvec3 cameraAt = camera.at.getLE();
    glm::fquat lookAtQuat = glm::quatLookAtRH(glm::normalize(cameraAt - cameraPos), { 0.0, 1.0, 0.0 });
    glm::fvec3 lookAtPos = cameraPos;
    lookAtPos.y += CemuHooks::GetSettings().playerHeightSetting.getLE();

    // read bone name
    if (boneNamePtr == 0)
        return;
    char* boneName = (char*)s_memoryBaseAddress + boneNamePtr;

    // real logic
    bool isHeldByPlayer = strcmp(actorName, "GameROMPlayer") == 0;
    bool isLeftHandWeapon = strcmp(boneName, "Weapon_L") == 0;
    bool isRightHandWeapon = strcmp(boneName, "Weapon_R") == 0;
    if (actorName[0] != '\0' && boneName[0] != '\0' && isHeldByPlayer && (isLeftHandWeapon || isRightHandWeapon)) {
        BEMatrix34 weaponMtx = {};
        readMemory(weaponMtxPtr, &weaponMtx);

        BEMatrix34 modelBindInfoMtx = {};
        readMemory(modelBindInfoMtxPtr, &modelBindInfoMtx);

        ModifyWeaponMtxToVRPose(isLeftHandWeapon ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT, weaponMtx, lookAtQuat, lookAtPos);

        // prevent weapon transparency
        BEType<float> modelOpacity = 1.0f;
        writeMemory(targetActorPtr + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
        uint8_t opacityOrSomethingEnabled = 1;
        writeMemory(targetActorPtr + offsetof(ActorWiiU, opacityOrSomethingEnabled), &opacityOrSomethingEnabled);

        writeMemory(weaponMtxPtr, &weaponMtx);
        writeMemory(modelBindInfoMtxPtr, &modelBindInfoMtx);

        hCPU->gpr[9] = 1;
    }
}

void CemuHooks::hook_EnableWeaponAttackSensor(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    //if (CemuHooks::GetSettings().IsThirdPersonMode()) {
    //    return;
    //}

    uint32_t weaponPtr = hCPU->gpr[3];
    uint32_t parentActorPtr = hCPU->gpr[4];
    uint32_t heldIndex = hCPU->gpr[5]; // this is either 0 or 1 depending on which hand the weapon is in
    bool isHeldByPlayer = hCPU->gpr[6] == 0;

    Weapon weapon = {};
    readMemory(weaponPtr, &weapon);

    WeaponType weaponType = weapon.type.getLE();
    if (weaponType == WeaponType::Bow || weaponType == WeaponType::Shield) {
        Log::print("!! Skipping motion analysis for Bow/Shield (type: {})", (int)weaponType);
        return;
    }

    if (heldIndex >= m_motionAnalyzers.size()) {
        Log::print("!! Invalid heldIndex: {}. Skipping motion analysis.", heldIndex);
        return;
    }

    heldIndex = heldIndex == 0 ? 1 : 0;

    auto state = VRManager::instance().XR->m_input.load();
    auto headset = VRManager::instance().XR->GetRenderer()->GetMiddlePose();

    if (!headset.has_value()) {
        return;
    }


    m_motionAnalyzers[heldIndex].ResetIfWeaponTypeChanged(weaponType);
    m_motionAnalyzers[heldIndex].Update(state.inGame.poseLocation[heldIndex], state.inGame.poseVelocity[heldIndex], headset.value(), state.inGame.inputTime);

    // Use the analysed motion to determine whether the weapon is swinging or stabbing, and whether the attackSensor should be active this frame
    if (isHeldByPlayer && m_motionAnalyzers[heldIndex].IsAttacking()) {
        m_motionAnalyzers[heldIndex].SetHitboxEnabled(true);
        Log::print("!! Activate sensor for {}: isHeldByPlayer={}, weaponType={}", heldIndex, isHeldByPlayer, (int)weaponType);
        weapon.setupAttackSensor.resetAttack = 1;
        weapon.setupAttackSensor.mode = 2;
        weapon.setupAttackSensor.isContactLayerInitialized = 0;
        // weapon.setupAttackSensor.overrideImpact = 1;
        // weapon.setupAttackSensor.multiplier = analyzer->GetDamage();
        // weapon.setupAttackSensor.impact = analyzer->GetImpulse();
        writeMemory(weaponPtr, &weapon);
    }
    else if (m_motionAnalyzers[heldIndex].IsHitboxEnabled()) {
        m_motionAnalyzers[heldIndex].SetHitboxEnabled(false);
        Log::print("!! Deactivate sensor for {}: isHeldByPlayer={}, weaponType={}", heldIndex, isHeldByPlayer, (int)weaponType);

        weapon.setupAttackSensor.resetAttack = 1;
        weapon.setupAttackSensor.mode = 1; // deactivate attack sensor
        weapon.setupAttackSensor.isContactLayerInitialized = 0;
        writeMemory(weaponPtr, &weapon);
    }
}

void CemuHooks::hook_EquipWeapon(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    auto input = VRManager::instance().XR->m_input.load();
    uint32_t toBeEquipedSlot = hCPU->gpr[25];

    if (toBeEquipedSlot == 0 || toBeEquipedSlot == 1) {
        //hCPU->gpr[25] = (input.inGame.lastPickupSide == OpenXR::EyeSide::LEFT) ? 1 : 0;
    }

    //switch (hCPU->gpr[25]) {
    //    case 1: {
    //        hCPU->gpr[25] = 0;
    //        break;
    //    }
    //    case 0: {
    //        hCPU->gpr[25] = 1;
    //    }
    //    default: {
    //        break;
    //    }
    //}
}

void CemuHooks::hook_ModifyHandModelAccessSearch(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    if (hCPU->gpr[3] == 0) {
        return;
    }
#ifdef _DEBUG
    // r3 holds the address of the string to search for
    const char* actorName = (const char*)(s_memoryBaseAddress + hCPU->gpr[3]);

    if (actorName != nullptr) {
        // Weapon_R is presumably his right hand bone name
        Log::print("! Searching for model handle using {}", actorName);
    }
#endif
}

void CemuHooks::DrawDebugOverlays() {
    if (ImGui::Begin("Weapon Motion Debugger")) {
        for (auto it = m_motionAnalyzers.rbegin(); it != m_motionAnalyzers.rend(); ++it) {
            ImGui::PushID(&(*it));
            ImGui::BeginGroup();
            it->DrawDebugOverlay();
            ImGui::EndGroup();
            ImGui::PopID();
        }
    }
    ImGui::End();
}
