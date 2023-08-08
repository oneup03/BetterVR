#include "framebuffer.h"
#include "instance.h"
#include "layer.h"
#include "utils/d3d12_utils.h"


std::mutex lockImageResolutions;
std::unordered_map<VkImage, std::pair<VkExtent2D, VkFormat>> imageResolutions;

std::vector<std::pair<VkCommandBuffer, SharedTexture*>> s_activeCopyOperations;

VkImage s_curr3DColorImage = VK_NULL_HANDLE;
VkImage s_curr3DDepthImage = VK_NULL_HANDLE;

using namespace VRLayer;
using Status3D = RND_Renderer::Layer3D::Status3D;
using Status2D = RND_Renderer::Layer2D::Status2D;


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
        checkAssert(captureIdx == 0 || captureIdx == 2, "Invalid capture index!");

        auto& layer3D = VRManager::instance().XR->GetRenderer()->m_layer3D;
        auto& layer2D = VRManager::instance().XR->GetRenderer()->m_layer2D;

        // initialize the textures of both 2D and 3D layer if either is found since they share the same VkImage and resolution
        if (captureIdx == 0 || captureIdx == 2) {
            if (layer2D.GetStatus() == Status2D::UNINITIALIZED && layer3D.GetStatus() == Status3D::UNINITIALIZED) {
                lockImageResolutions.lock();
                auto it = imageResolutions.find(image);
                if (it != imageResolutions.end()) {
                    layer3D.InitTextures(it->second.first);
                    layer2D.InitTextures(it->second.first);
                    Log::print("Found rendering resolution {}x{} @ {} using capture #{}", it->second.first.width, it->second.first.height, it->second.second, captureIdx);
                    VRManager::instance().XR->GetRenderer()->StartFrame();
                }
                else {
                    checkAssert(false, "Couldn't find image resolution in map!");
                }
                lockImageResolutions.unlock();
            }
        }

        if (captureIdx == 0) {
            // 3D layer - color texture for 3D rendering

            // check if the color texture has the appropriate texture format
            if (s_curr3DColorImage == VK_NULL_HANDLE) {
                lockImageResolutions.lock();
                if (auto it = imageResolutions.find(image); it != imageResolutions.end()) {
                    if (it->second.second == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
                        s_curr3DColorImage = it->first;
                    }
                }
                lockImageResolutions.unlock();
            }

            if (image != s_curr3DColorImage) {
                const_cast<VkClearColorValue*>(pColor)[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
                return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
            }

            if (layer3D.GetStatus() == Status3D::LEFT_BINDING_COLOR || layer3D.GetStatus() == Status3D::RIGHT_BINDING_COLOR) {
                const_cast<VkClearColorValue*>(pColor)[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
                return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
            }

            if (layer3D.GetStatus() == Status3D::LEFT_BINDING_DEPTH || layer3D.GetStatus() == Status3D::RIGHT_BINDING_DEPTH) {
                // seems to always be the case whenever closing the (inventory) menu
                Log::print("A color texture is already bound for the current frame!");
                return;
            }

            SharedTexture* texture = layer3D.CopyColorToLayer(commandBuffer, image);
            s_activeCopyOperations.emplace_back(commandBuffer, texture);

            // clear the image to be transparent to allow for the HUD to be rendered on top of it which results in a transparent HUD layer
            const_cast<VkClearColorValue*>(pColor)[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
            return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
        }
        else if (captureIdx == 2) {
            // 2D layer - color texture for HUD rendering

            // skip second attempt at capturing when GX2ClearColor is called for this image since it'll try to clear the 2D layer twice
            // note: this will ALSO skip each second frame the HUD gets rendered while waiting for the right-side of the 3D layer to render (since we can't ONLY update the 2D layer while presenting the old HUD)
            if (layer2D.GetStatus() == RND_Renderer::Layer2D::Status2D::BINDING_COLOR) {
                // clear it to black for a better visualization since Cemu's output is only the HUD at this point
                const_cast<VkClearColorValue*>(pColor)[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
                return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
            }

            SharedTexture* texture = layer2D.CopyColorToLayer(commandBuffer, image);
            s_activeCopyOperations.emplace_back(commandBuffer, texture);

            const_cast<VkClearColorValue*>(pColor)[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
            return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
        }
        return;
    }
    else {
        return pDispatch->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
    }
}

void VRLayer::VkDeviceOverrides::CmdClearDepthStencilImage(const vkroots::VkDeviceDispatch* pDispatch, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    // check for magical clear values
    if (rangeCount == 1 && pDepthStencil->depth >= 0.011456789 && pDepthStencil->depth <= 0.013456789) {
        // stencil value is the capture idx
        const uint32_t captureIdx = pDepthStencil->stencil;
        checkAssert(captureIdx == 1, "Invalid capture index!");

        auto& layer3D = VRManager::instance().XR->GetRenderer()->m_layer3D;
        auto& layer2D = VRManager::instance().XR->GetRenderer()->m_layer2D;

        if (captureIdx == 1) {
            // 3D layer - depth texture for 3D rendering

            if (s_curr3DDepthImage == VK_NULL_HANDLE) {
                lockImageResolutions.lock();
                if (auto it = imageResolutions.find(image); it != imageResolutions.end()) {
                    if (it->second.second == VK_FORMAT_D32_SFLOAT) {
                        s_curr3DDepthImage = it->first;
                    }
                }
                lockImageResolutions.unlock();
            }

            if (image != s_curr3DDepthImage) {
                return;
            }

            if (layer3D.GetStatus() == Status3D::LEFT_BINDING_DEPTH || layer3D.GetStatus() == Status3D::RIGHT_BINDING_DEPTH) {
                // seems to always be the case whenever closing the (inventory) menu
                Log::print("A depth texture is already bound for the current frame!");
                return;
            }

            checkAssert(layer3D.GetStatus() == Status3D::LEFT_BINDING_COLOR || layer3D.GetStatus() == Status3D::RIGHT_BINDING_COLOR, "3D layer is not in the correct state for capturing depth images!");

            SharedTexture* texture = layer3D.CopyDepthToLayer(commandBuffer, image);
            s_activeCopyOperations.emplace_back(commandBuffer, texture);
            return;
        }
    }
    else {
        return pDispatch->CmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
    }
}

VkResult VkDeviceOverrides::EndCommandBuffer(const vkroots::VkDeviceDispatch* pDispatch, VkCommandBuffer commandBuffer) {
    return pDispatch->EndCommandBuffer(commandBuffer);
}

VkResult VkDeviceOverrides::QueueSubmit(const vkroots::VkDeviceDispatch* pDispatch, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    if (s_activeCopyOperations.empty()) {
        return pDispatch->QueueSubmit(queue, submitCount, pSubmits, fence);
    }
    else {
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
            for (uint32_t j = 0; j < submitInfo.waitSemaphoreCount; j++) {
                modifiedSubmitInfo.waitSemaphores.emplace_back(submitInfo.pWaitSemaphores[j]);
                modifiedSubmitInfo.waitDstStageMasks.emplace_back(submitInfo.pWaitDstStageMask[j]);
                modifiedSubmitInfo.timelineWaitValues.emplace_back(0);
            }

            for (uint32_t j = 0; j < submitInfo.signalSemaphoreCount; j++) {
                modifiedSubmitInfo.signalSemaphores.emplace_back(submitInfo.pSignalSemaphores[j]);
                modifiedSubmitInfo.timelineSignalValues.emplace_back(0);
            }

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
                modifiedSubmitInfo.timelineWaitValues.emplace_back(timelineSemaphoreSubmitInfoPtr->pWaitSemaphoreValues[j]);
            }
            for (uint32_t j = 0; j < timelineSemaphoreSubmitInfoPtr->signalSemaphoreValueCount; j++) {
                modifiedSubmitInfo.timelineSignalValues.emplace_back(timelineSemaphoreSubmitInfoPtr->pSignalSemaphoreValues[j]);
            }

            // insert timeline semaphores for active copy operations
            for (uint32_t j = 0; j < submitInfo.commandBufferCount; j++) {
                for (auto it = s_activeCopyOperations.begin(); it != s_activeCopyOperations.end();) {
                    if (submitInfo.pCommandBuffers[j] == it->first) {
                        // wait for D3D12/XR to finish with previous shared texture render
                        modifiedSubmitInfo.waitSemaphores.emplace_back(it->second->GetSemaphore());
                        modifiedSubmitInfo.waitDstStageMasks.emplace_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                        modifiedSubmitInfo.timelineWaitValues.emplace_back(0);

                        // signal to D3D12/XR rendering that the shared texture can be rendered to VR headset
                        modifiedSubmitInfo.signalSemaphores.emplace_back(it->second->GetSemaphore());
                        modifiedSubmitInfo.timelineSignalValues.emplace_back(1);
                        it = s_activeCopyOperations.erase(it);
                    }
                    else {
                        ++it;
                    }
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

extern OpenXR::EyeSide s_currentEye;

VkResult VkDeviceOverrides::QueuePresentKHR(const vkroots::VkDeviceDispatch* pDispatch, VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    VRManager::instance().XR->ProcessEvents();

    RND_Renderer* renderer = VRManager::instance().XR->GetRenderer();
    if (renderer && renderer->m_layer3D.GetStatus() != Status3D::UNINITIALIZED) {
        if (renderer->m_layer3D.GetStatus() == Status3D::LEFT_BINDING_DEPTH) {
            // Log::print("Preparing for 3D rendering - right eye");
            renderer->m_layer3D.PrepareRendering(OpenXR::EyeSide::RIGHT);
            s_currentEye = OpenXR::EyeSide::LEFT;
        }
        else {
            renderer->EndFrame();
            renderer->StartFrame();
            s_currentEye = OpenXR::EyeSide::RIGHT;
        }
    }
    return pDispatch->QueuePresentKHR(queue, pPresentInfo);
}