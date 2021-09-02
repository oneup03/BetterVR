#pragma once
#include "pch.h"

// single global lock, for simplicity
extern std::mutex global_lock;
typedef std::lock_guard<std::mutex> scoped_lock;

// use the loader's dispatch table pointer as a key for dispatch map lookups
template<typename DispatchableType>
void* GetKey(DispatchableType inst) {
	return *(void**)inst;
}

// layer book-keeping information, to store dispatch tables by key
extern std::map<void*, VkLayerInstanceDispatchTable> instance_dispatch;
extern std::map<void*, VkLayerDispatchTable> device_dispatch;

// hook functions
VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage);
VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView);
VK_LAYER_EXPORT void VKAPI_CALL Layer_UpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies);
VK_LAYER_EXPORT void VKAPI_CALL Layer_CmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass);
VK_LAYER_EXPORT void VKAPI_CALL Layer_CmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents);
VK_LAYER_EXPORT void VKAPI_CALL Layer_CmdEndRenderPass(VkCommandBuffer commandBuffer);
VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_QueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_EndCommandBuffer(VkCommandBuffer commandBuffer);


// enums and structs

enum class SWAP_SIDE {
	LEFT,
	RIGHT,
};

enum class RENDER_STATUS {
	WAITING,
	IMAGE_COPIED,
	IMAGE_MUTEXED
};

struct sideTextureResources {
	bool initialized = false;
	uint32_t width;
	uint32_t height;
	double copiedTime;
	double mutexTime;
	std::atomic_uint64_t copiedTimeCounter = 0;
	std::atomic_uint64_t mutexTimeCounter = 0;
	std::atomic_uint64_t presentedTimeCounter = 0;
	HANDLE externalHandle;
	IDXGIKeyedMutex* keyedMutex;
	ID3D11Texture2D* dx11Texture;
	ID3D11Resource* dx11TextureResource;
	ID3D11ShaderResourceView* dx11ResourceView;
	VkImage vkImage;
	VkDeviceMemory vkImageMemory;
};

// internal functions of all the subsystems
bool cemuInitialize();

void* cemuFindAddressUsingAOBScan(const char* aobString, uint64_t searchStart = 0, uint64_t searchEnd = 0xFFFFFFFF/*End of Wii U memory*/);
void* cemuFindAddressUsingBytes(const char* bytes, uint32_t bytesSize, uint64_t searchStart = 0, uint64_t searchEnd = UINT64_MAX);

void dx11Initialize();
void dx11Thread();
void dx11Render();
void dx11RenderLayer(ID3D11Texture2D* swapchainTargetTexture, sideTextureResources* sideTexture);
void dx11CreateSideTexture(sideTextureResources* texture);
bool dx11ShouldRender();
bool dx11IsRunning();
void dx11Shutdown();

void cameraInitialize();
void cameraHookUpdate(PPCInterpreter_t* hCPU);
void cameraHookFrame(PPCInterpreter_t* hCPU);
void cameraHookInterface(PPCInterpreter_t* hCPU);
float cameraGetMenuZoom();
bool cameraUseSwappedFlipMode();
float cameraGetEyeSeparation();
float cameraGetZoomOutLevel();
bool cameraIsInGame();

void logInitialize();
void logShutdown();
void logPrint(const char* message);
void logPrint(const std::string_view& message_view);
void logTimeElapsed(char* prefixMessage, LARGE_INTEGER time);
void checkXRResult(XrResult result, const char* errorMessage = nullptr);
void checkHResult(HRESULT result, const char* errorMessage = nullptr);
void checkMutexHResult(HRESULT result);
void checkVkResult(VkResult result, const char* errorMessage = nullptr);

void modifyInstanceExtensions(VkInstanceCreateInfo* instanceCreateInfo);
void modifyDeviceExtensions(VkDeviceCreateInfo* deviceCreateInfo);

// global variables
extern VkDevice deviceHandle;
extern VkPhysicalDevice physicalDeviceHandle;
extern VkInstance instanceHandle;

static const bool sideBySideRenderingMode = true;

extern sideTextureResources leftEyeResources;
extern sideTextureResources rightEyeResources;
extern XrView leftView;
extern XrView rightView;

extern std::atomic<SWAP_SIDE> currSwapSide;
extern std::atomic<RENDER_STATUS> currRenderStatus;

static bool initializeLayer() {
	if (!cemuInitialize()) {
		// Vulkan layer is hooking something that's not Cemu
		return false;
	}
	logInitialize();
	cameraInitialize();
	return true;
}

static void shutdownLayer() {
	logShutdown();
	if (dx11IsRunning()) dx11Shutdown();
}