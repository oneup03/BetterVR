#pragma once

#include <unordered_map>
#include <sstream>

#include "vulkan/vulkan.h"
#include "vk_layer_logging.h"
#include "layer_factory.h"

#include <openvr.h>

class BetterVR : public layer_factory {
public:
    // Constructor for state_tracker
    BetterVR() {
        VRCemu::Initialize();
        if (!VRCemu::IsCemuParentProcess()) return;

        BetterVR::Logging::Initialize();
        BetterVR::VRRendering::Initialize();
        deviceHandle = nullptr;
        instanceHandle = nullptr;
    };

    ~BetterVR() {
        BetterVR::VRRendering::Shutdown();
        BetterVR::Logging::Shutdown();
    }

    VkResult PreCallCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance);
    VkResult PostCallCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance, VkResult result);
    VkResult PreCallCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice);
    void PostCallGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties);
    VkResult PreCallCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
    void PostCallGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue);
    VkResult PostCallGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages, VkResult result);
    VkResult PostCallCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView, VkResult result);
    
    VkResult PostCallCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass, VkResult result);
    void PostCallCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents);
    void PostCallCmdEndRenderPass(VkCommandBuffer commandBuffer);
    
    void PostCallCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
    void PostCallUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies);
    VkResult PreCallQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo);
    
    
    // temp
    VkResult PostCallQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence, VkResult result);
    void PreCallDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator);
    void PreCallDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator);
    void PreCallDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator);
    void PreCallDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator);
private:
    class Logging {
    public:
        static void Initialize();
        static void Shutdown();
        static void Print(std::string& message);
        static void Print(const char* message);
        static VkBool32 VulkanDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData);
        static HANDLE consoleHandle;

        static void printTrackingInfo(VkImage finalImage);
    };

    class VRRendering {
    public:
        static void Initialize();
        static void Shutdown();
        static void SetupTexture(uint32_t width, uint32_t height);
        static void CopyTexture(VkCommandBuffer commandBuffer, VkImage sourceImage);
        static void RenderTexture(VkQueue queue);
        static VkPhysicalDevice BetterVR::VRRendering::getPhysicalDevice();
        static vr::IVRSystem* vrSystem;
        static bool createdVRResources;
        static VkImage sourceTexture;
        static VkCommandBuffer sourceCommandBuffer;
        static VkQueue presentQueue;
        static VkImage presentImage;
        static VkExtent2D presentTextureSize;
        static VkDeviceMemory presentTextureMemory;
    };

    class VRCamera {
    public:
    };

    class VRCemu {
    public:
        static void Initialize();
        static bool IsCemuParentProcess();
    private:
        static bool cemuParentProcess;
    };

    static VkDevice deviceHandle;
    static VkPhysicalDevice physicalDeviceHandle;
    static VkInstance instanceHandle;
    std::unordered_map<VkQueue, uint32_t> queueWithIndexes;
    std::unordered_map<VkImageView, VkImage> allImageviews_map;

    std::unordered_map<VkImage, VkSwapchainKHR> swapchainImages_map;
    std::unordered_map<VkImageView, VkImage> swapchainImageviews_map;
    std::unordered_map<VkFramebuffer, VkImageView> swapchainFramebuffers_map;

    std::unordered_set<VkRenderPass> dontCareOpsRenderPass_set;

    std::unordered_map<VkDescriptorSet, VkImage> potentialUnscaledImages;
    std::vector<VkImage> unscaledImages;
    bool tryMatchingUnscaledImages = false;
};