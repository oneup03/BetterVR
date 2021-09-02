#include "layer.h"

VkDevice deviceHandle = VK_NULL_HANDLE;
VkPhysicalDevice physicalDeviceHandle = VK_NULL_HANDLE;
VkInstance instanceHandle = VK_NULL_HANDLE;

std::unordered_map<VkImageView, VkImage> allImageViewsMap;
std::unordered_map<VkImage, VkExtent2D> allImageResolutions;
std::unordered_map<VkDescriptorSet, VkImage> noOpsImages;
std::unordered_set<VkRenderPass> dontCareOpsRenderPassSet;
std::unordered_map<VkDescriptorSet, VkImage> potentialUnscaledImages;
std::vector<VkImage> unscaledImages;
bool tryMatchingUnscaledImages = false;

VkCommandBuffer commandBufferHandle;
VkQueue queueHandle;

std::vector<std::string> instanceExtensionsStrings;
std::vector<const char*> instanceExtensionsCStr;

void modifyInstanceExtensions(VkInstanceCreateInfo* instanceCreateInfo) {
	instanceExtensionsStrings.emplace_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
	instanceExtensionsStrings.emplace_back(VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME);

	for (uint32_t i=0; i<instanceCreateInfo->enabledExtensionCount; i++) {
		instanceExtensionsStrings.push_back(instanceCreateInfo->ppEnabledExtensionNames[i]);
	}

	for (std::string& extensionStr : instanceExtensionsStrings) {
		if (std::find(instanceExtensionsCStr.begin(), instanceExtensionsCStr.end(), extensionStr) == instanceExtensionsCStr.end()) {
			instanceExtensionsCStr.emplace_back(extensionStr.c_str());
		}
	}

	instanceCreateInfo->enabledExtensionCount = (uint32_t)instanceExtensionsCStr.size();
	instanceCreateInfo->ppEnabledExtensionNames = instanceExtensionsCStr.data();

	logPrint("Successfully added the required instance extensions to get interop going!");
}

std::vector<std::string> deviceExtensionsStrings;
std::vector<const char*> deviceExtensionsCStr;

void modifyDeviceExtensions(VkDeviceCreateInfo* deviceCreateInfo) {
	for (uint32_t i = 0; i < deviceCreateInfo->enabledExtensionCount; i++) {
		deviceExtensionsStrings.push_back(deviceCreateInfo->ppEnabledExtensionNames[i]);
	}

	for (std::string& extensionStr : deviceExtensionsStrings) {
		if (std::find(deviceExtensionsCStr.begin(), deviceExtensionsCStr.end(), extensionStr) == deviceExtensionsCStr.end()) {
			deviceExtensionsCStr.emplace_back(extensionStr.c_str());
		}
	}

	deviceCreateInfo->enabledExtensionCount = (uint32_t)deviceExtensionsCStr.size();
	deviceCreateInfo->ppEnabledExtensionNames = deviceExtensionsCStr.data();

	logPrint("Successfully added the required device extensions to get interop going!");
}

VkCommandBuffer presentImageCommandBuffer = VK_NULL_HANDLE;

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

uint64_t timeCounter = 0;
void increaseTimeSignature(double* time, std::atomic_uint64_t* counter) {
	timeCounter++;
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER currTime;
	QueryPerformanceCounter(&currTime);
	*counter = timeCounter;
	*time = double(currTime.QuadPart) / frequency.QuadPart;
}

void importExternalTexture(VkCommandBuffer commandBuffer, sideTextureResources* sideTexture) {
	VkPhysicalDeviceImageFormatInfo2 physicalDeviceImageFormatInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 };
	VkPhysicalDeviceExternalImageFormatInfo externalFormatInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO };
	externalFormatInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
	physicalDeviceImageFormatInfo.pNext = &externalFormatInfo;
	physicalDeviceImageFormatInfo.type = VK_IMAGE_TYPE_2D;
	physicalDeviceImageFormatInfo.format = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
	physicalDeviceImageFormatInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	physicalDeviceImageFormatInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	physicalDeviceImageFormatInfo.flags = 0;
	VkImageFormatProperties2 imageFormatProperties = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 };
	VkExternalImageFormatProperties externalImageProperties = { VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES };
	imageFormatProperties.pNext = &externalImageProperties;
	checkVkResult(instance_dispatch.begin()->second.GetPhysicalDeviceImageFormatProperties2(physicalDeviceHandle, &physicalDeviceImageFormatInfo, &imageFormatProperties), "Failed to get device image properties! Driver might not allow for the requested texture format!");

	if (!(externalImageProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) {
		logPrint("Vulkan driver doesn't support importing external memory!");
		throw std::runtime_error("Vulkan driver doesn't support importing external memory!");
	}

	logPrint("Vulkan driver supports required importing external memory!");

	VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.format = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
	createInfo.extent.width = sideTexture->width;
	createInfo.extent.height = sideTexture->height;
	createInfo.extent.depth = 1;
	createInfo.mipLevels = 1;
	createInfo.arrayLayers = 1;
	createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	createInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	createInfo.flags = 0;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkExternalMemoryImageCreateInfo externalImageCreateInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
	externalImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;

	createInfo.pNext = &externalImageCreateInfo;

	checkVkResult(device_dispatch.begin()->second.CreateImage(deviceHandle, &createInfo, NULL, &sideTexture->vkImage), "Failed to create external image on Vulkan side");

	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties = {};
	instance_dispatch.begin()->second.GetPhysicalDeviceMemoryProperties(physicalDeviceHandle, &physicalDeviceMemoryProperties);

	VkImageMemoryRequirementsInfo2 imageMemoryRequirements = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
	imageMemoryRequirements.image = sideTexture->vkImage;

	VkMemoryDedicatedRequirements dedicatedRequirements = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS };
	VkMemoryRequirements2 memoryRequirements = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
	memoryRequirements.pNext = &dedicatedRequirements;

	device_dispatch.begin()->second.GetImageMemoryRequirements2(deviceHandle, &imageMemoryRequirements, &memoryRequirements);

	VkImportMemoryWin32HandleInfoKHR importMemoryAllocHandle = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };
	importMemoryAllocHandle.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
	importMemoryAllocHandle.handle = sideTexture->externalHandle;
	importMemoryAllocHandle.name = NULL;

	VkMemoryAllocateInfo imageMemoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	getMemoryTypeFromProperties(physicalDeviceMemoryProperties, memoryRequirements.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &imageMemoryAllocateInfo.memoryTypeIndex);
	imageMemoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
	imageMemoryAllocateInfo.pNext = &importMemoryAllocHandle;

	VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	dedicatedAllocInfo.image = sideTexture->vkImage;
	dedicatedAllocInfo.pNext = &importMemoryAllocHandle;
	imageMemoryAllocateInfo.pNext = &dedicatedAllocInfo;

	checkVkResult(device_dispatch.begin()->second.AllocateMemory(deviceHandle, &imageMemoryAllocateInfo, NULL, &sideTexture->vkImageMemory), "Failed to allocate memory for external texture on Vulkan side");

	checkVkResult(device_dispatch.begin()->second.BindImageMemory(deviceHandle, sideTexture->vkImage, sideTexture->vkImageMemory, 0), "Failed to bind image memory of external texture on Vulkan side");

	VkImageMemoryBarrier initializeBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	initializeBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	initializeBarrier.subresourceRange.baseMipLevel = 0;
	initializeBarrier.subresourceRange.levelCount = 1;
	initializeBarrier.subresourceRange.baseArrayLayer = 0;
	initializeBarrier.subresourceRange.layerCount = 1;
	initializeBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	initializeBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	initializeBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	initializeBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	initializeBarrier.image = sideTexture->vkImage;
	device_dispatch.begin()->second.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &initializeBarrier);

	logPrint("Successfully imported a new external image into Vulkan!");
	sideTexture->initialized = true;
}

void copyTexture(VkCommandBuffer commandBuffer, VkImage sourceImage, sideTextureResources* sideTexture) {
	presentImageCommandBuffer = commandBuffer;
	VkImageCopy copyRegion = {};
	copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.srcSubresource.layerCount = 1;
	copyRegion.srcSubresource.mipLevel = 0;
	copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.dstSubresource.layerCount = 1;
	copyRegion.dstSubresource.mipLevel = 0;
	copyRegion.extent.width = sideTexture->width;
	copyRegion.extent.height = sideTexture->height;
	copyRegion.extent.depth = 1;

	device_dispatch[GetKey(commandBuffer)].CmdCopyImage(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_GENERAL, sideTexture->vkImage, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
	//logPrint("Copied the source image to the VR presenting image!");
}

// Track image and imageviews

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_EndCommandBuffer(VkCommandBuffer commandBuffer) {
	VkResult result;
	{
		scoped_lock l(global_lock);
		result = device_dispatch[GetKey(commandBuffer)].EndCommandBuffer(commandBuffer);
	}
	return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
	VkResult result;
	{
		scoped_lock l(global_lock);
		result = device_dispatch[GetKey(device)].CreateImage(device, pCreateInfo, pAllocator, pImage);
	}

	if (!leftEyeResources.initialized && pCreateInfo->extent.width >= pCreateInfo->extent.height && pCreateInfo->extent.width != 0 && pCreateInfo->extent.height != 0) {
		allImageResolutions[*pImage] = VkExtent2D{ pCreateInfo->extent.width, pCreateInfo->extent.height };
	}

	return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
	VkResult result;
	{
		scoped_lock l(global_lock);
		result = device_dispatch[GetKey(device)].CreateImageView(device, pCreateInfo, pAllocator, pView);
	}

	if (pCreateInfo->viewType == VK_IMAGE_VIEW_TYPE_2D) {
		allImageViewsMap.emplace(*pView, pCreateInfo->image);
	}

	return result;
}

VK_LAYER_EXPORT void VKAPI_CALL Layer_UpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
	{
		scoped_lock l(global_lock);
		device_dispatch[GetKey(device)].UpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
	}
	
	if (descriptorWriteCount == 1 && pDescriptorWrites[0].descriptorCount == 1 && (pDescriptorWrites[0].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || pDescriptorWrites[0].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
		auto imageIterator = allImageViewsMap.find(pDescriptorWrites[0].pImageInfo->imageView);
		if (imageIterator != allImageViewsMap.end()) {
			potentialUnscaledImages[pDescriptorWrites->dstSet] = imageIterator->second;
		}
	}
}

VK_LAYER_EXPORT void VKAPI_CALL Layer_CmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
	{
		scoped_lock l(global_lock);
		device_dispatch[GetKey(commandBuffer)].CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
	}
	
	if (tryMatchingUnscaledImages && descriptorSetCount == 1) {
		assert(potentialUnscaledImages.find(pDescriptorSets[0]) != potentialUnscaledImages.end());
		VkExtent2D imageResolution = allImageResolutions[potentialUnscaledImages[pDescriptorSets[0]]];
		if (imageResolution.width != 0 && imageResolution.height != 0) {
			unscaledImages.emplace_back(potentialUnscaledImages[pDescriptorSets[0]]);
		}
		commandBufferHandle = commandBuffer;
	}
}

// Track render passes

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
	VkResult result;
	{
		scoped_lock l(global_lock);
		result = device_dispatch[GetKey(device)].CreateRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
	}
	
	if (pCreateInfo->attachmentCount >= 1 && pCreateInfo->pAttachments->loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE) {
		dontCareOpsRenderPassSet.emplace(*pRenderPass);
	}
	return result;
}

VK_LAYER_EXPORT void VKAPI_CALL Layer_CmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
	if (dontCareOpsRenderPassSet.count(pRenderPassBegin->renderPass) == 1) {
		tryMatchingUnscaledImages = true;
	}

	{
		scoped_lock l(global_lock);
		device_dispatch[GetKey(commandBuffer)].CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
	}
}

VK_LAYER_EXPORT void VKAPI_CALL Layer_CmdEndRenderPass(VkCommandBuffer commandBuffer) {
	{
		scoped_lock l(global_lock);
		device_dispatch[GetKey(commandBuffer)].CmdEndRenderPass(commandBuffer);
	}

	if (tryMatchingUnscaledImages && !unscaledImages.empty()) {
		if (!leftEyeResources.initialized) {
			logPrint("Found unscaled textures:");
			VkExtent2D biggestResolution{};
			for (VkImage image : unscaledImages) {
				VkExtent2D imageResolution = allImageResolutions[image];
				logPrint(std::string("Texture #") + std::to_string((int64_t)image) + ", " + std::to_string(imageResolution.width) + "x" + std::to_string(imageResolution.height));
				if ((biggestResolution.width * biggestResolution.height) < (imageResolution.width * imageResolution.height)) {
					biggestResolution = imageResolution;
				}
			}
			logPrint(std::string("The native resolution of the game's current rendering was guessed to be ")+std::to_string(biggestResolution.width)+"x"+std::to_string(biggestResolution.height));
			if (biggestResolution.width != 0 && biggestResolution.height != 0) {
				dx11Initialize();

				leftEyeResources.width = biggestResolution.width;
				leftEyeResources.height = biggestResolution.height;
				rightEyeResources.width = biggestResolution.width;
				rightEyeResources.height = biggestResolution.height;
				dx11CreateSideTexture(&leftEyeResources);
				dx11CreateSideTexture(&rightEyeResources);
				importExternalTexture(commandBuffer, &leftEyeResources);
				importExternalTexture(commandBuffer, &rightEyeResources);

				std::thread newThread(dx11Thread);
				newThread.detach();
			}
		}
		else {
			for (const VkImage image : unscaledImages) {
				VkExtent2D imageResolution = allImageResolutions[image];
				if (imageResolution.width == leftEyeResources.width && imageResolution.height == leftEyeResources.height) {
					sideTextureResources* currSide = currSwapSide == SWAP_SIDE::LEFT ? &leftEyeResources : &rightEyeResources;
					copyTexture(commandBuffer, image, currSide);
					increaseTimeSignature(&currSide->copiedTime, &currSide->copiedTimeCounter);
					currRenderStatus = RENDER_STATUS::IMAGE_COPIED;
				}
			}

			tryMatchingUnscaledImages = false;
			unscaledImages.clear();
		}
	}
}

// Track frame rendering

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_QueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
	VkWin32KeyedMutexAcquireReleaseInfoKHR keyedMutexAcquireAndRelease = { VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR };
	for (uint32_t i=0; i<submitCount; i++) {
		for (uint32_t j=0; j<pSubmits[i].commandBufferCount; j++) {
			if (pSubmits[i].pCommandBuffers[j] == presentImageCommandBuffer && currRenderStatus == RENDER_STATUS::IMAGE_COPIED) {
				uint64_t waitForKeyValue = 1;
				uint64_t releaseToKeyValue = 1;
				uint32_t acquireTimeout = INFINITE;
				keyedMutexAcquireAndRelease.acquireCount = 1;
				keyedMutexAcquireAndRelease.pAcquireSyncs = currSwapSide == SWAP_SIDE::LEFT ? &leftEyeResources.vkImageMemory : &rightEyeResources.vkImageMemory;
				keyedMutexAcquireAndRelease.pAcquireKeys = &waitForKeyValue;
				keyedMutexAcquireAndRelease.pAcquireTimeouts = &acquireTimeout;
				keyedMutexAcquireAndRelease.releaseCount = 1;
				keyedMutexAcquireAndRelease.pReleaseSyncs = currSwapSide == SWAP_SIDE::LEFT ? &leftEyeResources.vkImageMemory : &rightEyeResources.vkImageMemory;
				keyedMutexAcquireAndRelease.pReleaseKeys = &releaseToKeyValue;
				const_cast<VkSubmitInfo*>(pSubmits)[i].pNext = &keyedMutexAcquireAndRelease;
				//logPrint("Vulkan: Acquire mutex");
			}
		}
	}
	VkResult result;
	{
		scoped_lock l(global_lock);
		result = device_dispatch[GetKey(queue)].QueueSubmit(queue, submitCount, pSubmits, fence);
	}
	if (pSubmits[0].pNext != nullptr) {
		//logPrint("Vulkan: Release mutex");
		currRenderStatus = RENDER_STATUS::IMAGE_MUTEXED;
		sideTextureResources* currSide = currSwapSide == SWAP_SIDE::LEFT ? &leftEyeResources : &rightEyeResources;
		increaseTimeSignature(&currSide->mutexTime, &currSide->mutexTimeCounter);
	}
	return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
	if (leftEyeResources.initialized) {
		currRenderStatus = RENDER_STATUS::WAITING;

		if (currSwapSide == SWAP_SIDE::LEFT) {
			//logPrint("Finished rendering left-side on Vulkan side!");
			currSwapSide = SWAP_SIDE::RIGHT;
		}
		else if (currSwapSide == SWAP_SIDE::RIGHT) {
			//logPrint("Finished rendering right-side on Vulkan side!");
			currSwapSide = SWAP_SIDE::LEFT;
		}
	}
	scoped_lock l(global_lock);
	return device_dispatch[GetKey(queue)].QueuePresentKHR(queue, pPresentInfo);
}