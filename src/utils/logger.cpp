#include "logger.h"

HANDLE Log::consoleHandle = NULL;
double Log::timeFrequency = 0.0f;
std::ofstream Log::logFile;
std::mutex Log::logMutex;

static void LogSystemHardwareInfo() {
    int cpuInfo[4] = {0, 0, 0, 0};
    __cpuid(cpuInfo, 0x80000000);
    const unsigned int maxExId = static_cast<unsigned int>(cpuInfo[0]);

    std::string cpuBrand;
    if (maxExId >= 0x80000004) {
        char brand[49] = {};
        __cpuid(reinterpret_cast<int*>(brand + 0), 0x80000002);
        __cpuid(reinterpret_cast<int*>(brand + 16), 0x80000003);
        __cpuid(reinterpret_cast<int*>(brand + 32), 0x80000004);
        cpuBrand = brand;
        while (!cpuBrand.empty() && cpuBrand.front() == ' ') cpuBrand.erase(cpuBrand.begin());
        while (!cpuBrand.empty() && cpuBrand.back() == ' ') cpuBrand.pop_back();
        if (!cpuBrand.empty()) {
            Log::print<INFO>("CPU: {}", cpuBrand.c_str());
        }
    }

    MEMORYSTATUSEX statex{};
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        const double totalGiB = double(statex.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
        Log::print<INFO>("RAM: {:.2f} GiB", totalGiB);
    }
}

Log::Log() {
    AllocConsole();
    SetConsoleTitleA("BetterVR Debugging Console");
    consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
#ifndef _DEBUG
    logFile.open("BetterVR.txt", std::ios::out | std::ios::trunc);
#endif
    Log::print<INFO>("Successfully started BetterVR!");
    LogSystemHardwareInfo();

    LARGE_INTEGER timeLI;
    QueryPerformanceFrequency(&timeLI);
    timeFrequency = double(timeLI.QuadPart) / 1000.0;
}

Log::~Log() {
    Log::print<INFO>("Shutting down BetterVR debugging console...");
    FreeConsole();
#ifndef _DEBUG
    if (logFile.is_open()) {
        logFile.close();
    }
#endif
}

void Log::printTimeElapsed(const char* message_prefix, LARGE_INTEGER time) {
    LARGE_INTEGER timeNow;
    QueryPerformanceCounter(&timeNow);
    Log::print<INFO>("{}: {} ms", message_prefix, double(time.QuadPart - timeNow.QuadPart) / timeFrequency);
}