#include "layer.h"
#include <fmt/core.h>

VkDevice BetterVR::deviceHandle = nullptr;
VkInstance BetterVR::instanceHandle = nullptr;

void BetterVR::PostCallGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
	if (VRCemu::IsCemuParentProcess()) {
		Logging::Print("Found the device handle of Cemu!");
		deviceHandle = device;
		// todo: check how to gracefully handle family indexes. Cemu seems to usually use 0 though.
	}
}

// ---------------------------------------

VkResult BetterVR::PostCallGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages, VkResult result) {
	if (VRCemu::IsCemuParentProcess() && pSwapchainImages != NULL) {
		for (uint32_t i = 0; i < *pSwapchainImageCount; i++) {
			swapchainImages_map.emplace(pSwapchainImages[i], swapchain);
			Logging::Print("New swapchain image was found!");
		}
	}
	return result;
}

std::vector<std::string> trackingAllImageViews;

VkResult BetterVR::PostCallCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView, VkResult result) {
	if (pCreateInfo->viewType == VK_IMAGE_VIEW_TYPE_2D) {
		allImageviews_map[*pView] = pCreateInfo->image;
		trackingAllImageViews.emplace_back(fmt::format("VkImage {0:#x}: related imageview = {1:#x}", (uint64_t)pCreateInfo->image, (uint64_t)*pView));
		Logging::Print("New imageview was collected!");
	}
	return result;
}

std::vector<std::string> trackingPotentialUnscaledImages;

void BetterVR::PostCallUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
	if (VRCemu::IsCemuParentProcess() && descriptorWriteCount == 1 && pDescriptorWrites[0].descriptorCount == 1 && (pDescriptorWrites[0].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || pDescriptorWrites[0].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
		for (auto const image : allImageviews_map) {
			if (image.first == pDescriptorWrites[0].pImageInfo->imageView) {
				trackingPotentialUnscaledImages.emplace_back(fmt::format("VkImage {0:#x}: related imageview = {1:#x}, related descriptor set = {2:#x}", (uint64_t)allImageviews_map.at(pDescriptorWrites[0].pImageInfo->imageView), (uint64_t)pDescriptorWrites[0].pImageInfo->imageView, (uint64_t)pDescriptorWrites->dstSet));
				potentialUnscaledImages[pDescriptorWrites->dstSet] = image.second;
			}
		}
	}
}

// ---------------------------------------

VkResult BetterVR::PostCallCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass, VkResult result) {
	if (VRCemu::IsCemuParentProcess() && pCreateInfo->pAttachments->loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE) {
		dontCareOpsRenderPass_set.emplace(*pRenderPass);
	}
	return result;
}

void BetterVR::PostCallCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
	if (VRCemu::IsCemuParentProcess() && dontCareOpsRenderPass_set.count(pRenderPassBegin->renderPass) == 1 && pRenderPassBegin->renderArea.extent.width == 1280) {
		VRRendering::SetupTexture(pRenderPassBegin->renderArea.extent.width, pRenderPassBegin->renderArea.extent.height);
		Logging::Print("Upscaling render pass just started!");
		tryMatchingUnscaledImages = true;
	}
}

std::vector<std::string> trackingUnscaledImages;

void BetterVR::PostCallCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
	if (VRCemu::IsCemuParentProcess() && tryMatchingUnscaledImages && descriptorSetCount == 1) {
		Logging::Print(std::string("Descriptor set with image ") + std::to_string((uint64_t)potentialUnscaledImages[pDescriptorSets[0]]) + " was found which could potentially be a VkImage!");
		VRRendering::sourceCommandBuffer = commandBuffer;

		assert(potentialUnscaledImages.find(pDescriptorSets[0]) != potentialUnscaledImages.end());
		unscaledImages.push_back(potentialUnscaledImages.at(pDescriptorSets[0]));

		auto result = std::find_if(allImageviews_map.begin(), allImageviews_map.end(), [&](const auto& mo) {return mo.second == potentialUnscaledImages.at(pDescriptorSets[0]); });
		assert(result != allImageviews_map.end());

		trackingUnscaledImages.push_back(fmt::format("VkImage {0:#x}: related imageview = {1:#x}, related descriptor set = {2:#x}", (uint64_t)potentialUnscaledImages.at(pDescriptorSets[0]), (uint64_t)result->first, (uint64_t)pDescriptorSets[0]));
	}
}

std::vector<std::string> trackingDeletedImages;

void BetterVR::PreCallDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
	trackingDeletedImages.push_back(fmt::format("VkImage {0:#x}: NOW DELETED!!", (uint64_t)image));
}

void BetterVR::Logging::printTrackingInfo(VkImage finalImage) {
	Logging::Print("");
	Logging::Print("-------------------------------------------------");

	std::string startString = fmt::format("VkImage {0:#x}", (uint64_t)finalImage);

	Logging::Print(std::string("Final Image is ") + startString);
	Logging::Print("-------------------------------------------------");
	Logging::Print("Tracing:\n");

	for (auto& str : trackingAllImageViews) {
		if (str.rfind(startString, 0) == 0) {
			Logging::Print(std::string("When appending AllImageViews			") + str);
		}
	}

	for (auto& str : trackingPotentialUnscaledImages) {
		if (str.rfind(startString, 0) == 0) {
			Logging::Print(std::string("When appending potentialUnscaledImages	") + str);
		}
	}

	for (auto& str : trackingUnscaledImages) {
		if (str.rfind(startString, 0) == 0) {
			Logging::Print(std::string("When appending trackingUnscaledImages	") + str);
		}
	}

	Logging::Print("");

	for (auto& str : trackingDeletedImages) {
		if (str.rfind(startString, 0) == 0) {
			Logging::Print(std::string("										") + str);
		}
	}

	trackingAllImageViews.clear();
	trackingPotentialUnscaledImages.clear();
	trackingUnscaledImages.clear();
	trackingDeletedImages.clear();
}

void BetterVR::PostCallCmdEndRenderPass(VkCommandBuffer commandBuffer) {
	if (VRCemu::IsCemuParentProcess() && tryMatchingUnscaledImages && !unscaledImages.empty()) {
		tryMatchingUnscaledImages = false;
		//Logging::Print(std::string("Upscaling render pass has ended and there were ") + std::to_string(unscaledImages.size()) + " potential unscaled textures!");
		//Logging::printTrackingInfo(unscaledImages[0]);
		VRRendering::CopyTexture(VRRendering::sourceCommandBuffer, unscaledImages[0]);
		unscaledImages.clear();
	}
}

VkResult BetterVR::PreCallQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo) {
	if (VRCemu::IsCemuParentProcess()) {
		// Now find the last drawcall, which framebuffer was related, then which texture view was related, and then which VkImage is related
		// todo: could detect using the results from the presentinfo struct whether a swapchain will be recreated by Cemu (cuz window scale changed) and then remove that one from my stored swapchain images
		BetterVR::VRRendering::RenderTexture(queue);
		Logging::Print("Frame is finished and is presented!");
		//if (VRRendering::sourceTexture != nullptr) {
		//	VRRendering::CopyTexture(VRRendering::sourceCommandBuffer, VRRendering::sourceTexture);
		//}
	}
	return VK_SUCCESS;
}