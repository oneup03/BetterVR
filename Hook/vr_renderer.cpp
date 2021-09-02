#include "layer.h"

vr::IVRSystem* BetterVR::VRRendering::vrSystem = nullptr;
bool BetterVR::VRRendering::createdVRResources = false;

VkCommandBuffer BetterVR::VRRendering::sourceCommandBuffer = nullptr;
VkImage BetterVR::VRRendering::sourceTexture = nullptr;

VkImage BetterVR::VRRendering::presentImage = VK_NULL_HANDLE;
VkExtent2D BetterVR::VRRendering::presentTextureSize = {};
VkPhysicalDevice BetterVR::physicalDeviceHandle = nullptr;
VkDeviceMemory BetterVR::VRRendering::presentTextureMemory = VK_NULL_HANDLE;

void BetterVR::VRRendering::Initialize() {
	vr::EVRInitError initializeResult;
	vrSystem = vr::VR_Init(&initializeResult, vr::VRApplication_Scene);
	if (initializeResult != vr::VRInitError_None) {
		Logging::Print("Couldn't initialize OpenVR! Make sure that you have Steam running perhaps?");
		exit(0);
	}
}

static bool getMemoryTypeFromProperties(const VkPhysicalDeviceMemoryProperties& memoryProperties, uint32_t nMemoryTypeBits, VkMemoryPropertyFlags nMemoryProperties, uint32_t* pTypeIndexOut) {
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
		if ((nMemoryTypeBits & 1) == 1) {
			if ((memoryProperties.memoryTypes[i].propertyFlags & nMemoryProperties) == nMemoryProperties) {
				*pTypeIndexOut = i;
				return true;
			}
		}
		nMemoryTypeBits >>= 1;
	}
	return false;
}

VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties = {};

void BetterVR::VRRendering::SetupTexture(uint32_t width, uint32_t height) {
	if (!VRRendering::createdVRResources) {
		VRRendering::createdVRResources = true;
		presentTextureSize.width = width;
		presentTextureSize.height = height;

		VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageInfo.flags = 0;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.extent.width = presentTextureSize.width;
		imageInfo.extent.height = presentTextureSize.height;
		imageInfo.extent.depth = 1;
		if (vulkan_layer_factory::CreateImage(deviceHandle, &imageInfo, nullptr, &presentImage) != VK_SUCCESS) {
			Logging::Print("Failed to create texture for the VR presenting!");
			throw std::runtime_error("Failed Creating Presenting Texture");
		}

		VkMemoryRequirements imageMemoryRequirements = {};
		vulkan_layer_factory::GetImageMemoryRequirements(deviceHandle, presentImage, &imageMemoryRequirements);

		VkMemoryAllocateInfo imageMemoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		imageMemoryAllocateInfo.allocationSize = imageMemoryRequirements.size;
		getMemoryTypeFromProperties(physicalDeviceMemoryProperties, imageMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &imageMemoryAllocateInfo.memoryTypeIndex);
		
		if (vulkan_layer_factory::AllocateMemory(deviceHandle, &imageMemoryAllocateInfo, nullptr, &presentTextureMemory) != VK_SUCCESS) {
			Logging::Print("Failed to allocate memory for the VR presenting texture!");
			throw std::runtime_error("Failed Allocating Memory");
		}
		if (vulkan_layer_factory::BindImageMemory(deviceHandle, presentImage, presentTextureMemory, 0) != VK_SUCCESS) {
			Logging::Print("Failed to bind image to memory for the VR presenting texture!");
			throw std::runtime_error("Failed Binding Memory");
		}

		Logging::Print(std::string("Successfully created and allocated the ") + std::to_string(width) + "x" + std::to_string(height) + " VR presenting texture!");
	}
}

void BetterVR::PostCallGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
	if (VRCemu::IsCemuParentProcess()) {
		Logging::Print("Acquired the physical device memory properties!");
		physicalDeviceMemoryProperties = *pMemoryProperties;
	}
}

VkResult BetterVR::PreCallCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
	//if (VRCemu::IsCemuParentProcess()) {
	//	Logging::Print("Found the instance handle that Cemu is using!");
	//	instanceHandle = instance;
	//}
	return VK_SUCCESS;
}

bool hookingCemuInstance = false;

static std::vector<std::string> instanceExtensionArrayStrings;
static std::vector<const char*> instanceExtensionArrayCStrings;

VkResult BetterVR::PreCallCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
	VkInstanceCreateInfo* modifiableCreateInfo = const_cast<VkInstanceCreateInfo*>(pCreateInfo);
	if (VRCemu::IsCemuParentProcess() && !hookingCemuInstance && false) {
		hookingCemuInstance = true;
		// Get required instance extensions from OpenVR
		uint32_t requiredExtensionsStrLength = vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(NULL, 0);
		char* requiredExtensionsCStr = new char[requiredExtensionsStrLength];
		vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(requiredExtensionsCStr, requiredExtensionsStrLength);
		
		hookingCemuInstance = false;
		std::string requiredExtensionsStr(requiredExtensionsCStr);
		delete[] requiredExtensionsCStr;

		std::string newExtension;
		for (char& c : requiredExtensionsStr) {
			if (c == ' ') {
				newExtension += '\0';
				instanceExtensionArrayStrings.emplace_back(std::string(newExtension));
				newExtension = "";
			}
			else newExtension += c;
		}

		// Add old extensions back
		for (uint32_t i=0; i<modifiableCreateInfo->enabledExtensionCount; i++) {
			instanceExtensionArrayStrings.emplace_back(modifiableCreateInfo->ppEnabledExtensionNames[i]);
		}

		for (std::string& str : instanceExtensionArrayStrings) {
			if (std::find(instanceExtensionArrayCStrings.begin(), instanceExtensionArrayCStrings.end(), str) == instanceExtensionArrayCStrings.end()) {
				instanceExtensionArrayCStrings.emplace_back(str.c_str());
			}
		}

		modifiableCreateInfo->enabledExtensionCount = (uint32_t)instanceExtensionArrayCStrings.size();
		modifiableCreateInfo->ppEnabledExtensionNames = instanceExtensionArrayCStrings.data();

		Logging::Print("Successfully added the vulkan extensions that OpenVR requires to Cemu's Vulkan instance creation!");
	}
	return VK_SUCCESS;
}

static bool onlyFirst = false;

VkResult BetterVR::PostCallCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance, VkResult result) {
	instanceHandle = *pInstance;
	Logging::Print("Tried to get another instance!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	return result;
}

static std::vector<std::string> extensionArrayStrings;
static std::vector<const char*> extensionArrayCStrings;

VkResult BetterVR::PreCallCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
	if (VRCemu::IsCemuParentProcess()) {
		VkDeviceCreateInfo* modifiableCreateInfo = const_cast<VkDeviceCreateInfo*>(pCreateInfo);

		//if (finalPhysicalDevice == VK_NULL_HANDLE) {
		//	Logging::Print("Couldn't find the physical device that the HMD wanted when enumerating them...");
		//	throw std::runtime_error("Unknown Physical Device When Enumerating");
		//}

		//if (finalPhysicalDevice != physicalDevice) {
		//	MessageBox(NULL, "The GPU used by Cemu is different from the one from VR headset. Try changing the VR headset's HDMI ports or change the selected device in Cemu's settings!", "Physical device mismatch!", NULL);
		//	throw std::runtime_error("Vulkan Physical Device Mismatch");
		//}

		// Check which physical device that should be used

		uint64_t hmdPhysicalDevice = 0;
		BetterVR::VRRendering::vrSystem->GetOutputDevice(&hmdPhysicalDevice, vr::TextureType_Vulkan, instanceHandle);

		uint32_t queriedDeviceAmount = 0;
		vkEnumeratePhysicalDevices(instanceHandle, &queriedDeviceAmount, nullptr);

		std::vector<VkPhysicalDevice> vulkanPhysicalDevices;
		vulkanPhysicalDevices.resize(queriedDeviceAmount);
		vkEnumeratePhysicalDevices(instanceHandle, &queriedDeviceAmount, vulkanPhysicalDevices.data());

		for (uint32_t i = 0; i < queriedDeviceAmount; i++) {
			if ((VkPhysicalDevice)hmdPhysicalDevice == vulkanPhysicalDevices[i]) {
				physicalDeviceHandle = (VkPhysicalDevice)hmdPhysicalDevice;
			}
		}

		physicalDeviceHandle = vulkanPhysicalDevices.at(0);

		// Get required device extensions from OpenVR
		uint32_t requiredExtensionsStrLength = vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(physicalDeviceHandle, NULL, 0);
		char* requiredExtensionsCStr = new char[requiredExtensionsStrLength];
		vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(physicalDeviceHandle, requiredExtensionsCStr, requiredExtensionsStrLength);
		std::string requiredExtensionsStr(requiredExtensionsCStr);
		delete[] requiredExtensionsCStr;

		std::string newExtension;
		for (char& c : requiredExtensionsStr) {
			if (c == ' ') {
				newExtension += '\0';
				extensionArrayStrings.emplace_back(std::string(newExtension));
				newExtension = "";
			}
			else newExtension += c;
		}

		// Add old extensions back
		extensionArrayStrings.clear();
		extensionArrayCStrings.clear();

		for (uint32_t i = 0; i < modifiableCreateInfo->enabledExtensionCount; i++) {
			extensionArrayStrings.emplace_back(modifiableCreateInfo->ppEnabledExtensionNames[i]);
		}

		// Make array of C string pointers to all the extensions
		for (std::string& str : extensionArrayStrings) {
			if (std::find(extensionArrayCStrings.begin(), extensionArrayCStrings.end(), str) == extensionArrayCStrings.end()) {
				extensionArrayCStrings.emplace_back(str.c_str());
			}
		}

		modifiableCreateInfo->enabledExtensionCount = (uint32_t)extensionArrayCStrings.size();
		modifiableCreateInfo->ppEnabledExtensionNames = extensionArrayCStrings.data();


		//VkDebugReportCallbackCreateInfoEXT callbackCreateInfo{};
		//callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		//callbackCreateInfo.pNext = nullptr;
		//callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
		//	VK_DEBUG_REPORT_WARNING_BIT_EXT;// |
		//	//VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		//callbackCreateInfo.pfnCallback = &BetterVR::Logging::VulkanDebugCallback;
		//callbackCreateInfo.pUserData = nullptr;

		////PFN_vkCreateDebugReportCallbackEXT ptrfunc_vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instanceHandle, "vkCreateDebugReportCallbackEXT"));

		//VkDebugReportCallbackEXT callback;
		//VkResult result = vulkan_layer_factory::CreateDebugReportCallbackEXT(instanceHandle, &callbackCreateInfo, nullptr, &callback);

		//if (result != VK_SUCCESS) {
		//	Logging::Print("Couldn't add the debug reporting callback!");
		//}
	}
	return VK_SUCCESS;
}

void BetterVR::VRRendering::Shutdown() {
	Logging::Print("OpenVR shutting down...");
	vr::VR_Shutdown();
}

// Frame Rendering

uint32_t skipFrames = 160;
bool initializedTexture = false;

void BetterVR::VRRendering::CopyTexture(VkCommandBuffer commandBuffer, VkImage sourceImage) {
	if (skipFrames > 0) {
		skipFrames--;
		return;
	}

	VkImageMemoryBarrier copyToBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	copyToBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyToBarrier.subresourceRange.baseMipLevel = 0;
	copyToBarrier.subresourceRange.levelCount = 1;
	copyToBarrier.subresourceRange.baseArrayLayer = 0;
	copyToBarrier.subresourceRange.layerCount = 1;
	copyToBarrier.oldLayout = initializedTexture ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
	copyToBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	copyToBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	copyToBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	copyToBarrier.image = presentImage;
	vulkan_layer_factory::CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &copyToBarrier);
	initializedTexture = true;

	VkImageCopy copyRegion = {};
	copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.srcSubresource.layerCount = 1;
	copyRegion.srcSubresource.mipLevel = 0;
	copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.dstSubresource.layerCount = 1;
	copyRegion.dstSubresource.mipLevel = 0;
	copyRegion.extent.width = max(1u, presentTextureSize.width >> 1);
	copyRegion.extent.height = max(1u, presentTextureSize.height >> 1);
	copyRegion.extent.depth = 1;

	vulkan_layer_factory::CmdCopyImage(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_GENERAL, presentImage, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
	Logging::Print("Successfully copied the source image to the VR presenting image!");

	VkImageMemoryBarrier optimalFormatBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	optimalFormatBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	optimalFormatBarrier.subresourceRange.baseMipLevel = 0;
	optimalFormatBarrier.subresourceRange.levelCount = 1;
	optimalFormatBarrier.subresourceRange.baseArrayLayer = 0;
	optimalFormatBarrier.subresourceRange.layerCount = 1;
	optimalFormatBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	optimalFormatBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	optimalFormatBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	optimalFormatBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	optimalFormatBarrier.image = presentImage;
	vulkan_layer_factory::CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &optimalFormatBarrier);
}


VkResult BetterVR::PostCallQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence, VkResult result) {
	if (skipFrames == 0) {
		Logging::Print("Successfully submitted some junk too!");
	}
	return result;
}


bool flipping = true;

void BetterVR::VRRendering::RenderTexture(VkQueue queue) {
	if (skipFrames != 0) return;
	flipping = !flipping;

	vr::VRVulkanTextureData_t vulkanTextureData = {};
	vulkanTextureData.m_nImage = (uint64_t)presentImage;
	vulkanTextureData.m_pDevice = deviceHandle;
	vulkanTextureData.m_pPhysicalDevice = physicalDeviceHandle;
	vulkanTextureData.m_pInstance = instanceHandle;
	vulkanTextureData.m_pQueue = queue;
	vulkanTextureData.m_nQueueFamilyIndex = 0;
	vulkanTextureData.m_nFormat = VK_FORMAT_R8G8B8A8_UNORM;
	vulkanTextureData.m_nWidth = presentTextureSize.width;
	vulkanTextureData.m_nHeight = presentTextureSize.height;
	vulkanTextureData.m_nSampleCount = 4;

	vr::Texture_t textureData = {};
	textureData.handle = &vulkanTextureData;
	textureData.eType = vr::TextureType_Vulkan;
	textureData.eColorSpace = vr::ColorSpace_Auto;

	vr::VRTextureBounds_t textureBounds = {0.0f, 0.0f, 1.0f, 1.0f};

	vr::VRCompositor()->Submit(vr::EVREye::Eye_Left, &textureData, &textureBounds, vr::Submit_Default);
	vr::VRCompositor()->Submit(vr::EVREye::Eye_Right, &textureData, &textureBounds, vr::Submit_Default);
}

void BetterVR::PreCallDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
	//VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	//createInfo.attachmentCount = 0;
	//createInfo.dependencyCount = 0;
	//createInfo.subpassCount = 0;
	//VkRenderPass fakeRenderPass = VK_NULL_HANDLE;
	//vulkan_layer_factory::CreateRenderPass(device, &createInfo, pAllocator, &fakeRenderPass);
	//BetterVR
}

void BetterVR::PreCallDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
}

void BetterVR::PreCallDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
}

BetterVR better_vr_instance;