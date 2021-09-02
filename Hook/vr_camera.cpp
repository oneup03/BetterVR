#include "layer.h"

bool BetterVR::VRCemu::cemuParentProcess = false;

void BetterVR::VRCemu::Initialize() {
	CHAR tempPath[MAX_PATH + 1];
	GetModuleFileName(NULL, tempPath, MAX_PATH);
	std::string cemuExePath(tempPath);

	std::string cemuEnding("Cemu.exe");
	if (cemuExePath.size() >= cemuEnding.size() && cemuExePath.compare(cemuExePath.size() - cemuEnding.size(), cemuEnding.size(), cemuEnding) == 0) cemuParentProcess = true;
	else cemuParentProcess = false;

	if (!IsCemuParentProcess()) return;
}

inline bool ends_with(std::string const& value, std::string const& ending) {
	if (ending.size() > value.size()) return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool inline BetterVR::VRCemu::IsCemuParentProcess() {
	return cemuParentProcess;
}
