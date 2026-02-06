#include "cemu_hooks.h"
#include "imgui_internal.h"
#include "instance.h"
#include "hooking/entity_debugger.h"

ModSettings g_settings = {};

ModSettings& GetSettings() {
    return g_settings;
}

static void* Settings_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
    if (strcmp(name, "Settings") != 0)
        return nullptr;
    return &GetSettings();
}

static void Settings_ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
    auto* s = (ModSettings*)entry;
    int i_val;
    float f_val;
    if (sscanf(line, "CameraMode=%d", &i_val) == 1)      { s->cameraMode.store((CameraMode)i_val); return; }
    if (sscanf(line, "PlayMode=%d", &i_val) == 1)        { s->playMode.store((PlayMode)i_val); return; }
    if (sscanf(line, "ThirdPlayerDistance=%f", &f_val) == 1) { s->thirdPlayerDistance.store(f_val); return; }
    if (sscanf(line, "CutsceneCameraMode=%d", &i_val) == 1) { s->cutsceneCameraMode.store((EventMode)i_val); return; }
    if (sscanf(line, "UseBlackBarsForCutscenes=%d", &i_val) == 1) { s->useBlackBarsForCutscenes.store(i_val); return; }
    if (sscanf(line, "PlayerHeightOffset=%f", &f_val) == 1) { s->playerHeightOffset.store(f_val); return; }
    if (sscanf(line, "LeftHanded=%d", &i_val) == 1)      { s->leftHanded.store(i_val); return; }
    if (sscanf(line, "UiFollowsGaze=%d", &i_val) == 1)   { s->uiFollowsGaze.store(i_val); return; }
    if (sscanf(line, "CropFlatTo16x9=%d", &i_val) == 1)  { s->cropFlatTo16x9.store(i_val); return; }
    if (sscanf(line, "EnableDebugOverlay=%d", &i_val) == 1) { s->enableDebugOverlay.store(i_val); return; }
    if (sscanf(line, "BuggyAngularVelocity=%d", &i_val) == 1) { s->buggyAngularVelocity.store((AngularVelocityFixerMode)i_val); return; }
    if (sscanf(line, "PerformanceOverlay=%d", &i_val) == 1) { s->performanceOverlay.store(i_val); return; }
    if (sscanf(line, "PerformanceOverlayFrequency=%d", &i_val) == 1) { s->performanceOverlayFrequency.store(i_val); return; }
    if (sscanf(line, "TutorialPromptShown=%d", &i_val) == 1) { s->tutorialPromptShown.store(i_val); return; }
    if (sscanf(line, "AxisThreshold=%f", &f_val) == 1) { s->axisThreshold.store(f_val); return; }
    if (sscanf(line, "StickDeadzone=%f", &f_val) == 1) { s->stickDeadzone.store(f_val); return; }
}

static void Settings_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
    auto& s = GetSettings();
    buf->reserve(buf->size() + 1024);
    buf->appendf("[%s][Settings]\n", handler->TypeName);
    buf->appendf("CameraMode=%d\n", (int)s.cameraMode.load());
    buf->appendf("PlayMode=%d\n", (int)s.playMode.load());
    buf->appendf("ThirdPlayerDistance=%.3f\n", s.thirdPlayerDistance.load());
    buf->appendf("CutsceneCameraMode=%d\n", (int)s.cutsceneCameraMode.load());
    buf->appendf("UseBlackBarsForCutscenes=%d\n", (int)s.useBlackBarsForCutscenes.load());
    buf->appendf("PlayerHeightOffset=%.3f\n", s.playerHeightOffset.load());
    buf->appendf("LeftHanded=%d\n", (int)s.leftHanded.load());
    buf->appendf("UiFollowsGaze=%d\n", (int)s.uiFollowsGaze.load());
    buf->appendf("CropFlatTo16x9=%d\n", (int)s.cropFlatTo16x9.load());
    buf->appendf("EnableDebugOverlay=%d\n", (int)s.enableDebugOverlay.load());
    buf->appendf("BuggyAngularVelocity=%d\n", (int)s.buggyAngularVelocity.load());
    buf->appendf("PerformanceOverlay=%d\n", (int)s.performanceOverlay.load());
    buf->appendf("PerformanceOverlayFrequency=%d\n", s.performanceOverlayFrequency.load());
    buf->appendf("TutorialPromptShown=%d\n", (int)s.tutorialPromptShown.load());
    buf->appendf("AxisThreshold=%.3f\n", s.axisThreshold.load());
    buf->appendf("StickDeadzone=%.3f\n", s.stickDeadzone.load());
    buf->appendf("\n");
}

void InitSettings() {
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "BetterVR";
    ini_handler.TypeHash = ImHashStr("BetterVR");
    ini_handler.ReadOpenFn = Settings_ReadOpen;
    ini_handler.ReadLineFn = Settings_ReadLine;
    ini_handler.WriteAllFn = Settings_WriteAll;
    ImGui::AddSettingsHandler(&ini_handler);
}

HWND CemuHooks::m_cemuTopWindow = NULL;
HWND CemuHooks::m_cemuRenderWindow = NULL;
uint64_t CemuHooks::s_memoryBaseAddress = 0;
std::atomic_uint32_t CemuHooks::s_framesSinceLastCameraUpdate = 0;


bool CemuHooks::IsScreenOpen(ScreenId screen) {
    uint32_t screenManagerInstance = getMemory<BEType<uint32_t>>(0x1047E650).getLE();
    if (screenManagerInstance != 0) {
        uint32_t screenBools = getMemory<BEType<uint32_t>>(screenManagerInstance + 0x18).getLE();
        uint32_t screenPtr = getMemory<BEType<uint32_t>>(screenBools + (std::to_underlying(screen) * 4)).getLE();
        return screenPtr != 0;
    }
    return false;
}

std::unordered_set<ScreenId> prevEnabledScreens = {};

void CemuHooks::InitWindowHandles() {
    // find HWND that starts with Cemu in its title
    struct EnumWindowsData {
        DWORD cemuPid;
        HWND outHwnd;
    } enumData = { .cemuPid = GetCurrentProcessId(), .outHwnd = NULL };

    EnumWindows([](HWND iteratedHwnd, LPARAM data) -> BOOL {
        EnumWindowsData* enumData = (EnumWindowsData*)data;
        DWORD currPid;
        GetWindowThreadProcessId(iteratedHwnd, &currPid);
        if (currPid == enumData->cemuPid) {
            constexpr size_t bufSize = 256;
            wchar_t buf[bufSize];
            GetWindowTextW(iteratedHwnd, buf, bufSize);
            if (wcsstr(buf, L"Cemu") != nullptr) {
                enumData->outHwnd = iteratedHwnd;
                return FALSE;
            }
        }
        return TRUE;
    },
    (LPARAM)&enumData);
    m_cemuTopWindow = enumData.outHwnd;

    // find the most nested child window since that's the rendering window
    HWND iteratedHwnd = m_cemuTopWindow;
    while (true) {
        HWND nextIteratedHwnd = FindWindowExW(iteratedHwnd, NULL, NULL, NULL);
        if (nextIteratedHwnd == NULL) {
            break;
        }
        iteratedHwnd = nextIteratedHwnd;
    }
    m_cemuRenderWindow = iteratedHwnd;
}

void CemuHooks::hook_UpdateSettings(PPCInterpreter_t* hCPU) {
    // Log::print("Updated settings!");
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t ppc_tableOfCutsceneEventSettings = hCPU->gpr[6];
    
    if (GetSettings().ShowDebugOverlay() && VRManager::instance().Hooks->m_entityDebugger) {
        VRManager::instance().Hooks->m_entityDebugger->UpdateEntityMemory();
    }

    ++s_framesSinceLastCameraUpdate;

#ifdef _DEBUG
    constexpr uint32_t maxScreenIdx = std::to_underlying(ScreenId::ScreenId_END);
    std::unordered_set<ScreenId> currentEnabledScreens;
    for (uint32_t i = 0; i < maxScreenIdx; i++) {
        ScreenId id = (ScreenId)i;
        bool hasScreen = IsScreenOpen(id);

        if (hasScreen) {            

            if (!prevEnabledScreens.contains(id)) {
                if (currentEnabledScreens.empty()) {
                    Log::print<INFO>("---------");
                }
                Log::print<INFO>("Screen {} is ON", ScreenIdToString((ScreenId)i));
            }
            currentEnabledScreens.emplace(id);
        }
        else if (prevEnabledScreens.contains(id)) {
            Log::print<INFO>("Screen {} is OFF", ScreenIdToString((ScreenId)i));
        }
    }
    prevEnabledScreens = currentEnabledScreens;
#endif

    static bool logSettings = true;
    if (logSettings) {
        Log::print<INFO>("VR Settings:\n{}", g_settings.ToString());
        logSettings = false;
    }

    initCutsceneDefaultSettings(ppc_tableOfCutsceneEventSettings);
}

void CemuHooks::hook_OSReportToConsole(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t strPtr = hCPU->gpr[3];
    if (strPtr == 0) {
        return;
    }
    char* str = (char*)(s_memoryBaseAddress + strPtr);
    if (str == nullptr) {
        return;
    }
    if (str[0] != '\0') {
        Log::print<PPC>(str);
    }
}

constexpr uint32_t playerVtable = 0x101E5FFC;
void CemuHooks::hook_RouteActorJob(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t actorPtr = hCPU->gpr[3];
    uint32_t jobName = hCPU->gpr[4];
    uint32_t side = hCPU->gpr[5]; // 0 = left, 1 = right

    std::string jobNameStr = std::string((char*)(s_memoryBaseAddress + jobName));

    ActorWiiU actor;
    readMemory(actorPtr, &actor);
    std::string actorName = actor.name.getLE();

#define SKIP_ON_LEFT_SIDE if (side == 0) { hCPU->gpr[3] = 1; }
#define SKIP_ON_RIGHT_SIDE if (side == 1) { hCPU->gpr[3] = 1; }
#define USE_ALTERED_PATH_ON_LEFT_SIDE if (side == 0) { hCPU->gpr[3] = 2; }
#define USE_ALTERED_PATH_ON_RIGHT_SIDE if (side == 1) { hCPU->gpr[3] = 2; }

    hCPU->gpr[3] = 0;
    if (actorName == "GameROMPlayer") {
        if (jobNameStr == "job0_1") {
            // this only runs the climbing portion of this actor job on the left eye's side
            // so that later jobs on the left side can use the state set by this portion of code
            USE_ALTERED_PATH_ON_LEFT_SIDE
        }
        else if (jobNameStr == "job0_2") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job1_1") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job1_2") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job2_1_ragdoll_related") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job2_2") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job4") {
            SKIP_ON_RIGHT_SIDE
        }
    }
    else {
        if (jobNameStr == "job0_1") {
            SKIP_ON_LEFT_SIDE
        }
        else if (jobNameStr == "job0_2") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job1_1") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job1_2") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job2_1_ragdoll_related") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job2_2") {
            SKIP_ON_RIGHT_SIDE
        }
        else if (jobNameStr == "job4") {
            SKIP_ON_RIGHT_SIDE
        }
    }

    if (hCPU->gpr[3] == 0) {
        //Log::print<INFO>("[{}] Ran {}", actorName, jobNameStr);
    }
    else if (hCPU->gpr[3] == 2) {
        //Log::print<INFO>("[{}] Ran ALTERED VERSION of {}", actorName, jobNameStr);
    }

    // exit r3:
    // 1 = skip job
    // 0 = perform job
    // 2 = altered job
}

// todo: this only runs when it's shown for the first time!
void CemuHooks::hook_CreateNewScreen(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    const char* screenName = (const char*)(s_memoryBaseAddress + hCPU->gpr[7]);
    ScreenId screenId = (ScreenId)hCPU->gpr[5];
    Log::print<CONTROLS>("Creating new screen \"{}\" with ID {:08X}...", screenName, std::to_underlying(screenId));

    // todo: When a pickup screen is shown, we should track if the user does a short grip press, and if it was the left and right hand.
    if (screenId == ScreenId::PickUp_00) {
        Log::print<CONTROLS>("PickUp screen detected, waiting for grip button press to bind item to hand...");
    }
}


enum class GX2_BLENDFACTOR {
    ZERO = 0x00,
    ONE = 0x01,
    SRC_COLOR = 0x02,
    ONE_MINUS_SRC_COLOR = 0x03,
    SRC_ALPHA = 0x04,
    ONE_MINUS_SRC_ALPHA = 0x05,
    DST_ALPHA = 0x06,
    ONE_MINUS_DST_ALPHA = 0x07,
    DST_COLOR = 0x08,
    ONE_MINUS_DST_COLOR = 0x09,
    SRC_ALPHA_SATURATE = 0x0A,
    BOTH_SRC_ALPHA = 0x0B,
    BOTH_INV_SRC_ALPHA = 0x0C,
    CONST_COLOR = 0x0D,
    ONE_MINUS_CONST_COLOR = 0x0E,
    SRC1_COLOR = 0x0F,
    INV_SRC1_COLOR = 0x10,
    SRC1_ALPHA = 0x11,
    INV_SRC1_ALPHA = 0x12,
    CONST_ALPHA = 0x13,
    ONE_MINUS_CONST_ALPHA = 0x14
};

enum class GX2_COMBINEFUNC {
    DST_PLUS_SRC = 0,
    SRC_MINUS_DST = 1,
    MIN_DST_SRC = 2,
    MAX_DST_SRC = 3,
    DST_MINUS_SRC = 4
};

void CemuHooks::hook_FixUIBlending(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t renderTargetIndex = hCPU->gpr[3];
    GX2_BLENDFACTOR colorSrcFactor = (GX2_BLENDFACTOR)hCPU->gpr[4];
    GX2_BLENDFACTOR colorDstFactor = (GX2_BLENDFACTOR)hCPU->gpr[5];
    GX2_COMBINEFUNC colorCombineFunc = (GX2_COMBINEFUNC)hCPU->gpr[6];
    uint32_t separateAlphaBlend = hCPU->gpr[7];
    GX2_BLENDFACTOR alphaSrcFactor = (GX2_BLENDFACTOR)hCPU->gpr[8];
    GX2_BLENDFACTOR alphaDstFactor = (GX2_BLENDFACTOR)hCPU->gpr[9];
    GX2_COMBINEFUNC alphaCombineFunc = (GX2_COMBINEFUNC)hCPU->gpr[10];

    {
        bool matchesColorSettings = colorSrcFactor == GX2_BLENDFACTOR::DST_COLOR && colorDstFactor == GX2_BLENDFACTOR::SRC_ALPHA && colorCombineFunc == GX2_COMBINEFUNC::DST_PLUS_SRC;
        bool matchesAlpha = alphaSrcFactor == GX2_BLENDFACTOR::SRC_ALPHA && alphaDstFactor == GX2_BLENDFACTOR::ONE_MINUS_SRC_ALPHA && alphaCombineFunc == GX2_COMBINEFUNC::DST_PLUS_SRC;

        if (matchesColorSettings && matchesAlpha) {
            hCPU->gpr[7] = 1;
            hCPU->gpr[8] = std::to_underlying(GX2_BLENDFACTOR::ZERO);
            hCPU->gpr[9] = std::to_underlying(GX2_BLENDFACTOR::DST_ALPHA);

            //Log::print<VERBOSE>("FixUIBlending called with renderTargetIndex: {}, colorSrcFactor: {}, colorDstFactor: {}, colorCombineFunc: {}, separateAlphaBlend: {}, alphaSrcFactor: {}, alphaDstFactor: {}, alphaCombineFunc: {}", renderTargetIndex, std::to_underlying(colorSrcFactor), std::to_underlying(colorDstFactor), std::to_underlying(colorCombineFunc), separateAlphaBlend, std::to_underlying(alphaSrcFactor), std::to_underlying(alphaDstFactor), std::to_underlying(alphaCombineFunc));
        }
    }
}
