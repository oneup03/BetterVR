#pragma once
#include "pch.h"

extern uint64_t memoryBaseAddress;

template <typename T>
void swapEndianness(T& val) {
    union U {
        T val;
        std::array<std::uint8_t, sizeof(T)> raw;
    } src, dst;

    src.val = val;
    std::reverse_copy(src.raw.begin(), src.raw.end(), dst.raw.begin());
    val = dst.val;
}

template <typename T>
void writeMemoryBE(uint64_t offset, T value) {
	swapEndianness(value);
	memcpy((void*)(memoryBaseAddress + offset), (void*)&value, sizeof(T));
}

template <typename T>
void writeMemory(uint64_t offset, T value) {
	memcpy((void*)(memoryBaseAddress + offset), (void*)&value, sizeof(T));
}

template <typename T>
void readMemoryBE(uint64_t offset, T* resultPtr) {
	uint64_t memoryAddress = memoryBaseAddress + offset;
	memcpy(resultPtr, (void*)memoryAddress, sizeof(T));
	swapEndianness(resultPtr);
}

template <typename T>
void readMemory(uint64_t offset, T* resultPtr) {
	uint64_t memoryAddress = memoryBaseAddress + offset;
	memcpy(resultPtr, (void*)memoryAddress, sizeof(T));
}