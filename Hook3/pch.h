#pragma once

#include <Windows.h>
#include <winrt/base.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcommon.h>
#include <directxmath.h>
#include <D3Dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "D3DCompiler.lib")

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "vk_layer.h"

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <mutex>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <string>
#include <assert.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <bitset>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/projection.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#undef VK_LAYER_EXPORT
#if defined(WIN32)
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)
#else
#define VK_LAYER_EXPORT extern "C"
#endif

static PFN_xrGetD3D11GraphicsRequirementsKHR func_xrGetD3D11GraphicsRequirementsKHR;
static PFN_xrConvertTimeToWin32PerformanceCounterKHR func_xrConvertTimeToWin32PerformanceCounterKHR;
static PFN_xrConvertWin32PerformanceCounterToTimeKHR func_xrConvertWin32PerformanceCounterToTimeKHR;
static PFN_xrCreateDebugUtilsMessengerEXT func_xrCreateDebugUtilsMessengerEXT;

union FPR_t {
	double fpr;
	struct
	{
		double fp0;
		double fp1;
	};
	struct
	{
		uint64_t guint;
	};
	struct
	{
		uint64_t fp0int;
		uint64_t fp1int;
	};
};

struct PPCInterpreter_t {
	uint32_t instructionPointer;
	uint32_t gpr[32];
	FPR_t fpr[32];
	uint32_t fpscr;
	uint8_t crNew[32]; // 0 -> bit not set, 1 -> bit set (upper 7 bits of each byte must always be zero) (cr0 starts at index 0, cr1 at index 4 ..)
	uint8_t xer_ca;  // carry from xer
	uint8_t LSQE;
	uint8_t PSE;
	// thread remaining cycles
	int32_t remainingCycles; // if this value goes below zero, the next thread is scheduled
	int32_t skippedCycles; // if remainingCycles is set to zero to immediately end thread execution, this value holds the number of skipped cycles
	struct
	{
		uint32_t LR;
		uint32_t CTR;
		uint32_t XER;
		uint32_t UPIR;
		uint32_t UGQR[8];
	}sprNew;
};

enum class ScreenId {
	GamePadBG_00 = 0x0,
	Title_00 = 0x1,
	MainScreen3D_00 = 0x2,
	Message3D_00 = 0x3,
	AppCamera_00 = 0x4,
	WolfLinkHeartGauge_00 = 0x5,
	EnergyMeterDLC_00 = 0x6,
	MainHorse_00 = 0x7,
	MiniGame_00 = 0x8,
	ReadyGo_00 = 0x9,
	KeyNum_00 = 0xA,
	MessageGet_00 = 0xB,
	DoCommand_00 = 0xC,
	SousaGuide_00 = 0xD,
	GameTitle_00 = 0xE,
	DemoName_00 = 0xF,
	DemoNameEnemy_00 = 0x10,
	MessageSp_00_NoTop = 0x11,
	ShopBG_00 = 0x12,
	ShopBtnList5_00 = 0x13,
	ShopBtnList20_00 = 0x14,
	ShopBtnList15_00 = 0x15,
	ShopInfo_00 = 0x16,
	ShopHorse_00 = 0x17,
	Rupee_00 = 0x18,
	KologNum_00 = 0x19,
	AkashNum_00 = 0x1A,
	MamoNum_00 = 0x1B,
	Time_00 = 0x1C,
	PauseMenuBG_00 = 0x1D,
	SeekPadMenuBG_00 = 0x1E,
	AppTool_00 = 0x1F,
	AppAlbum_00 = 0x20,
	AppPictureBook_00 = 0x21,
	AppMapDungeon_00 = 0x22,
	MainScreenMS_00 = 0x23,
	MainScreenHeartIchigekiDLC_00 = 0x24,
	MainScreen_00 = 0x25,
	MainDungeon_00 = 0x26,
	ChallengeWin_00 = 0x27,
	PickUp_00 = 0x28,
	MessageTipsRunTime_00 = 0x29,
	AppMap_00 = 0x2A,
	AppSystemWindowNoBtn_00 = 0x2B,
	AppHome_00 = 0x2C,
	MainShortCut_00 = 0x2D,
	PauseMenu_00 = 0x2E,
	PauseMenuInfo_00 = 0x2F,
	GameOver_00 = 0x30,
	HardMode_00 = 0x31,
	SaveTransferWindow_00 = 0x32,
	MessageTipsPauseMenu_00 = 0x33,
	MessageTips_00 = 0x34,
	OptionWindow_00 = 0x35,
	AmiiboWindow_00 = 0x36,
	SystemWindowNoBtn_00 = 0x37,
	ControllerWindow_00 = 0x38,
	SystemWindow_01 = 0x39,
	SystemWindow_00 = 0x3A,
	PauseMenuRecipe_00 = 0x3B,
	PauseMenuMantan_00 = 0x3C,
	PauseMenuEiketsu_00 = 0x3D,
	AppSystemWindow_00 = 0x3E,
	DLCWindow_00 = 0x3F,
	HardModeTextDLC_00 = 0x40,
	TestButton = 0x41,
	TestPocketUIDRC = 0x42,
	TestPocketUITV = 0x43,
	BoxCursorTV = 0x44,
	FadeDemo_00 = 0x45,
	StaffRoll_00 = 0x46,
	StaffRollDLC_00 = 0x47,
	End_00 = 0x48,
	DLCSinJuAkashiNum_00 = 0x49,
	MessageDialog = 0x4A,
	DemoMessage = 0x4B,
	MessageSp_00 = 0x4C,
	Thanks_00 = 0x4D,
	Fade = 0x4E,
	KeyBoradTextArea_00 = 0x4F,
	LastComplete_00 = 0x50,
	OPtext_00 = 0x51,
	LoadingWeapon_00 = 0x52,
	MainHardMode_00 = 0x53,
	LoadSaveIcon_00 = 0x54,
	FadeStatus_00 = 0x55,
	Skip_00 = 0x56,
	ChangeController_00 = 0x57,
	ChangeControllerDRC_00 = 0x58,
	DemoStart_00 = 0x59,
	BootUp_00 = 0x5A,
	BootUp_00_2 = 0x5B,
	ChangeControllerNN_00 = 0x5C,
	AppMenuBtn_00 = 0x5D,
	HomeMenuCapture_00 = 0x5E,
	HomeMenuCaptureDRC_00 = 0x5F,
	HomeNixSign_00 = 0x60,
	ErrorViewer_00 = 0x61,
	ErrorViewerDRC_00 = 0x62,
};