#include "cemu_hooks.h"
#include "instance.h"
#include "rendering/openxr.h"

data_VRProjectionMatrixOut calculateFOVAndOffset(XrFovf viewFOV) {
    float totalHorizontalFov = viewFOV.angleRight - viewFOV.angleLeft;
    float totalVerticalFov = viewFOV.angleUp - viewFOV.angleDown;

    float aspectRatio = totalHorizontalFov / totalVerticalFov;
    float fovY = totalVerticalFov;
    float projectionCenter_offsetX = (viewFOV.angleRight + viewFOV.angleLeft) / 2.0f;
    float projectionCenter_offsetY = (viewFOV.angleUp + viewFOV.angleDown) / 2.0f;

    data_VRProjectionMatrixOut ret = {};
    ret.aspectRatio = aspectRatio;
    ret.fovY = fovY;
    ret.offsetX = projectionCenter_offsetX;
    ret.offsetY = projectionCenter_offsetY;
    return ret;
}

glm::fvec3 g_lookAtPos;
glm::fquat g_lookAtQuat;

void CemuHooks::hook_BeginCameraSide(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    OpenXR::EyeSide side = hCPU->gpr[0] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;

    Log::print("");
    Log::print("===============================================================================");
    Log::print("{0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0}", side == OpenXR::EyeSide::LEFT ? "LEFT" : "RIGHT");

    bool layersInitialized = VRManager::instance().XR->GetRenderer()->m_layer3D && VRManager::instance().XR->GetRenderer()->m_layer2D;
    if (layersInitialized && side == OpenXR::EyeSide::LEFT) {
        VRManager::instance().XR->GetRenderer()->StartFrame();
    }
}

void CemuHooks::hook_GetRenderCamera(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;
    uint32_t cameraIn = hCPU->gpr[3];
    uint32_t cameraOut = hCPU->gpr[12];
    OpenXR::EyeSide cameraSide = hCPU->gpr[11] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;

    BESeadLookAtCamera camera = {};
    readMemory(cameraIn, &camera);

    if (camera.pos.x.getLE() != 0.0f) {
        // Log::print("[MODIFIED] Getting render camera (with LR: {:08X}): {}", hCPU->sprNew.LR, camera);

        // in-game camera
        glm::mat3x4 originalMatrix = camera.mtx.getLEMatrix();
        glm::mat4 viewGame = glm::transpose(originalMatrix);
        glm::mat4 worldGame = glm::inverse(viewGame);
        glm::quat baseRot  = glm::quat_cast(worldGame);
        glm::vec3 basePos  = glm::vec3(worldGame[3]);

        // vr camera
        if (!VRManager::instance().XR->GetRenderer()->GetPose(cameraSide).has_value()) {
            return;
        }
        XrPosef currPose = VRManager::instance().XR->GetRenderer()->GetPose(cameraSide).value();
        glm::fvec3 eyePos(currPose.position.x, currPose.position.y, currPose.position.z);
        glm::fquat eyeRot(currPose.orientation.w, currPose.orientation.x, currPose.orientation.y, currPose.orientation.z);

        glm::vec3 newPos = basePos + (baseRot * eyePos);
        glm::quat newRot = baseRot * eyeRot;

        glm::mat4 newWorldVR = glm::translate(glm::mat4(1.0f), newPos) * glm::mat4_cast(newRot);
        glm::mat4 newViewVR = glm::inverse(newWorldVR);
        glm::mat3x4 rowMajor = glm::transpose(newViewVR);

        camera.mtx.setLEMatrix(rowMajor);
        // glm::fvec3 newPos = glm::fvec3(newWorldVR[3]);
        camera.pos.x = newPos.x;
        camera.pos.y = newPos.y;
        camera.pos.z = newPos.z;

        // Set look-at point by offsetting position in view direction
        glm::vec3 viewDir = -glm::vec3(rowMajor[2]); // Forward direction is -Z in view space
        camera.at.x = newPos.x + viewDir.x;
        camera.at.y = newPos.y + viewDir.y;
        camera.at.z = newPos.z + viewDir.z;

        // Transform world up vector by new rotation
        glm::vec3 upDir = glm::vec3(rowMajor[1]); // Up direction is +Y in view space
        camera.up.x = upDir.x;
        camera.up.y = upDir.y;
        camera.up.z = upDir.z;

        writeMemory(cameraOut, &camera);
        hCPU->gpr[3] = cameraOut;

        // set the lookAt position and quaternion with offset to be able to translate the controller position to the game world
        g_lookAtPos = basePos;
        // g_lookAtPos.y += CemuHooks::GetSettings().playerHeightSetting.getLE();
        g_lookAtQuat = baseRot;
        s_framesSinceLastCameraUpdate = 0;
    }
    else {
        // Log::print("[ERROR!!!] Getting render camera (with LR: {:08X}): {}", hCPU->sprNew.LR, camera);
    }
}

constexpr uint32_t seadOrthoProjection = 0x1027B5BC;
constexpr uint32_t seadPerspectiveProjection = 0x1027B54C;

// https://github.com/KhronosGroup/OpenXR-SDK/blob/858912260ca616f4c23f7fb61c89228c353eb124/src/common/xr_linear.h#L564C1-L632C2
// https://github.com/aboood40091/sead/blob/45b629fb032d88b828600a1b787729f2d398f19d/engine/library/modules/src/gfx/seadProjection.cpp#L166
static glm::mat4 calculateProjectionMatrix(float nearZ, float farZ, const XrFovf& fov) {
    float l = tanf(fov.angleLeft) * nearZ;
    float r = tanf(fov.angleRight) * nearZ;
    float b = tanf(fov.angleDown) * nearZ;
    float t = tanf(fov.angleUp) * nearZ;

    float invW = 1.0f / (r - l);
    float invH = 1.0f / (t - b);
    float invD = 1.0f / (farZ - nearZ);

    glm::mat4 col;
    col[0][0] = 2 * nearZ * invW;
    col[1][1] = 2 * nearZ * invH;
    col[0][2] = (r + l) * invW;
    col[1][2] = (t + b) * invH;
    col[2][2] = -(farZ + nearZ) * invD;
    col[2][3] = -(2 * farZ * nearZ) * invD;
    col[3][2] = -1.0f;
    col[3][3] = 0.0f;

    return glm::transpose(col);
}

void CemuHooks::hook_GetRenderProjection(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t projectionIn = hCPU->gpr[3];
    uint32_t projectionOut = hCPU->gpr[12];
    OpenXR::EyeSide side = hCPU->gpr[0] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;

    BESeadProjection projection = {};
    readMemory(projectionIn, &projection);

    if (projection.__vftable == seadPerspectiveProjection) {
        BESeadPerspectiveProjection perspectiveProjection = {};
        readMemory(projectionIn, &perspectiveProjection);
        // Log::print("Render Proj. (LR: {:08X}): {}", hCPU->sprNew.LR, perspectiveProjection);

        if (!VRManager::instance().XR->GetRenderer()->GetFOV(side).has_value()) {
            return;
        }
        XrFovf currFOV = VRManager::instance().XR->GetRenderer()->GetFOV(side).value();
        auto newProjection = calculateFOVAndOffset(currFOV);

        perspectiveProjection.aspect = newProjection.aspectRatio;
        perspectiveProjection.fovYRadiansOrAngle = newProjection.fovY;
        float halfAngle = newProjection.fovY.getLE() * 0.5f;
        perspectiveProjection.fovySin = sinf(halfAngle);
        perspectiveProjection.fovyCos = cosf(halfAngle);
        perspectiveProjection.fovyTan = tanf(halfAngle);
        perspectiveProjection.offset.x = newProjection.offsetX;
        perspectiveProjection.offset.y = newProjection.offsetY;

        glm::fmat4 newMatrix = calculateProjectionMatrix(perspectiveProjection.zNear.getLE(), perspectiveProjection.zFar.getLE(), currFOV);
        glm::fmat4 newDeviceMatrix = newMatrix;
        perspectiveProjection.matrix = newMatrix;

        // calculate device matrix
        float zScale = perspectiveProjection.deviceZScale.getLE();
        float zOffset = perspectiveProjection.deviceZOffset.getLE();

        newDeviceMatrix[2][0] *= zScale;
        newDeviceMatrix[2][1] *= zScale;
        newDeviceMatrix[2][2] = (newDeviceMatrix[2][2] + newDeviceMatrix[3][2] * zOffset) * zScale;
        newDeviceMatrix[2][3] = newDeviceMatrix[2][3] * zScale + newDeviceMatrix[3][3] * zOffset;

        perspectiveProjection.deviceMatrix = newDeviceMatrix;
        perspectiveProjection.dirty = true;
        perspectiveProjection.deviceDirty = true;

        writeMemory(projectionOut, &perspectiveProjection);
        hCPU->gpr[3] = projectionOut;
    }
}

void CemuHooks::hook_EndCameraSide(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    OpenXR::EyeSide side = hCPU->gpr[3] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;

    if (VRManager::instance().XR->GetRenderer()->IsInitialized() && side == OpenXR::EyeSide::RIGHT) {
        VRManager::instance().XR->GetRenderer()->EndFrame();
    }

    Log::print("{0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0}", side == OpenXR::EyeSide::LEFT ? "LEFT" : "RIGHT");
    Log::print("===============================================================================");
    Log::print("");
}