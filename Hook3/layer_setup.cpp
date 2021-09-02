#include "layer.h"

constexpr const char* const_BetterVR_Layer_Name = "VK_LAYER_BetterVR_Layer";
constexpr const char* const_BetterVR_Layer_Description = "Vulkan layer used to breath some VR into BotW for Cemu, using OpenXR!";

std::mutex global_lock;

std::map<void*, VkLayerInstanceDispatchTable> instance_dispatch;
std::map<void*, VkLayerDispatchTable> device_dispatch;

// Layer init and shutdown
std::atomic_bool hookedCemu = false;

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
	if (initializeLayer()) hookedCemu = true;
	VkLayerInstanceCreateInfo* layerCreateInfo = (VkLayerInstanceCreateInfo*)pCreateInfo->pNext;

	// step through the chain of pNext until we get to the link info
	while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
		layerCreateInfo = (VkLayerInstanceCreateInfo*)layerCreateInfo->pNext;
	}

	if (layerCreateInfo == NULL) {
		// No loader instance create info
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	// move chain on for next layer
	layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

	PFN_vkCreateInstance createFunc = (PFN_vkCreateInstance)gpa(VK_NULL_HANDLE, "vkCreateInstance");

	if (hookedCemu) modifyInstanceExtensions(const_cast<VkInstanceCreateInfo*>(pCreateInfo));
	createFunc(pCreateInfo, pAllocator, pInstance);
	instanceHandle = *pInstance;

	// fetch our own dispatch table for the functions we need, into the next layer
	VkLayerInstanceDispatchTable dispatchTable;
	dispatchTable.GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)gpa(*pInstance, "vkGetInstanceProcAddr");
	dispatchTable.DestroyInstance = (PFN_vkDestroyInstance)gpa(*pInstance, "vkDestroyInstance");
	dispatchTable.EnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)gpa(*pInstance, "vkEnumerateDeviceExtensionProperties");

	dispatchTable.GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)gpa(*pInstance, "vkGetPhysicalDeviceMemoryProperties");
	dispatchTable.GetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)gpa(*pInstance, "vkGetPhysicalDeviceProperties2");
	dispatchTable.GetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR)gpa(*pInstance, "vkGetPhysicalDeviceProperties2KHR");
	dispatchTable.CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)gpa(*pInstance, "vkCreateWin32SurfaceKHR");
	dispatchTable.GetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)gpa(*pInstance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
	dispatchTable.GetPhysicalDeviceImageFormatProperties2 = (PFN_vkGetPhysicalDeviceImageFormatProperties2)gpa(*pInstance, "vkGetPhysicalDeviceImageFormatProperties2");

	// store the table by key
	{
		scoped_lock l(global_lock);
		instance_dispatch[GetKey(*pInstance)] = dispatchTable;
	}

	return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL Layer_DestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
	if (hookedCemu) shutdownLayer();
	scoped_lock l(global_lock);
	instance_dispatch.erase(GetKey(instance));
}

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
	VkLayerDeviceCreateInfo* layerCreateInfo = (VkLayerDeviceCreateInfo*)pCreateInfo->pNext;

	// step through the chain of pNext until we get to the link info
	while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
		layerCreateInfo = (VkLayerDeviceCreateInfo*)layerCreateInfo->pNext;
	}

	if (layerCreateInfo == NULL) {
		// No loader instance create info
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	PFN_vkGetDeviceProcAddr gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
	// move chain on for next layer
	layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

	PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice)gipa(VK_NULL_HANDLE, "vkCreateDevice");

	physicalDeviceHandle = physicalDevice;
	if (hookedCemu) modifyDeviceExtensions(const_cast<VkDeviceCreateInfo*>(pCreateInfo));
	VkResult ret = createFunc(physicalDevice, pCreateInfo, pAllocator, pDevice);
	deviceHandle = *pDevice;

	// fetch our own dispatch table for the functions we need, into the next layer
	VkLayerDispatchTable dispatchTable;
	dispatchTable.GetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)gdpa(*pDevice, "vkGetDeviceProcAddr");
	dispatchTable.DestroyDevice = (PFN_vkDestroyDevice)gdpa(*pDevice, "vkDestroyDevice");

	dispatchTable.CreateImage = (PFN_vkCreateImage)gdpa(*pDevice, "vkCreateImage");
	dispatchTable.CreateImageView = (PFN_vkCreateImageView)gdpa(*pDevice, "vkCreateImageView");
	dispatchTable.UpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)gdpa(*pDevice, "vkUpdateDescriptorSets");
	dispatchTable.CmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)gdpa(*pDevice, "vkCmdBindDescriptorSets");
	dispatchTable.CreateRenderPass = (PFN_vkCreateRenderPass)gdpa(*pDevice, "vkCreateRenderPass");
	dispatchTable.CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)gdpa(*pDevice, "vkCmdBeginRenderPass");
	dispatchTable.CmdEndRenderPass = (PFN_vkCmdEndRenderPass)gdpa(*pDevice, "vkCmdEndRenderPass");
	dispatchTable.QueuePresentKHR = (PFN_vkQueuePresentKHR)gdpa(*pDevice, "vkQueuePresentKHR");
	dispatchTable.QueueSubmit = (PFN_vkQueueSubmit)gdpa(*pDevice, "vkQueueSubmit");

	dispatchTable.AllocateMemory = (PFN_vkAllocateMemory)gdpa(*pDevice, "vkAllocateMemory");
	dispatchTable.BindImageMemory = (PFN_vkBindImageMemory)gdpa(*pDevice, "vkBindImageMemory");
	dispatchTable.GetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)gdpa(*pDevice, "vkGetImageMemoryRequirements");
	dispatchTable.CreateImage = (PFN_vkCreateImage)gdpa(*pDevice, "vkCreateImage");
	dispatchTable.CmdCopyImage = (PFN_vkCmdCopyImage)gdpa(*pDevice, "vkCmdCopyImage");
	dispatchTable.CmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)gdpa(*pDevice, "vkCmdPipelineBarrier");
	dispatchTable.GetImageMemoryRequirements2 = (PFN_vkGetImageMemoryRequirements2)gdpa(*pDevice, "vkGetImageMemoryRequirements2");
	dispatchTable.EndCommandBuffer = (PFN_vkEndCommandBuffer)gdpa(*pDevice, "vkEndCommandBuffer");

	// store the table by key
	{
		scoped_lock l(global_lock);
		device_dispatch[GetKey(*pDevice)] = dispatchTable;
	}
	return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL Layer_DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
	scoped_lock l(global_lock);
	device_dispatch.erase(GetKey(device));
}

// Enumeration function

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_EnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
	if (pPropertyCount) *pPropertyCount = 1;

	if (pProperties) {
		strcpy_s(pProperties->layerName, const_BetterVR_Layer_Name);
		strcpy_s(pProperties->description, const_BetterVR_Layer_Description);
		pProperties->implementationVersion = 1;
		pProperties->specVersion = VK_MAKE_VERSION(1,2,154);
	}

	return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_EnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
	return Layer_EnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_EnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
	if (pLayerName == NULL || strcmp(pLayerName, const_BetterVR_Layer_Name))
		return VK_ERROR_LAYER_NOT_PRESENT;

	// don't expose any extensions
	if (pPropertyCount) *pPropertyCount = 0;
	return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_EnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
	// pass through any queries that aren't to us
	if (pLayerName == NULL || strcmp(pLayerName, const_BetterVR_Layer_Name)) {
		if (physicalDevice == VK_NULL_HANDLE)
			return VK_SUCCESS;

		scoped_lock l(global_lock);
		return instance_dispatch[GetKey(physicalDevice)].EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
	}

	// don't expose any extensions
	if (pPropertyCount) *pPropertyCount = 0;
	return VK_SUCCESS;
}

#define GETPROCADDR(func) if(!strcmp(pName, "vk" #func)) return (PFN_vkVoidFunction)&Layer_##func;

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL Layer_GetDeviceProcAddr(VkDevice device, const char* pName) {
	GETPROCADDR(GetDeviceProcAddr);
	GETPROCADDR(CreateDevice);
	GETPROCADDR(DestroyDevice);

	GETPROCADDR(EnumerateDeviceLayerProperties);
	GETPROCADDR(EnumerateDeviceExtensionProperties);

	// device chain functions we intercept
	if (hookedCemu) {
		GETPROCADDR(CreateImageView);
		GETPROCADDR(UpdateDescriptorSets);
		GETPROCADDR(CmdBindDescriptorSets);
		GETPROCADDR(CreateRenderPass);
		GETPROCADDR(CmdBeginRenderPass);
		GETPROCADDR(CmdEndRenderPass);
		GETPROCADDR(QueuePresentKHR);

		GETPROCADDR(CreateImage);
		GETPROCADDR(QueueSubmit);
		GETPROCADDR(EndCommandBuffer);
	}

	{
		scoped_lock l(global_lock);
		return device_dispatch[GetKey(device)].GetDeviceProcAddr(device, pName);
	}
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL Layer_GetInstanceProcAddr(VkInstance instance, const char* pName) {
	GETPROCADDR(GetInstanceProcAddr);
	GETPROCADDR(GetDeviceProcAddr);

	GETPROCADDR(CreateInstance);
	GETPROCADDR(DestroyInstance);
	GETPROCADDR(EnumerateInstanceLayerProperties);
	GETPROCADDR(EnumerateInstanceExtensionProperties);

	GETPROCADDR(EnumerateDeviceLayerProperties);
	GETPROCADDR(EnumerateDeviceExtensionProperties);
	GETPROCADDR(CreateDevice);
	GETPROCADDR(DestroyDevice);

	if (hookedCemu) {
		// instance chain functions we intercept
		GETPROCADDR(CreateImageView);
		GETPROCADDR(UpdateDescriptorSets);
		GETPROCADDR(CmdBindDescriptorSets);
		GETPROCADDR(CreateRenderPass);
		GETPROCADDR(CmdBeginRenderPass);
		GETPROCADDR(CmdEndRenderPass);
		GETPROCADDR(QueuePresentKHR);
		GETPROCADDR(QueueSubmit);

		// device chain functions we intercept
		GETPROCADDR(CreateImage);
		GETPROCADDR(EndCommandBuffer);
	}

	{
		scoped_lock l(global_lock);
		return instance_dispatch[GetKey(instance)].GetInstanceProcAddr(instance, pName);
	}
}