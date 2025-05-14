#include "../instance.h"
#include "cemu_hooks.h"

#include <glm/gtx/quaternion.hpp>


extern glm::fvec3 g_lookAtPos;
extern glm::fquat g_lookAtQuat;

void modifyWeaponMtxToVRPose(OpenXR::EyeSide side, BEMatrix34& toBeAdjustedMtx, BEMatrix34& defaultMtx) {
    // convert VR controller info to glm
    OpenXR::InputState inputs = VRManager::instance().XR->m_input.load();

    glm::fvec3 controllerPos = glm::fvec3(0.0f);
    glm::fquat controllerQuat = glm::identity<glm::fquat>();

    if (inputs.inGame.in_game && inputs.inGame.pose[side].isActive) {
        auto& handPose = side == OpenXR::EyeSide::LEFT ? inputs.inGame.poseLocation[side] : inputs.inGame.poseLocation[side];

        if (handPose.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
            controllerPos = glm::fvec3(
                handPose.pose.position.x,
                handPose.pose.position.y,
                handPose.pose.position.z
            );
        }
        if (handPose.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
            controllerQuat = glm::fquat(
                handPose.pose.orientation.w,
                handPose.pose.orientation.x,
                handPose.pose.orientation.y,
                handPose.pose.orientation.z
            );
        }
    }

    // handPose.orientation.w = rotateHorizontalCounter.w;
    // handPose.orientation.x = rotateHorizontalCounter.x;
    // handPose.orientation.y = rotateHorizontalCounter.y;
    // handPose.orientation.z = rotateHorizontalCounter.z;
    // rotateHorizontalCounter = glm::rotate(rotateHorizontalCounter, glm::radians(360.0f/30.0f/1.0f), glm::fvec3(1.0f, 0.0f, 0.0f));

    // Next, calculate the rotation
    glm::fquat rotatedControllerQuat = glm::normalize(g_lookAtQuat * controllerQuat);
    rotatedControllerQuat = glm::rotate(rotatedControllerQuat, glm::radians(180.0f), glm::fvec3(1.0f, 0.0f, 0.0f));
    rotatedControllerQuat = glm::rotate(rotatedControllerQuat, glm::radians(180.0f), glm::fvec3(0.0f, 0.0f, 1.0f));
    glm::fmat3 finalMtx = glm::toMat3(glm::inverse(rotatedControllerQuat));

    toBeAdjustedMtx.x_x = finalMtx[0][0];
    toBeAdjustedMtx.y_x = finalMtx[0][1];
    toBeAdjustedMtx.z_x = finalMtx[0][2];

    toBeAdjustedMtx.x_y = finalMtx[1][0];
    toBeAdjustedMtx.y_y = finalMtx[1][1];
    toBeAdjustedMtx.z_y = finalMtx[1][2];

    toBeAdjustedMtx.x_z = finalMtx[2][0];
    toBeAdjustedMtx.y_z = finalMtx[2][1];
    toBeAdjustedMtx.z_z = finalMtx[2][2];

    // First, calculate the position
    // Use player position as the origin since we want to overwrite the weapon position with the VR controller position
    glm::fvec3 rotatedControllerPos = g_lookAtQuat * controllerPos;
    glm::fvec3 finalPos = g_lookAtPos + rotatedControllerPos;

    toBeAdjustedMtx.pos_x = finalPos.x;
    toBeAdjustedMtx.pos_y = finalPos.y;
    toBeAdjustedMtx.pos_z = finalPos.z;
}

void CemuHooks::hook_ChangeWeaponMtx(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    // r3 holds an actor pointer, I think?
    // r4 holds the bone name
    // r5 holds the matrix that is to be set
    // r6 holds the extra matrix that is to be set
    // r7 holds the ModelBindInfo->mtx

    hCPU->gpr[9] = 0;

    uint32_t actorLinkPtr = hCPU->gpr[3] + offsetof(ActorWiiU, baseProcPtr);
    uint32_t actorNamePtr = 0;
    readMemoryBE(actorLinkPtr, &actorNamePtr);
    if (actorNamePtr == 0)
        return;
    char* actorName = (char*)s_memoryBaseAddress + actorNamePtr;

    // read bone name
    uint32_t boneNamePtr = hCPU->gpr[4];
    if (boneNamePtr == 0)
        return;
    char* boneName = (char*)s_memoryBaseAddress + boneNamePtr;

    // real logic
    bool isHeldByPlayer = strcmp(actorName, "GameROMPlayer") == 0;
    bool isLeftHandWeapon = strcmp(boneName, "Weapon_L") == 0;
    bool isRightHandWeapon = strcmp(boneName, "Weapon_R") == 0;
    if (actorName[0] != '\0' && boneName[0] != '\0' && isHeldByPlayer && (isLeftHandWeapon || isRightHandWeapon)) {
        BEMatrix34 weaponMtx = {};
        readMemory(hCPU->gpr[5], &weaponMtx);

        BEMatrix34 playerMtx = {};
        readMemory(hCPU->gpr[6], &playerMtx);

        BEMatrix34 modelBindInfoMtx = {};
        readMemory(hCPU->gpr[7], &modelBindInfoMtx);

        modifyWeaponMtxToVRPose(isLeftHandWeapon ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT, weaponMtx, playerMtx);

        // prevent weapon transparency
        BEType<float> modelOpacity = 1.0f;
        writeMemory(hCPU->gpr[8] + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
        uint8_t opacityOrSomethingEnabled = 1;
        writeMemory(hCPU->gpr[8] + offsetof(ActorWiiU, opacityOrSomethingEnabled), &opacityOrSomethingEnabled);

        writeMemory(hCPU->gpr[5], &weaponMtx);
        writeMemory(hCPU->gpr[6], &playerMtx);
        writeMemory(hCPU->gpr[7], &modelBindInfoMtx);

        hCPU->gpr[9] = 1;
    }
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
        Log::print("Searching for model handle using {}", actorName);
    }
#endif
}

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