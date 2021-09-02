#include "layer.h"

HANDLE BetterVR::Logging::consoleHandle = INVALID_HANDLE_VALUE;

void BetterVR::Logging::Initialize() {
#ifdef _DEBUG
	AllocConsole();
	SetConsoleTitle("BetterVR Debugging Console");
	consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
	BetterVR::Logging::Print("Successfully started BetterVR!");
}

void BetterVR::Logging::Shutdown() {
	BetterVR::Logging::Print("Shutting down BetterVR application...");
	FreeConsole();
}

void BetterVR::Logging::Print(std::string& message) {
#ifdef _DEBUG
	message += "\n";
	LPCSTR lineCStr = message.c_str();
	DWORD charsWritten = 0;
	WriteConsole(consoleHandle, lineCStr, (DWORD)message.size(), &charsWritten, NULL);
	OutputDebugStringA(lineCStr);
#endif
}

void BetterVR::Logging::Print(const char* message) {
	std::string stringLine(message);
	BetterVR::Logging::Print(stringLine);
}

VkBool32 BetterVR::Logging::VulkanDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData) {
	BetterVR::Logging::Print(pMessage);
	return VK_TRUE;
}
