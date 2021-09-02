#include "layer.h"
#include "memory.h"

uint64_t memoryBaseAddress = NULL;

typedef void* (*memory_getBaseType)();
typedef uint64_t (*gameMeta_getTitleIdType)();

bool cemuInitialize() {
	HMODULE cemuModuleHandle = GetModuleHandleA(NULL);

	memory_getBaseType memory_getBase = (memory_getBaseType)GetProcAddress(cemuModuleHandle, "memory_getBase");
	gameMeta_getTitleIdType gameMeta_getTitleId = (gameMeta_getTitleIdType)GetProcAddress(cemuModuleHandle, "gameMeta_getTitleId");

	if (cemuModuleHandle == NULL || memory_getBase == NULL || gameMeta_getTitleId == NULL) {
		//MessageBoxA(NULL, "Couldn't find the Cemu related functions in the newly launched process.", "Injected into wrong process", NULL);
		logPrint("Some Vulkan application got launched internally and tried to initialize a Vulkan instance which got accidentally hooked");
		return false;
		char currProcessExecutableCStr[MAX_PATH];
		GetModuleFileNameA(cemuModuleHandle, currProcessExecutableCStr, MAX_PATH);
		std::string currProcessExecutableStr(currProcessExecutableCStr);
		if (!currProcessExecutableStr.ends_with("vrwebhelper.exe")) {
			MessageBoxA(NULL, "Couldn't find the Cemu related functions in the newly launched process.", "Injected into wrong process", NULL);
			return false;
		}
		else {
			logPrint("Steam's vrwebhelper.exe got launched and tried to initialize a Vulkan instance that got accidentally hooked");
		}
	}

	uint64_t currTitleId = gameMeta_getTitleId();
	if (currTitleId != 0x00050000101C9300 && currTitleId != 0x00050000101C9400 && currTitleId != 0x00050000101C9500) {
		MessageBoxA(NULL, "You're using BotW BetterVR with a different game. Please start BotW whenever using the launcher.", "Launched wrong game", NULL);
		exit(0);
	}

	memoryBaseAddress = (uint64_t)memory_getBase();
	return true;
}

void cemuShutdown() {
}

void* cemuFindAddressUsingAOBScan(const char* aobString, uint64_t searchStart, uint64_t searchEnd) {
	if (searchStart == 0) {
		searchStart = memoryBaseAddress;
	}

	MEMORY_BASIC_INFORMATION memBasicInfo{};
	const char* aobStringEnd = aobString + strlen(aobString);

	//char hexBuffer[4];
	//for (char* curr = (char*)searchStart; curr < (char*)searchEnd; curr += memBasicInfo.RegionSize) {
	//	if (!VirtualQuery(curr, &memBasicInfo, sizeof(memBasicInfo)) || memBasicInfo.State != MEM_COMMIT || memBasicInfo.State == PAGE_NOACCESS)
	//		continue;
	//	for (char* c=curr; c<curr+memBasicInfo.RegionSize; c++) {
	//		for (const auto& aobByte : compiledAobPattern) {
	//			c
	//		}
	//	}
	//}

	return nullptr;
}

void* cemuFindAddressUsingBytes(const char* bytes, uint32_t bytesSize, uint64_t searchStart, uint64_t searchEnd) {
	std::string createdAOBString = "";
	char hexBuffer[4];
	for (uint32_t i=0; i<bytesSize; i++) {
		createdAOBString += _itoa_s(int(bytes[i]), hexBuffer, 16);
		createdAOBString += " ";
	}
	cemuFindAddressUsingAOBScan(createdAOBString.c_str(), searchStart, searchEnd);
	return nullptr;
}