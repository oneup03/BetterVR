#include <pch.h>

#include "hooking/cemu_hooks.h"
#include "rendering/d3d12.h"
#include "rendering/openxr.h"
#include "rendering/renderer.h"
#include "rendering/vulkan.h"

class VRManager {
public:
    static VRManager& instance() {
        static VRManager singletonInstance;
        return singletonInstance;
    }

    VRManager(VRManager const&) = delete;
    void operator=(VRManager const&) = delete;

    void Init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
        D3D12 = std::make_unique<RND_D3D12>();
        VK = std::make_unique<RND_Vulkan>(instance, physicalDevice, device);
        Log::print("Initialized BotW VR instance...");
    }

    void InitSession() {
        XrGraphicsBindingD3D12KHR d3d12Binding = { XR_TYPE_GRAPHICS_BINDING_D3D12_KHR };
        d3d12Binding.device = D3D12->GetDevice();
        d3d12Binding.queue = D3D12->GetCommandQueue();
        XR->CreateSession(d3d12Binding);
        Hooks = std::make_unique<CemuHooks>();
    }

    std::unique_ptr<OpenXR> XR;
    std::unique_ptr<RND_D3D12> D3D12;
    std::unique_ptr<RND_Vulkan> VK;
    std::unique_ptr<CemuHooks> Hooks;

private:
    VRManager() {
        m_logger = std::make_unique<Log>();

        XR = std::make_unique<OpenXR>();
    };

    ~VRManager() {
        // note: OpenXR gets to remove its swapchains first before D3D12 gets destroyed, so reverse that order
        VK.reset();
        XR.reset();
        D3D12.reset();

        m_logger.reset();
    };

    std::unique_ptr<Log> m_logger;
};

static VRManager* get_instance(const vkroots::VkInstanceDispatch* pDispatch) {
    if (pDispatch->UserData == NULL) {
        return nullptr;
    }
    return reinterpret_cast<VRManager*>(pDispatch->UserData);
}

static VRManager* get_instance(const vkroots::VkDeviceDispatch* pDispatch) {
    if (pDispatch->UserData == NULL) {
        return nullptr;
    }
    return reinterpret_cast<VRManager*>(pDispatch->UserData);
}

static_assert(sizeof(vkroots::VkDeviceDispatch::UserData) == sizeof(uint64_t), "UserData pointer size must be 64 bits");