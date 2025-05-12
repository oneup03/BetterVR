#pragma once
#include "pch.h"

class CemuHooks {
public:
    CemuHooks() {
        m_cemuHandle = GetModuleHandleA(NULL);
        checkAssert(m_cemuHandle != NULL, "Failed to get handle of Cemu process which is required for interfacing with Cemu!");

        gameMeta_getTitleId = (gameMeta_getTitleIdPtr_t)GetProcAddress(m_cemuHandle, "gameMeta_getTitleId");
        memory_getBase = (memory_getBasePtr_t)GetProcAddress(m_cemuHandle, "memory_getBase");
        osLib_registerHLEFunction = (osLib_registerHLEFunctionPtr_t)GetProcAddress(m_cemuHandle, "osLib_registerHLEFunction");
        checkAssert(gameMeta_getTitleId != NULL && memory_getBase != NULL && osLib_registerHLEFunction != NULL, "Failed to get function pointers of Cemu functions! Is this hook being used on Cemu?");

        bool isSupportedTitleId = gameMeta_getTitleId() == 0x00050000101C9300 || gameMeta_getTitleId() == 0x00050000101C9400 || gameMeta_getTitleId() == 0x00050000101C9500;
        checkAssert(isSupportedTitleId, std::format("Expected title IDs for Breath of the Wild (00050000-101C9300, 00050000-101C9400 or 00050000-101C9500) but received {:16x}!", gameMeta_getTitleId()).c_str());

        s_memoryBaseAddress = (uint64_t)memory_getBase();
        checkAssert(s_memoryBaseAddress != 0, "Failed to get memory base address of Cemu process!");

        osLib_registerHLEFunction("coreinit", "hook_UpdateSettings", &hook_UpdateSettings);
        osLib_registerHLEFunction("coreinit", "hook_CreateNewScreen", &hook_CreateNewScreen);
        osLib_registerHLEFunction("coreinit", "hook_UpdateActorList", &hook_UpdateActorList);
        osLib_registerHLEFunction("coreinit", "hook_CreateNewActor", &hook_CreateNewActor);
        osLib_registerHLEFunction("coreinit", "hook_InjectXRInput", &hook_InjectXRInput);
        osLib_registerHLEFunction("coreinit", "hook_modifyHandModelAccessSearch", &hook_modifyHandModelAccessSearch);
        osLib_registerHLEFunction("coreinit", "hook_changeWeaponMtx", &hook_changeWeaponMtx);

        osLib_registerHLEFunction("coreinit", "hook_BeginCameraSide", &hook_BeginCameraSide);
        osLib_registerHLEFunction("coreinit", "hook_EndCameraSide", &hook_EndCameraSide);
        osLib_registerHLEFunction("coreinit", "hook_GetRenderProjection", &hook_GetRenderProjection);
        osLib_registerHLEFunction("coreinit", "hook_GetRenderCamera", &hook_GetRenderCamera);
        osLib_registerHLEFunction("coreinit", "hook_OSReportToConsole", &hook_OSReportToConsole);
        osLib_registerHLEFunction("coreinit", "hook_OSReportToConsole2", &hook_OSReportToConsole2);
        osLib_registerHLEFunction("coreinit", "hook_OSReportToConsole3", &hook_OSReportToConsole3);
        osLib_registerHLEFunction("coreinit", "hook_OSReportToConsole4", &hook_OSReportToConsole4);

        osLib_registerHLEFunction("coreinit", "hook_updateCameraOLD", &hook_updateCameraOLD);
    };
    ~CemuHooks() {
        FreeLibrary(m_cemuHandle);
    };

    static data_VRSettingsIn GetSettings();
    static uint32_t GetFramesSinceLastCameraUpdate() { return s_framesSinceLastCameraUpdate.load(); }
    static uint64_t GetMemoryBaseAddress() { return s_memoryBaseAddress; }

private:
    HMODULE m_cemuHandle;

    osLib_registerHLEFunctionPtr_t osLib_registerHLEFunction;
    memory_getBasePtr_t memory_getBase;
    gameMeta_getTitleIdPtr_t gameMeta_getTitleId;

    static uint64_t s_memoryBaseAddress;
    static std::atomic_uint32_t s_framesSinceLastCameraUpdate;

    static void hook_UpdateSettings(PPCInterpreter_t* hCPU);

    static void hook_CreateNewScreen(PPCInterpreter_t* hCPU);
    static void hook_UpdateActorList(PPCInterpreter_t* hCPU);
    static void hook_CreateNewActor(PPCInterpreter_t* hCPU);
    static void hook_InjectXRInput(PPCInterpreter_t* hCPU);
    static void hook_modifyHandModelAccessSearch(PPCInterpreter_t* hCPU);
    static void hook_changeWeaponMtx(PPCInterpreter_t* hCPU);

    // todo: remove this in favour of a better tell when the user is updated
    static void hook_updateCameraOLD(PPCInterpreter_t* hCPU);
    static void hook_BeginCameraSide(PPCInterpreter_t* hCPU);
    static void hook_GetRenderCamera(PPCInterpreter_t* hCPU);
    static void hook_GetRenderProjection(PPCInterpreter_t* hCPU);
    static void hook_OSReportToConsole(PPCInterpreter_t* hCPU);
    static void hook_OSReportToConsole2(PPCInterpreter_t* hCPU);
    static void hook_OSReportToConsole3(PPCInterpreter_t* hCPU);
    static void hook_OSReportToConsole4(PPCInterpreter_t* hCPU);
    static void hook_EndCameraSide(PPCInterpreter_t* hCPU);

    static void updateFrames();

public:
    template <typename T>
    static void writeMemoryBE(uint64_t offset, T* valuePtr) {
        *valuePtr = swapEndianness(*valuePtr);
        memcpy((void*)(s_memoryBaseAddress + offset), (void*)valuePtr, sizeof(T));
    }

    template <typename T>
    static void writeMemory(uint64_t offset, T* valuePtr) {
        memcpy((void*)(s_memoryBaseAddress + offset), (void*)valuePtr, sizeof(T));
    }

    template <typename T>
    static void readMemoryBE(uint64_t offset, T* resultPtr) {
        uint64_t memoryAddress = s_memoryBaseAddress + offset;
        memcpy(resultPtr, (void*)memoryAddress, sizeof(T));
        *resultPtr = swapEndianness(*resultPtr);
    }

    template <typename T>
    static void readMemory(uint64_t offset, T* resultPtr) {
        uint64_t memoryAddress = s_memoryBaseAddress + offset;
        memcpy(resultPtr, (void*)memoryAddress, sizeof(T));
    }

    template <typename T>
    static auto getMemory(uint64_t offset) {
        if constexpr (is_BEType_v<T>) {
            T result;
            readMemory(offset, &result);
            return result;
        }
        else {
            BEType<T> result;
            readMemory(offset, &result);
            return result;
        }
    }
};
