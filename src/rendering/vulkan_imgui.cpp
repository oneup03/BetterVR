#include "instance.h"
#include "vulkan.h"
#include "hooking/entity_debugger.h"
#include "utils/vulkan_utils.h"

RND_Renderer::ImGuiOverlay::ImGuiOverlay(VkCommandBuffer cb, VkExtent2D fbRes, VkFormat fbFormat): m_outputRes(fbRes) {
    ImGui::CreateContext();
    ImPlot3D::CreateContext();
    ImPlot::CreateContext();

    VkQueue queue = nullptr;
    VRManager::instance().VK->GetDeviceDispatch()->GetDeviceQueue(VRManager::instance().VK->GetDevice(), 0, 0, &queue);

    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = std::size(poolSizes),
        .pPoolSizes = poolSizes
    };
    checkVkResult(VRManager::instance().VK->GetDeviceDispatch()->CreateDescriptorPool(VRManager::instance().VK->GetDevice(), &poolInfo, nullptr, &m_descriptorPool), "Failed to create descriptor pool for ImGui");

    // create render pass to render imgui to a texture which we'll copy to the swapchain
    VkAttachmentDescription colorAttachment = {
        .format = fbFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        .dependencyFlags = 0
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };
    checkVkResult(VRManager::instance().VK->GetDeviceDispatch()->CreateRenderPass(VRManager::instance().VK->GetDevice(), &renderPassInfo, nullptr, &m_renderPass), "Failed to create render pass for ImGui");

    // // initialize imgui for vulkan
    ImGui::GetIO().DisplaySize = ImVec2((float)fbRes.width, (float)fbRes.height);

    // load vulkan functions
    checkAssert(ImGui_ImplVulkan_LoadFunctions(VRManager::instance().vkVersion, [](const char* funcName, void* data_queue) {
        VkInstance instance = VRManager::instance().VK->GetInstance();
        VkDevice device = VRManager::instance().VK->GetDevice();
        PFN_vkVoidFunction addr = VRManager::instance().VK->GetDeviceDispatch()->GetDeviceProcAddr(device, funcName);

        if (addr == nullptr) {
            addr = VRManager::instance().VK->GetInstanceDispatch()->GetInstanceProcAddr(instance, funcName);
            Log::print<VERBOSE>("Loaded function {} at {} using instance", funcName, (void*)addr);
        }
        else {
            Log::print<VERBOSE>("Loaded function {} at {}", funcName, (void*)addr);
        }

        return addr;
    }), "Failed to load vulkan functions for ImGui");

    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = VRManager::instance().VK->GetInstance(),
        .PhysicalDevice = VRManager::instance().VK->GetPhysicalDevice(),
        .Device = VRManager::instance().VK->GetDevice(),
        .QueueFamily = 0,
        .Queue = queue,
        .DescriptorPool = m_descriptorPool,
        .RenderPass = m_renderPass,
        .MinImageCount = 6,
        .ImageCount = 6,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,

        .UseDynamicRendering = false,

        .Allocator = nullptr,
        .CheckVkResultFn = nullptr,
        .MinAllocationSize = 1024 * 1024
    };
    checkAssert(ImGui_ImplVulkan_Init(&init_info), "Failed to initialize ImGui");

    auto* renderer = VRManager::instance().XR->GetRenderer();
    for (int i = 0; i < 2; ++i) {
        renderer->GetFrame(i).imguiFramebuffer = std::make_unique<VulkanFramebuffer>(fbRes.width, fbRes.height, fbFormat, m_renderPass);
    }

    Log::print<VERBOSE>("Initializing font textures for ImGui...");
    ImGui_ImplVulkan_CreateFontsTexture();

    // create sampler
    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = -1000.0f;
    samplerInfo.maxLod = 1000.0f;
    checkVkResult(VRManager::instance().VK->GetDeviceDispatch()->CreateSampler(VRManager::instance().VK->GetDevice(), &samplerInfo, nullptr, &m_sampler), "Failed to create sampler for ImGui");

    for (int i = 0; i < 2; ++i) {
        auto& frame = renderer->GetFrame(i);

        frame.mainFramebuffer = std::make_unique<VulkanTexture>(fbRes.width, fbRes.height, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
        frame.mainFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
        frame.mainFramebuffer->vkClear(cb, { 0.0f, 0.0f, 0.0f, 0.0f });
        frame.mainFramebufferDS = ImGui_ImplVulkan_AddTexture(m_sampler, frame.mainFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);

        frame.hudFramebuffer = std::make_unique<VulkanTexture>(fbRes.width, fbRes.height, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
        frame.hudFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
        frame.hudFramebuffer->vkClear(cb, { 0.0f, 0.0f, 0.0f, 0.0f });
        frame.hudFramebufferDS = ImGui_ImplVulkan_AddTexture(m_sampler, frame.hudFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);

        frame.hudWithoutAlphaFramebuffer = std::make_unique<VulkanTexture>(fbRes.width, fbRes.height, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, true);
        frame.hudWithoutAlphaFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
        frame.hudWithoutAlphaFramebuffer->vkClear(cb, { 0.0f, 0.0f, 0.0f, 0.0f });
        frame.hudWithoutAlphaFramebufferDS = ImGui_ImplVulkan_AddTexture(m_sampler, frame.hudWithoutAlphaFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
    }
    
    VulkanUtils::DebugPipelineBarrier(cb);

    // start the first frame right away
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
}

RND_Renderer::ImGuiOverlay::~ImGuiOverlay() {
    auto* renderer = VRManager::instance().XR->GetRenderer();
    for (int i = 0; i < 2; ++i) {
        auto& frame = renderer->GetFrame(i);
        if (frame.mainFramebufferDS != VK_NULL_HANDLE)
            ImGui_ImplVulkan_RemoveTexture(frame.mainFramebufferDS);
        if (frame.mainFramebuffer != nullptr)
            frame.mainFramebuffer.reset();
        if (frame.hudFramebufferDS != VK_NULL_HANDLE)
            ImGui_ImplVulkan_RemoveTexture(frame.hudFramebufferDS);
        if (frame.hudFramebuffer != nullptr)
            frame.hudFramebuffer.reset();
        if (frame.hudWithoutAlphaFramebufferDS != VK_NULL_HANDLE)
            ImGui_ImplVulkan_RemoveTexture(frame.hudWithoutAlphaFramebufferDS);
        if (frame.hudWithoutAlphaFramebuffer != nullptr)
            frame.hudWithoutAlphaFramebuffer.reset();
        if (frame.imguiFramebuffer != nullptr)
            frame.imguiFramebuffer.reset();
    }

    if (m_sampler != VK_NULL_HANDLE)
        VRManager::instance().VK->GetDeviceDispatch()->DestroySampler(VRManager::instance().VK->GetDevice(), m_sampler, nullptr);
    if (m_renderPass != VK_NULL_HANDLE)
        VRManager::instance().VK->GetDeviceDispatch()->DestroyRenderPass(VRManager::instance().VK->GetDevice(), m_renderPass, nullptr);
    if (m_descriptorPool != VK_NULL_HANDLE)
        VRManager::instance().VK->GetDeviceDispatch()->DestroyDescriptorPool(VRManager::instance().VK->GetDevice(), m_descriptorPool, nullptr);

    ImGui_ImplVulkan_Shutdown();
    ImPlot::DestroyContext();
    ImPlot3D::DestroyContext();
    ImGui::DestroyContext();
}

void RND_Renderer::ImGuiOverlay::Update() {
    ImGui::GetIO().FontGlobalScale = 0.9f;

    POINT p;
    GetCursorPos(&p);
    ScreenToClient(CemuHooks::m_cemuRenderWindow, &p);

    // calculate how many client side pixels are used on the border since its not a 16:9 aspect ratio
    RECT rect;
    GetClientRect(CemuHooks::m_cemuRenderWindow, &rect);
    uint32_t nonCenteredWindowWidth = rect.right - rect.left;
    uint32_t nonCenteredWindowHeight = rect.bottom - rect.top;

    // calculate window size without black bars due to a non-16:9 aspect ratio
    uint32_t renderPhysicalWidth = nonCenteredWindowWidth;
    uint32_t renderPhysicalHeight = nonCenteredWindowHeight;
    if (nonCenteredWindowWidth * 9 > nonCenteredWindowHeight * 16) {
        renderPhysicalWidth = nonCenteredWindowHeight * 16 / 9;
    }
    else {
        renderPhysicalHeight = nonCenteredWindowWidth * 9 / 16;
    }
    ImVec2 physicalRes = ImVec2(renderPhysicalWidth, renderPhysicalHeight);
    ImVec2 framebufferRes = ImVec2((float)m_outputRes.width, (float)m_outputRes.height);

    ImGui::GetIO().DisplaySize = physicalRes;

    // calculate the black bars
    uint32_t blackBarWidth = (nonCenteredWindowWidth - renderPhysicalWidth) / 2;
    uint32_t blackBarHeight = (nonCenteredWindowHeight - renderPhysicalHeight) / 2;

    // adjust the framebuffer scale
    ImGui::GetIO().DisplayFramebufferScale = framebufferRes / physicalRes;

    // the actual window is centered, so add offsets to both x and y on both sides
    p.x = p.x - blackBarWidth;
    p.y = p.y - blackBarHeight;

    // update mouse controls and keyboard input
    bool isWindowFocused = CemuHooks::m_cemuTopWindow == GetForegroundWindow();

    ImGui::GetIO().AddFocusEvent(isWindowFocused);
    if (isWindowFocused) {
        // update mouse state depending on if the window is focused
        ImGui::GetIO().AddMousePosEvent((float)p.x, (float)p.y);

        ImGui::GetIO().AddMouseButtonEvent(0, GetAsyncKeyState(VK_LBUTTON) & 0x8000);
        ImGui::GetIO().AddMouseButtonEvent(1, GetAsyncKeyState(VK_RBUTTON) & 0x8000);
        ImGui::GetIO().AddMouseButtonEvent(2, GetAsyncKeyState(VK_MBUTTON) & 0x8000);

        // toggle between no overlay, 2D-only FPS overlay and fps overlay both for 2D and VR
        bool isF3Pressed = GetKeyState(VK_F3) & 0x8000;
        if (isF3Pressed && !m_wasF3Pressed) {
            m_showAppMS++;
            if (m_showAppMS > 2) {
                m_showAppMS = 0;
            }
        }
        m_wasF3Pressed = isF3Pressed;
    }

    if (VRManager::instance().Hooks->m_entityDebugger && isWindowFocused) {
        VRManager::instance().Hooks->m_entityDebugger->UpdateKeyboardControls();
    }
}

constexpr ImGuiWindowFlags FULLSCREEN_WINDOW_FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

void RND_Renderer::ImGuiOverlay::Render(long frameIdx, bool renderBackground) {
    auto* renderer = VRManager::instance().XR->GetRenderer();
    auto& frame = renderer->GetFrame(frameIdx);

    // calculate width minus the retina scaling
    ImVec2 windowSize = ImGui::GetIO().DisplaySize;
    //ImVec2 renderScale = ImGui::GetIO().DisplayFramebufferScale;
    //ImVec2 imageScale = windowSize * renderScale;
    //ImVec2 viewportWorkSize = ImGui::GetMainViewport()->WorkSize;
    //ImVec2 framebufferSize = ImVec2(m_outputRes.width, m_outputRes.height);
    //Log::print<INFO>("Window Size = {}x{} - Work Size = {}x{}", windowSize.x, windowSize.y, viewportWorkSize.x, viewportWorkSize.y);

    auto renderHUDBackground = [&](bool withAlpha) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(windowSize);

        ImGui::Begin("HUD Background", nullptr, FULLSCREEN_WINDOW_FLAGS);
        ImGui::Image((ImTextureID)(withAlpha ? frame.hudFramebufferDS : frame.hudWithoutAlphaFramebufferDS), windowSize);
        ImGui::End();

        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
    };

    if (renderBackground || CemuHooks::UseBlackBarsDuringEvents()) {
        const bool shouldCrop3DTo16_9 = CemuHooks::GetSettings().cropFlatTo16x9Setting == 1;

        bool shouldRender3DBackground = VRManager::instance().XR->GetRenderer()->IsRendering3D(frameIdx) || CemuHooks::UseBlackBarsDuringEvents();
        bool shouldRenderHUDWithAlpha = shouldRender3DBackground && !CemuHooks::UseBlackBarsDuringEvents();

        renderHUDBackground(shouldRenderHUDWithAlpha);

        if (shouldRender3DBackground) {
            // center position using aspect ratio
            ImVec2 centerPos = ImVec2((windowSize.x - windowSize.y * frame.mainFramebufferAspectRatio) / 2, 0);
            ImVec2 squishedWindowSize = ImVec2(windowSize.y * frame.mainFramebufferAspectRatio, windowSize.y);

            // calculate UVs for cropping to 16:9
            ImVec2 croppedUv0 = ImVec2(0.0f, 0.0f);
            ImVec2 croppedUv1 = ImVec2(1.0f, 1.0f);
            if (shouldCrop3DTo16_9) {
                ImVec2 displaySize = ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y / 2 / frame.mainFramebufferAspectRatio);
                ImVec2 displayOffset = ImVec2(ImGui::GetIO().DisplaySize.x / 2 - (displaySize.x / 2), ImGui::GetIO().DisplaySize.y / 2 - (displaySize.y / 2));
                ImVec2 textureSize = ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

                croppedUv0 = ImVec2(displayOffset.x / textureSize.x, displayOffset.y / textureSize.y);
                croppedUv1 = ImVec2((displayOffset.x + displaySize.x) / textureSize.x, (displayOffset.y + displaySize.y) / textureSize.y);
            }

            // render 3D background
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::SetNextWindowPos(shouldCrop3DTo16_9 ? ImVec2(0, 0) : centerPos);
            ImGui::SetNextWindowSize(windowSize);
            
            ImGui::Begin("3D Background", nullptr, FULLSCREEN_WINDOW_FLAGS);
            ImGui::Image((ImTextureID)frame.mainFramebufferDS, shouldCrop3DTo16_9 ? windowSize : squishedWindowSize, croppedUv0, croppedUv1);
            ImGui::End();

            ImGui::PopStyleVar();
            ImGui::PopStyleVar();
        }
    }
    else {
        renderHUDBackground(VRManager::instance().XR->GetRenderer()->IsRendering3D(frameIdx));
    }

    if (VRManager::instance().Hooks->m_entityDebugger) {
        VRManager::instance().Hooks->m_entityDebugger->DrawEntityInspector();
        VRManager::instance().Hooks->DrawDebugOverlays();
    }

    if ((renderBackground && m_showAppMS == 1) || (m_showAppMS == 2)) {
        EntityDebugger::DrawFPSOverlay(renderer);
    }
}

void RND_Renderer::ImGuiOverlay::Draw3DLayerAsBackground(VkCommandBuffer cb, VkImage srcImage, float aspectRatio, long frameIdx) {
    auto& frame = VRManager::instance().XR->GetRenderer()->GetFrame(frameIdx);

    frame.mainFramebuffer->vkCopyFromImage(cb, srcImage);
    frame.mainFramebufferAspectRatio = aspectRatio;
}

void RND_Renderer::ImGuiOverlay::DrawHUDLayerAsBackground(VkCommandBuffer cb, VkImage srcImage, long frameIdx) {
    auto& frame = VRManager::instance().XR->GetRenderer()->GetFrame(frameIdx);

    frame.hudFramebuffer->vkCopyFromImage(cb, srcImage);
    frame.hudWithoutAlphaFramebuffer->vkCopyFromImage(cb, srcImage);

}

void RND_Renderer::ImGuiOverlay::DrawAndCopyToImage(VkCommandBuffer cb, VkImage destImage, long frameIdx) {
    ImGui::Render();

    auto* dispatch = VRManager::instance().VK->GetDeviceDispatch();
    auto* renderer = VRManager::instance().XR->GetRenderer();
    auto& frame = renderer->GetFrame(frameIdx);

    frame.imguiFramebuffer->vkClear(cb, { 0.0f, 0.0f, 0.0f, 0.0f });

    // draw imgui to framebuffer
    VkClearValue clearValue = { .color = { 0.0f, 0.0f, 0.0f, 0.0f } };
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_renderPass,
        .framebuffer = frame.imguiFramebuffer->GetFramebuffer(),
        .renderArea = {
            .offset = { 0, 0 },
            .extent = m_outputRes,
        },
        .clearValueCount = 0,
        .pClearValues = &clearValue
    };
    dispatch->CmdBeginRenderPass(cb, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);

    dispatch->CmdEndRenderPass(cb);

    // copy rendered imgui to destination image
    frame.imguiFramebuffer->vkPipelineBarrier(cb);
    frame.imguiFramebuffer->vkCopyToImage(cb, destImage);
    frame.imguiFramebuffer->vkPipelineBarrier(cb);

    // prepare for next frame immediately
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
}