#include "framebuffer.h"
#include "instance.h"
#include "layer.h"
#include "utils/vulkan_utils.h"

#include <codecvt>


std::mutex lockImageResolutions;
std::unordered_map<VkImage, std::pair<VkExtent2D, VkFormat>> imageResolutions;

std::vector<std::pair<VkCommandBuffer, SharedTexture*>> s_activeCopyOperations;

VkImage s_curr3DColorImage = VK_NULL_HANDLE;
VkImage s_curr3DDepthImage = VK_NULL_HANDLE;

using namespace VRLayer;

VkResult VkDeviceOverrides::CreateImage(const vkroots::VkDeviceDispatch* pDispatch, VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    VkResult res = pDispatch->CreateImage(device, pCreateInfo, pAllocator, pImage);

    if (pCreateInfo->extent.width >= 1280 && pCreateInfo->extent.height >= 720) {
        lockImageResolutions.lock();
        // Log::print("Added texture {}: {}x{} @ {}", (void*)*pImage, pCreateInfo->extent.width, pCreateInfo->extent.height, pCreateInfo->format);
        checkAssert(imageResolutions.try_emplace(*pImage, std::make_pair(VkExtent2D{ pCreateInfo->extent.width, pCreateInfo->extent.height }, pCreateInfo->format)).second, "Couldn't insert image resolution into map!");
        lockImageResolutions.unlock();
    }
    return res;
}

void VkDeviceOverrides::DestroyImage(const vkroots::VkDeviceDispatch* pDispatch, VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    pDispatch->DestroyImage(device, image, pAllocator);

    lockImageResolutions.lock();
    if (imageResolutions.erase(image)) {
        // Log::print("Removed texture {}", (void*)image);
        if (s_curr3DColorImage == image) {
            s_curr3DColorImage = VK_NULL_HANDLE;
        }
        else if (s_curr3DDepthImage == image) {
            s_curr3DDepthImage = VK_NULL_HANDLE;
        }
    }
    lockImageResolutions.unlock();
}

void VkDeviceOverrides::CmdClearColorImage(const vkroots::VkDeviceDispatch* pDispatch, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    // check for magical clear values
    if (pColor->float32[1] >= 0.12 && pColor->float32[1] <= 0.13 && pColor->float32[2] >= 0.97 && pColor->float32[2] <= 0.99) {
        // r value in magical clear value is the capture idx after rounding down
        const long captureIdx = std::lroundf(pColor->float32[0] * 32.0f);
        const OpenXR::EyeSide side = pColor->float32[3] < 0.5f ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;
        checkAssert(captureIdx == 0 || captureIdx == 2, "Invalid capture index!");

        Log::print("[VULKAN] Clearing color image for {} layer for {} side ({})", captureIdx == 0 ? "3D" : "2D", side == OpenXR::EyeSide::LEFT ? "left" : "right", pColor->float32[3]);

        auto* renderer = VRManager::instance().XR->GetRenderer();
        auto& layer3D = renderer->m_layer3D;
        auto& layer2D = renderer->m_layer2D;
        auto& imguiOverlay = VRManager::instance().VK->m_imguiOverlay;

        // initialize the textures of both 2D and 3D layer if either is found since they share the same VkImage and resolution
        if (captureIdx == 0 || captureIdx == 2) {
            if (!layer2D) {
                lockImageResolutions.lock();
                if (const auto it = imageResolutions.find(image); it != imageResolutions.end()) {
                    layer3D = std::make_unique<RND_Renderer::Layer3D>(it->second.first);
                    layer2D = std::make_unique<RND_Renderer::Layer2D>(it->second.first);

                    // Log::print("Found rendering resolution {}x{} @ {} using capture #{}", it->second.first.width, it->second.first.height, it->second.second, captureIdx);
                    if (CemuHooks::GetSettings().Is2DVRViewEnabled()) {
                        imguiOverlay = std::make_unique<RND_Vulkan::ImGuiOverlay>(commandBuffer, it->second.first.width, it->second.first.height, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
                        if (CemuHooks::GetSettings().ShowDebugOverlay()) {
                            VRManager::instance().Hooks->m_entityDebugger = std::make_unique<EntityDebugger>();
                        }
                    }
                }
                else {
                    checkAssert(false, "Couldn't find image resolution in map!");
                }
                lockImageResolutions.unlock();
            }
        }

        if (!VRManager::instance().XR->GetRenderer()->IsInitialized()) {
            return;
        }

        checkAssert(layer3D && layer2D, "Couldn't find 3D or 2D layer!");
        if (captureIdx == 0) {
            // 3D layer - color texture for 3D rendering

            // check if the color texture has the appropriate texture format
            if (s_curr3DColorImage == VK_NULL_HANDLE) {
                lockImageResolutions.lock();
                if (const auto it = imageResolutions.find(image); it != imageResolutions.end()) {
                    if (it->second.second == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
                        s_curr3DColorImage = it->first;
                    }
                }
                lockImageResolutions.unlock();
            }

            if (image != s_curr3DColorImage) {
                Log::print("[VULKAN] Color image is not the same as the current 3D color image! ({} != {})", (void*)image, (void*)s_curr3DColorImage);
                const_cast<VkClearColorValue*>(pColor)[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
                return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
            }

            if (layer3D->HasCopiedColor(side)) {
                // the color texture has already been copied to the layer
                Log::print("[VULKAN] A color texture is already bound for the current frame!");
                const_cast<VkClearColorValue*>(pColor)[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
                return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
            }

            // if (layer3D.GetStatus() == Status3D::LEFT_BINDING_COLOR || layer3D.GetStatus() == Status3D::RIGHT_BINDING_COLOR) {
            //
            // }

            layer3D->PrepareRendering(side);

            // note: This uses vkCmdCopyImage to copy the image to an OpenXR-specific texture. s_activeCopyOperations queues a semaphore for the D3D12 side to wait on.
            SharedTexture* texture = layer3D->CopyColorToLayer(side, commandBuffer, image);
            // Log::print("[VULKAN] Waiting for {} side to be 0", side == OpenXR::EyeSide::LEFT ? "left" : "right");
            s_activeCopyOperations.emplace_back(commandBuffer, texture);
            Log::print("[VULKAN] Queueing up a 3D_COLOR signal inside cmd buffer {} for {} side", (void*)commandBuffer, side == OpenXR::EyeSide::LEFT ? "left" : "right");
            VulkanUtils::DebugPipelineBarrier(commandBuffer);
            VulkanUtils::TransitionLayout(commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);

            if (imguiOverlay && side == OpenXR::EyeSide::RIGHT) {
                float aspectRatio = layer3D->GetAspectRatio(OpenXR::EyeSide::RIGHT);

                // note: Uses vkCmdCopyImage to copy the (right-eye-only) image to the imgui overlay's texture
                imguiOverlay->Draw3DLayerAsBackground(commandBuffer, image, aspectRatio);

                VulkanUtils::DebugPipelineBarrier(commandBuffer);
                VulkanUtils::TransitionLayout(commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
            }

            // clear the image to be transparent to allow for the HUD to be rendered on top of it which results in a transparent HUD layer
            const_cast<VkClearColorValue*>(pColor)[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
            pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);

            VulkanUtils::DebugPipelineBarrier(commandBuffer);
            VulkanUtils::TransitionLayout(commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
            return;
        }
        else if (captureIdx == 2) {
            // 2D layer - color texture for HUD rendering

            // if (layer2D->HasCopied()) {
            //     const_cast<VkClearColorValue*>(pColor)[0] = { 1.0f, 0.0f, 0.0f, 1.0f };
            //     return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
            // }

            if (!layer2D->HasCopied()) {
                // only copy the first attempt at capturing when GX2ClearColor is called with this capture index since the game/Cemu clears the 2D layer twice
                // Log::print("[VULKAN - 2D Layer] Waiting for {} side to be 0", side == OpenXR::EyeSide::LEFT ? "left" : "right");
                SharedTexture* texture = layer2D->CopyColorToLayer(commandBuffer, image);
                s_activeCopyOperations.emplace_back(commandBuffer, texture);
                Log::print("[VULKAN] Queueing up a 2D_COLOR signal inside cmd buffer {} for {} side", (void*)commandBuffer, side == OpenXR::EyeSide::LEFT ? "left" : "right");
                // Log::print("[VULKAN] Signalling for {} side to be 1", side == OpenXR::EyeSide::LEFT ? "left" : "right");
            }

            VulkanUtils::DebugPipelineBarrier(commandBuffer);
            VulkanUtils::TransitionLayout(commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);

            // copy the image to the imgui overlay's texture
            if (VRManager::instance().VK->m_imguiOverlay && VRManager::instance().VK->m_imguiOverlay->m_initialized) {
                VRManager::instance().VK->m_imguiOverlay->DrawHUDLayerAsBackground(commandBuffer, image);
                VulkanUtils::DebugPipelineBarrier(commandBuffer);
                VulkanUtils::TransitionLayout(commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
            }

            // render imgui, and then copy the framebuffer to the 2D layer
            if (VRManager::instance().VK->m_imguiOverlay && VRManager::instance().VK->m_imguiOverlay->m_initialized) {
                VRManager::instance().VK->m_imguiOverlay->Render();
                VRManager::instance().VK->m_imguiOverlay->DrawOverlayToImage(commandBuffer, image);
                VulkanUtils::DebugPipelineBarrier(commandBuffer);
                VulkanUtils::TransitionLayout(commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
            }

            // const_cast<VkClearColorValue*>(pColor)[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
            // pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
        }
        return;
    }
    else {
        return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
    }
}

void VkDeviceOverrides::CmdClearDepthStencilImage(const vkroots::VkDeviceDispatch* pDispatch, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    // check for magical clear values
    if (rangeCount == 1 && pDepthStencil->depth >= 0.011456789 && pDepthStencil->depth <= 0.013456789) {
        // stencil value is the capture idx
        const uint32_t captureIdx = pDepthStencil->stencil;
        const OpenXR::EyeSide side = captureIdx == 1 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;
        checkAssert(captureIdx == 1 || captureIdx == 2, "Invalid capture index!");

        auto& layer3D = VRManager::instance().XR->GetRenderer()->m_layer3D;
        auto& layer2D = VRManager::instance().XR->GetRenderer()->m_layer2D;

        if (!VRManager::instance().XR->GetRenderer()->IsInitialized()) {
            return;
        }

        Log::print("[VULKAN] Clearing depth image for 3D layer for {} side ({})", side == OpenXR::EyeSide::LEFT ? "left" : "right", pDepthStencil->stencil);

        if (captureIdx == 1 || captureIdx == 2) {
            // 3D layer - depth texture for 3D rendering
            if (s_curr3DDepthImage == VK_NULL_HANDLE) {
                lockImageResolutions.lock();
                if (const auto it = imageResolutions.find(image); it != imageResolutions.end()) {
                    Log::print("Depth 3: Found depth image {} with format {} for 3D layer", (void*)it->first, it->second.second);
                    if (it->second.second == VK_FORMAT_D32_SFLOAT) {
                        Log::print(" -> Depth 3: Okay, I resolved the issue, phew");
                        s_curr3DDepthImage = it->first;
                    }
                }
                lockImageResolutions.unlock();
            }

            if (image != s_curr3DDepthImage) {
                Log::print("[VULKAN] Depth image is not the same as the current 3D depth image! ({} != {})", (void*)image, (void*)s_curr3DDepthImage);
                return;
            }

            if (layer3D->HasCopiedDepth(side)) {
                // the depth texture has already been copied to the layer
                Log::print("[VULKAN] A depth texture is already bound for the current frame!");
                return;
            }

            // if (layer3D.GetStatus() == Status3D::LEFT_BINDING_DEPTH || layer3D.GetStatus() == Status3D::RIGHT_BINDING_DEPTH) {
            //     // seems to always be the case whenever closing the (inventory) menu
            //     Log::print("A depth texture is already bound for the current frame!");
            //     return;
            // }
            //
            // checkAssert(layer3D.GetStatus() == Status3D::LEFT_BINDING_COLOR || layer3D.GetStatus() == Status3D::RIGHT_BINDING_COLOR, "3D layer is not in the correct state for capturing depth images!");

            SharedTexture* texture = layer3D->CopyDepthToLayer(side, commandBuffer, image);
            s_activeCopyOperations.emplace_back(commandBuffer, texture);
            Log::print("[VULKAN] Queueing up a DEPTH signal inside cmd buffer {} for {} side", (void*)commandBuffer, side == OpenXR::EyeSide::LEFT ? "left" : "right");
            return;
        }
    }
    else {
        return pDispatch->CmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
    }
}

static std::unordered_set<VkSemaphore> s_isTimeline;

VkResult VkDeviceOverrides::CreateSemaphore(const vkroots::VkDeviceDispatch* pDispatch, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    VkResult res = pDispatch->CreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
    if (res == VK_SUCCESS && vkroots::FindInChain<VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, const VkSemaphoreCreateInfo>(pCreateInfo->pNext)) {
        s_isTimeline.emplace(*pSemaphore);
    }
    return res;
}

void VkDeviceOverrides::DestroySemaphore(const vkroots::VkDeviceDispatch* pDispatch, VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    s_isTimeline.erase(semaphore);
    return pDispatch->DestroySemaphore(device, semaphore, pAllocator);
}

inline bool IsTimeline(const VkSemaphore semaphore) {
    auto it = s_isTimeline.find(semaphore);
    return it != s_isTimeline.end();
}

VkResult VkDeviceOverrides::QueueSubmit(const vkroots::VkDeviceDispatch* pDispatch, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    if (s_activeCopyOperations.empty()) {
        return pDispatch->QueueSubmit(queue, submitCount, pSubmits, fence);
    }
    else {
        Log::print("[VULKAN] QueueSubmit called with {} active copy operations", s_activeCopyOperations.size());

        // insert (possible) pipeline barriers for any active copy operations
        struct ModifiedSubmitInfo_t {
            std::vector<VkSemaphore> waitSemaphores;
            std::vector<uint64_t> timelineWaitValues;
            std::vector<VkPipelineStageFlags> waitDstStageMasks;
            std::vector<VkSemaphore> signalSemaphores;
            std::vector<uint64_t> timelineSignalValues;

            VkTimelineSemaphoreSubmitInfo timelineSemaphoreSubmitInfo = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
        };

        std::vector<ModifiedSubmitInfo_t> modifiedSubmitInfos(submitCount);
        for (uint32_t i = 0; i < submitCount; i++) {
            const VkSubmitInfo& submitInfo = pSubmits[i];
            ModifiedSubmitInfo_t& modifiedSubmitInfo = modifiedSubmitInfos[i];

            // copy old semaphores into new vectors
            modifiedSubmitInfo.waitSemaphores.assign(submitInfo.pWaitSemaphores, submitInfo.pWaitSemaphores + submitInfo.waitSemaphoreCount);
            modifiedSubmitInfo.waitDstStageMasks.assign(submitInfo.pWaitDstStageMask, submitInfo.pWaitDstStageMask + submitInfo.waitSemaphoreCount);
            modifiedSubmitInfo.timelineWaitValues.resize(submitInfo.waitSemaphoreCount, 0);

            modifiedSubmitInfo.signalSemaphores.assign(submitInfo.pSignalSemaphores, submitInfo.pSignalSemaphores + submitInfo.signalSemaphoreCount);
            modifiedSubmitInfo.timelineSignalValues.resize(submitInfo.signalSemaphoreCount, 0);

            // find timeline semaphore submit info if already present
            const VkTimelineSemaphoreSubmitInfo* timelineSemaphoreSubmitInfoPtr = &modifiedSubmitInfo.timelineSemaphoreSubmitInfo;

            const VkBaseInStructure* pNextIt = static_cast<const VkBaseInStructure*>(submitInfo.pNext);
            while (pNextIt) {
                if (pNextIt->sType == VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO) {
                    timelineSemaphoreSubmitInfoPtr = reinterpret_cast<const VkTimelineSemaphoreSubmitInfo*>(pNextIt);
                    break;
                }
                pNextIt = pNextIt->pNext;
            }
            if (pNextIt == nullptr)
                const_cast<VkSubmitInfo&>(submitInfo).pNext = &modifiedSubmitInfo.timelineSemaphoreSubmitInfo;

            // copy any existing timeline values into new vectors
            for (uint32_t j = 0; j < timelineSemaphoreSubmitInfoPtr->waitSemaphoreValueCount; j++) {
                modifiedSubmitInfo.timelineWaitValues[j] = timelineSemaphoreSubmitInfoPtr->pWaitSemaphoreValues[j];
            }
            for (uint32_t j = 0; j < timelineSemaphoreSubmitInfoPtr->signalSemaphoreValueCount; j++) {
                modifiedSubmitInfo.timelineSignalValues[j] = timelineSemaphoreSubmitInfoPtr->pSignalSemaphoreValues[j];
            }

            // insert timeline semaphores for active copy operations
            bool foundCommandBufferForActiveCopyOperations = false;
            for (uint32_t j = 0; j < submitInfo.commandBufferCount; j++) {
                for (auto it = s_activeCopyOperations.begin(); it != s_activeCopyOperations.end();) {
                    if (submitInfo.pCommandBuffers[j] == it->first) {
                        // wait for D3D12/XR to finish with the previous shared texture render
                        modifiedSubmitInfo.waitSemaphores.emplace_back(it->second->GetSemaphore());
                        modifiedSubmitInfo.waitDstStageMasks.emplace_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                        modifiedSubmitInfo.timelineWaitValues.emplace_back(SEMAPHORE_TO_VULKAN);

#if defined(_DEBUG)
                        WCHAR nameChars[128] = {};
                        UINT size = sizeof(nameChars);
                        it->second->d3d12GetTexture()->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, nameChars);
                        std::string nameStr = wcharToUtf8(nameChars);

                        Log::print("[VULKAN] Waiting for {} to be 0 inside the cmd buffer {}", nameStr, (void*)submitInfo.pCommandBuffers[j]);
                        Log::print("[VULKAN] Signalling to {} to be 1 inside the cmd buffer {}", nameStr, (void*)submitInfo.pCommandBuffers[j]);
#endif

                        // signal to D3D12/XR rendering that the shared texture can be rendered to VR headset
                        modifiedSubmitInfo.signalSemaphores.emplace_back(it->second->GetSemaphore());
                        modifiedSubmitInfo.timelineSignalValues.emplace_back(SEMAPHORE_TO_D3D12);
                        it = s_activeCopyOperations.erase(it);
                        foundCommandBufferForActiveCopyOperations = true;
                    }
                    else {
                        ++it;
                    }
                }
            }

            if (!foundCommandBufferForActiveCopyOperations) {
                Log::print("[VULKAN] No command buffer found for active copy operations!");
                Log::print("[VULKAN] Active copy operations:");
                for (const auto& op : s_activeCopyOperations) {
                    wchar_t name[128] = {};
                    UINT size = sizeof(name);
                    op.second->d3d12GetTexture()->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
                    std::string nameStr = wcharToUtf8(name);
                    Log::print("[VULKAN]   - Command buffer: {}, Texture: {}", (void*)op.first, nameStr);
                }
                Log::print("[VULKAN] Submitted command buffers:");
                for (uint32_t j = 0; j < submitInfo.commandBufferCount; j++) {
                    Log::print("[VULKAN]   - Command buffer: {}", (void*)submitInfo.pCommandBuffers[j]);
                }
            }

            // update the VkTimelineSemaphoreSubmitInfo struct
            const_cast<VkTimelineSemaphoreSubmitInfo*>(timelineSemaphoreSubmitInfoPtr)->waitSemaphoreValueCount = (uint32_t)modifiedSubmitInfo.timelineWaitValues.size();
            const_cast<VkTimelineSemaphoreSubmitInfo*>(timelineSemaphoreSubmitInfoPtr)->pWaitSemaphoreValues = modifiedSubmitInfo.timelineWaitValues.data();
            const_cast<VkTimelineSemaphoreSubmitInfo*>(timelineSemaphoreSubmitInfoPtr)->signalSemaphoreValueCount = (uint32_t)modifiedSubmitInfo.timelineSignalValues.size();
            const_cast<VkTimelineSemaphoreSubmitInfo*>(timelineSemaphoreSubmitInfoPtr)->pSignalSemaphoreValues = modifiedSubmitInfo.timelineSignalValues.data();

            // add wait and signal semaphores to the submit info
            const_cast<VkSubmitInfo&>(submitInfo).waitSemaphoreCount = (uint32_t)modifiedSubmitInfo.waitSemaphores.size();
            const_cast<VkSubmitInfo&>(submitInfo).pWaitSemaphores = modifiedSubmitInfo.waitSemaphores.data();
            const_cast<VkSubmitInfo&>(submitInfo).pWaitDstStageMask = modifiedSubmitInfo.waitDstStageMasks.data();
            const_cast<VkSubmitInfo&>(submitInfo).signalSemaphoreCount = (uint32_t)modifiedSubmitInfo.signalSemaphores.size();
            const_cast<VkSubmitInfo&>(submitInfo).pSignalSemaphores = modifiedSubmitInfo.signalSemaphores.data();
        }
        return pDispatch->QueueSubmit(queue, submitCount, pSubmits, fence);
    }
}

VkResult VkDeviceOverrides::QueuePresentKHR(const vkroots::VkDeviceDispatch* pDispatch, VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    VRManager::instance().XR->ProcessEvents();

    return pDispatch->QueuePresentKHR(queue, pPresentInfo);
}