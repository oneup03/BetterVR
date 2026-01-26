#include "instance.h"
#include "vulkan.h"
#include "utils/controller_image.h"
#include "hooking/entity_debugger.h"
#include "hooking/cemu_hooks.h"
#include "utils/vulkan_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


RND_Renderer::ImGuiOverlay::ImGuiOverlay(VkCommandBuffer cb, VkExtent2D fbRes, VkFormat fbFormat): m_outputRes(fbRes) {
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_HasGamepad;
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

    // create VulkanTexture
    auto createHelpImage = [&](stbi_uc const* imageData, int imageSize) {
        // load png image using stb_image
        int width, height, channels;
        unsigned char* img = stbi_load_from_memory(imageData, imageSize, &width, &height, &channels, STBI_rgb_alpha);

        // create VulkanTexture from image data
        HelpImage image;
        image.m_image = new VulkanTexture{ (uint32_t)width, (uint32_t)height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT };
        image.m_image->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
        image.m_image->vkUpload(cb, img, width * height * 4);
        image.m_image->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);

        stbi_image_free(img);

        image.m_imageDS = ImGui_ImplVulkan_AddTexture(m_sampler, image.m_image->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
        return image;
    };

    std::vector<HelpImage> page1;
    page1.emplace_back(createHelpImage(BOTWcontrolScheme1_inputs, sizeof(BOTWcontrolScheme1_inputs)));
    page1.emplace_back(createHelpImage(BOTWcontrolScheme_BodySlots, sizeof(BOTWcontrolScheme_BodySlots)));
    m_helpImagePages.push_back(page1);

    std::vector<HelpImage> page2;
    page2.push_back(createHelpImage(BOTWcontrolScheme_Attack, sizeof(BOTWcontrolScheme_Attack)));
    page2.push_back(createHelpImage(BOTWcontrolScheme_Shield, sizeof(BOTWcontrolScheme_Shield)));
    m_helpImagePages.push_back(page2);

    std::vector<HelpImage> page3;
    page3.push_back(createHelpImage(BOTWcontrolScheme_Magnesis, sizeof(BOTWcontrolScheme_Magnesis)));
    page3.push_back(createHelpImage(BOTWcontrolScheme_Whistle, sizeof(BOTWcontrolScheme_Whistle)));
    m_helpImagePages.push_back(page3);

    
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

    for (auto& imagePage : m_helpImagePages) {
        for (auto& helpImage : imagePage) {
            ImGui_ImplVulkan_RemoveTexture(helpImage.m_imageDS);
            delete helpImage.m_image;
        }
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
    bool isWindowFocused = CemuHooks::m_cemuTopWindow == GetForegroundWindow() || true;

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

    DrawHelpMenu();
}

void RND_Renderer::ImGuiOverlay::ProcessInputs(OpenXR::InputState& inputs, OpenXR::HelpMenuState& menuState) {
    if (!menuState.isOpen) return;

    auto& io = ImGui::GetIO();
    bool inGame = inputs.inGame.in_game;

    // Use in-game move stick or menu navigate stick
    auto& stick = inGame ? inputs.inGame.move : inputs.inMenu.navigate;

    // Map Stick to DPad with hysteresis and repeating pulse
    constexpr float THRESHOLD_PRESS = 0.5f;
    constexpr float THRESHOLD_RELEASE = 0.3f; 

    static bool dpadState[4] = { false }; // Up, Down, Left, Right
    static double lastRefireTime[6] = { 0.0 }; // 0-3 Dpad, 4 B, 5 A
    constexpr double REFIRE_DELAY = 0.4f; // Seconds between repeats
    double currentTime = ImGui::GetTime();

    auto updateDpadState = [&](int idx, float val, bool positive) {
        bool isPressed = dpadState[idx];
        if (positive) {
            if (!isPressed && val >= THRESHOLD_PRESS) isPressed = true;
            else if (isPressed && val <= THRESHOLD_RELEASE) isPressed = false;
        } else {
            if (!isPressed && val <= -THRESHOLD_PRESS) isPressed = true;
            else if (isPressed && val >= -THRESHOLD_RELEASE) isPressed = false;
        }
        dpadState[idx] = isPressed;
        return isPressed;
    };

    auto applyInput = [&](ImGuiKey key, bool isPressed, int idx) {
        if (isPressed) {
            if (currentTime - lastRefireTime[idx] >= REFIRE_DELAY || lastRefireTime[idx] == 0.0) {
                 io.AddKeyEvent(key, true);
                 lastRefireTime[idx] = currentTime;
            } else {
                 io.AddKeyEvent(key, false);
            }
        } else {
            io.AddKeyEvent(key, false);
            lastRefireTime[idx] = 0.0;
        }
    };

    applyInput(ImGuiKey_GamepadDpadUp, updateDpadState(0, stick.currentState.y, true), 0);
    applyInput(ImGuiKey_GamepadDpadDown, updateDpadState(1, stick.currentState.y, false), 1);
    applyInput(ImGuiKey_GamepadDpadLeft, updateDpadState(2, stick.currentState.x, false), 2);
    applyInput(ImGuiKey_GamepadDpadRight, updateDpadState(3, stick.currentState.x, true), 3);

    // Map Buttons
    bool backDown = false;
    bool confirmDown = false;

    if (inGame) {
        backDown = inputs.inGame.run_interact_cancel.currentState;
        // Map jump (X) to confirm/enter for now since A is not clearly mapped in the InGame struct active here
        confirmDown = inputs.inGame.jump.currentState;
    }
    else {
        backDown = inputs.inMenu.back.currentState;
        confirmDown = inputs.inMenu.select.currentState;
    }

    applyInput(ImGuiKey_GamepadFaceRight, backDown, 4); // B -> Cancel
    applyInput(ImGuiKey_GamepadFaceDown, confirmDown, 5); // A -> Confirm

    // Handle Exit (B press)
    bool shouldClose = false;
    if (inGame) {
        if (inputs.inGame.run_interact_cancel.changedSinceLastSync && inputs.inGame.run_interact_cancel.currentState) {
            shouldClose = true;
            inputs.inGame.run_interact_cancel.currentState = XR_FALSE; // consume
        }
    }
    else {
        if (inputs.inMenu.back.changedSinceLastSync && inputs.inMenu.back.currentState) {
            shouldClose = true;
        }
    }

    if (shouldClose) {
        menuState.isOpen = false;
        VRManager::instance().XR->m_helpMenuState.store(menuState);
    }
}

void RND_Renderer::ImGuiOverlay::DrawHelpMenu() {
    auto menuState = VRManager::instance().XR->m_helpMenuState.load();

    if (ImGui::GetTime() < 10.0f && !menuState.isOpen) {
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
        if (ImGui::Begin("HelpNotify", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs)) {
            ImGui::TextColored(ImVec4(1.0, 1.0, 1.0, 1.0), "Long-Press The Inventory Button (Right Thumbstick By Default) To Open BetterVR Help & Settings");
        }
        ImGui::End();
    }

    if (!menuState.isOpen) return;

    ImVec2 fullWindowWidth = ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
    ImVec2 windowWidth = fullWindowWidth * ImVec2(0.8f, 1.0f);

    ImGui::SetNextWindowPos(fullWindowWidth * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(windowWidth, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowFocus();

    if (ImGui::Begin("BetterVR Settings & Help | Press B Or Long-Press Inventory Button To Exit", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {

        if (ImGui::BeginTabBar("HelpMenuTabs")) {
            auto tabFlags = [](int idx, int current) {
                return (idx == current) ? ImGuiTabItemFlags_SetSelected : 0;
            };

            if (ImGui::BeginTabItem("Control Guide", nullptr, 0)) {
                menuState.currPageIdx = 0;
                VRManager::instance().XR->m_helpMenuState.store(menuState);

                ImGui::PushItemWidth(windowWidth.x * 0.5f);

                for (const auto& imagePage : m_helpImagePages) {
                    for (const auto& image : imagePage) {
                        ImGui::PushID(&image);
                        float imageHeightAtFullWidth = (windowWidth.x) * ((float)image.m_image->GetHeight() / (float)image.m_image->GetWidth());
                        ImVec2 imageSize = ImVec2(windowWidth.x, imageHeightAtFullWidth);
                        ImVec2 p = ImGui::GetCursorPos();
                        ImGui::Selectable("##imgButton", false, 0, imageSize);
                        ImGui::SetCursorPos(p);
                        ImGui::Image((ImTextureID)image.m_imageDS, imageSize);
                        ImGui::PopID();
                    }
                }

                ImGui::PopItemWidth();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Settings", nullptr, 0)) {
                menuState.currPageIdx = 1;
                VRManager::instance().XR->m_helpMenuState.store(menuState);

                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                ImGui::Indent(10.0f);
                ImGui::PushItemWidth(windowWidth.x * 0.5f);

                auto settings = CemuHooks::GetSettings();
                bool changed = false;

                ImGui::Separator();
                ImGui::Text("Camera Mode");
                int cameraMode = settings.IsFirstPersonMode() ? 1 : 0;
                if (ImGui::RadioButton("First Person", &cameraMode, 1)) { settings.cameraModeSetting = 1; changed = true; }
                ImGui::SameLine();
                if (ImGui::RadioButton("Third Person", &cameraMode, 0)) { settings.cameraModeSetting = 0; changed = true; }

                if (cameraMode == 0) {
                    float distance = settings.thirdPlayerDistance.getLE();
                    if (ImGui::SliderFloat("Camera Distance", &distance, 0.4f, 1.1f, "%.2f m")) {
                        settings.thirdPlayerDistance = distance;
                        changed = true;
                    }
                }
                else {
                    
                }

                ImGui::Separator();
                ImGui::Text("Comfort & Accessibility");
                if (cameraMode == 1) {
                    bool leftHanded = settings.IsLeftHanded();
                    if (ImGui::Checkbox("Left Handed Mode", &leftHanded)) {
                        settings.leftHandedSetting = leftHanded ? 1 : 0;
                        changed = true;
                    }

                    bool guiFollow = settings.UIFollowsLookingDirection();
                    if (ImGui::Checkbox("UI Follows View", &guiFollow)) {
                        settings.guiFollowSetting = guiFollow ? 1 : 0;
                        changed = true;
                    }

                    ImGui::Separator();
                    ImGui::Text("Player Height Offset");
                    float height = settings.playerHeightSetting.getLE();
                    if (ImGui::SliderFloat("Height (meters)", &height, -0.5f, 1.0f, "%.2f")) {
                        settings.playerHeightSetting = height;
                        changed = true;
                    }
                    if (ImGui::Button("Reset Height")) {
                        settings.playerHeightSetting = 0.0f;
                        changed = true;
                    }
                }
                else {
                    settings.playerHeightSetting = 0.0f;
                    settings.guiFollowSetting = 1;
                    settings.leftHandedSetting = 0;
                }

                ImGui::Separator();
                ImGui::Text("Cutscenes");
                int cutsceneMode = settings.cutsceneCameraMode.getLE();
                int currentCutsceneModeIdx = cutsceneMode - 1;
                if (currentCutsceneModeIdx < 0) currentCutsceneModeIdx = 1; 
                if (ImGui::Combo("Cutscene Camera", &currentCutsceneModeIdx, "First Person (Always)\0Recommended (Mix)\0Third Person (Always)\0\0")) {
                     settings.cutsceneCameraMode = currentCutsceneModeIdx + 1;
                     changed = true;
                }

                bool blackBars = settings.UseBlackBarsForCutscenes();
                if (ImGui::Checkbox("Cinematic Black Bars", &blackBars)) { settings.cutsceneBlackBars = blackBars ? 1 : 0; changed = true; }

                ImGui::Separator();
                ImGui::Text("Display & Debug");

                bool crop16x9 = settings.ShouldFlatPreviewBeCroppedTo16x9();
                if (ImGui::Checkbox("Crop Flat View to 16:9", &crop16x9)) { settings.cropFlatTo16x9Setting = crop16x9 ? 1 : 0; changed = true; }

                bool debugOverlay = settings.ShowDebugOverlay();
                if (ImGui::Checkbox("Show Debug Overlay", &debugOverlay)) { settings.enableDebugOverlay = debugOverlay ? 1 : 0; changed = true; }

                ImGui::Separator();
                ImGui::Text("Fixes");
                int angularFix = settings.buggyAngularVelocity.getLE();
                const char* angularOptions[] = { "Auto (Oculus Link)", "Forced On", "Forced Off" };
                if (angularFix < 0 || angularFix > 2) angularFix = 0;
                if (ImGui::Combo("Angular Velocity Fixer", &angularFix, angularOptions, 3)) {
                    settings.buggyAngularVelocity = angularFix;
                    changed = true;
                }

                if (changed) {
                    CemuHooks::SetSettings(settings);
                }

                ImGui::PopItemWidth();
                ImGui::Unindent(10.0f);
                
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
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

    // try to delete the staging buffer of the controller scheme textures if possible
    for (auto imagePage : m_helpImagePages) {
        for (auto& helpImage : imagePage) {
            helpImage.m_image->vkTryToFinishAnyUploads(cb);
        }
    }

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