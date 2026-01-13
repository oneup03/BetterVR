#include "hooking/cemu_hooks.h"
#include "hooking/entity_debugger.h"
#include "instance.h"
#include "utils/vulkan_utils.h"
#include "vulkan.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// include font and assets
#include "font_kenney.h"
#include "font_roboto.h"
#include "utils/controller_image.h"


void SetupImGuiStyle() {
    // Dark Ruda style by Raikiri from ImThemes
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(32.0f, 32.0f);
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(4.0f, 3.0f);
    style.FrameRounding = 4.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = 21.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 14.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.TabBorderSize = 0.0f;
    //style.TabMinWidthForCloseButton = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    style.Colors[ImGuiCol_Text] = ImVec4(0.9490196f, 0.95686275f, 0.9764706f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.35686275f, 0.41960785f, 0.46666667f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10980392f, 0.14901961f, 0.16862746f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.14901961f, 0.1764706f, 0.21960784f, 1.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.078431375f, 0.078431375f, 0.078431375f, 0.94f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.078431375f, 0.09803922f, 0.11764706f, 1.0f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.24705882f, 0.28627452f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.11764706f, 0.2f, 0.2784314f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.08627451f, 0.11764706f, 0.13725491f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08627451f, 0.11764706f, 0.13725491f, 0.65f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.078431375f, 0.09803922f, 0.11764706f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.51f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.14901961f, 0.1764706f, 0.21960784f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.019607844f, 0.019607844f, 0.019607844f, 0.39f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.2f, 0.24705882f, 0.28627452f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.1764706f, 0.21960784f, 0.24705882f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.08627451f, 0.20784314f, 0.30980393f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.2784314f, 0.5568628f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.2784314f, 0.5568628f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.36862746f, 0.60784316f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.24705882f, 0.28627452f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2784314f, 0.5568628f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.05882353f, 0.5294118f, 0.9764706f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.24705882f, 0.28627452f, 0.55f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 0.8f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 1.0f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.2f, 0.24705882f, 0.28627452f, 1.0f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.09803922f, 0.4f, 0.7490196f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.09803922f, 0.4f, 0.7490196f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 0.25f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 0.95f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.10980392f, 0.14901961f, 0.16862746f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 0.8f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.2f, 0.24705882f, 0.28627452f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.10980392f, 0.14901961f, 0.16862746f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.10980392f, 0.14901961f, 0.16862746f, 1.0f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.60784316f, 0.60784316f, 0.60784316f, 1.0f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.42745098f, 0.34901962f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8980392f, 0.69803923f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882353f, 0.1882353f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30980393f, 0.30980393f, 0.34901962f, 1.0f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.22745098f, 0.22745098f, 0.24705882f, 1.0f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 0.35f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.9f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.35f);
}

RND_Renderer::ImGuiOverlay::ImGuiOverlay(VkCommandBuffer cb, VkExtent2D fbRes, VkFormat fbFormat): m_outputRes(fbRes) {
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = "BetterVR_settings.ini";
    InitSettings();
    ImGui::LoadIniSettingsFromDisk("BetterVR_settings.ini");
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImFontConfig fontCfg{};
    fontCfg.OversampleH = 8;
    fontCfg.OversampleV = 8;
    fontCfg.FontDataOwnedByAtlas = false;
    ImFont* textFont = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(roboto_compressed_data, roboto_compressed_size, 16.0f, &fontCfg);

    ImFontConfig iconCfg{};
    iconCfg.MergeMode = true;
    iconCfg.GlyphMinAdvanceX = 16.0f;
    iconCfg.OversampleH = 8;
    iconCfg.OversampleV = 8;
    iconCfg.FontDataOwnedByAtlas = false;
    static const ImWchar icon_ranges[] = { ICON_MIN_KI, ICON_MAX_KI, 0 };
    iconCfg.GlyphRanges = icon_ranges;
    ImFont* iconFont = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(kenney_compressed_data, kenney_compressed_size, 16.0f, &iconCfg);
    if (iconFont == nullptr || textFont == nullptr) {
        Log::print<ERROR>("Failed to load custom fonts for ImGui overlay, using default font");
    }
    else {
        ImGui::GetIO().Fonts->Build();
    }

    SetupImGuiStyle();

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

        frame.mainFramebuffer = std::make_unique<VulkanTexture>(fbRes.width, fbRes.height, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, true);
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
        image.m_image = new VulkanTexture{ (uint32_t)width, (uint32_t)height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT };
        image.m_image->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
        image.m_image->vkUpload(cb, img, width * height * 4);
        image.m_image->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);

        stbi_image_free(img);

        image.m_imageDS = ImGui_ImplVulkan_AddTexture(m_sampler, image.m_image->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
        return image;
    };

    std::vector<HelpImage> page1;
    page1.emplace_back(createHelpImage((stbi_uc const*)controls, sizeof(controls)));
    page1.push_back(createHelpImage((stbi_uc const*)equip, sizeof(equip)));
    page1.push_back(createHelpImage((stbi_uc const*)swing, sizeof(swing)));
    page1.push_back(createHelpImage((stbi_uc const*)whistle_and_magnesis, sizeof(whistle_and_magnesis)));
    m_helpImagePages.push_back(page1);

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
    ImGui::GetIO().FontGlobalScale = 1.0f;

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

    // calculate a virtual resolution to keep UI size consistent
    float uiScaleFactor = (float)renderPhysicalHeight / 1080.0f;
    uiScaleFactor = std::max(uiScaleFactor, 0.1f);

    uiScaleFactor *= 2.0f;

    ImVec2 logicalRes = ImVec2(physicalRes.x / uiScaleFactor, physicalRes.y / uiScaleFactor);

    ImGui::GetIO().DisplaySize = logicalRes;

    // calculate the black bars
    uint32_t blackBarWidth = (nonCenteredWindowWidth - renderPhysicalWidth) / 2;
    uint32_t blackBarHeight = (nonCenteredWindowHeight - renderPhysicalHeight) / 2;

    // adjust the framebuffer scale
    ImGui::GetIO().DisplayFramebufferScale = framebufferRes / logicalRes;

    // the actual window is centered, so add offsets to both x and y on both sides
    p.x = p.x - blackBarWidth;
    p.y = p.y - blackBarHeight;

    // update mouse controls and keyboard input
    bool isWindowFocused = CemuHooks::m_cemuTopWindow == GetForegroundWindow();

    //ImGui::GetIO().AddFocusEvent(true);
    if (isWindowFocused) {
        // update mouse state depending on if the window is focused
        ImGui::GetIO().AddMousePosEvent((float)p.x / uiScaleFactor, (float)p.y / uiScaleFactor);

        ImGui::GetIO().AddMouseButtonEvent(0, GetAsyncKeyState(VK_LBUTTON) & 0x8000);
        ImGui::GetIO().AddMouseButtonEvent(1, GetAsyncKeyState(VK_RBUTTON) & 0x8000);
        ImGui::GetIO().AddMouseButtonEvent(2, GetAsyncKeyState(VK_MBUTTON) & 0x8000);
    }

    if (GetSettings().ShowDebugOverlay() && isWindowFocused) {
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
        const bool shouldCrop3DTo16_9 = GetSettings().cropFlatTo16x9 == 1;

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

    if (GetSettings().ShowDebugOverlay()) {
        VRManager::instance().Hooks->m_entityDebugger->DrawEntityInspector();
        VRManager::instance().Hooks->DrawDebugOverlays();
    }

    if (((renderBackground && GetSettings().performanceOverlay == 1) || GetSettings().performanceOverlay == 2) && !VRManager::instance().XR->m_isMenuOpen) {
        EntityDebugger::DrawFPSOverlay(renderer);
    }

    DrawHelpMenu();
}

constexpr uint8_t TOTAL_TABS = 4;
void RND_Renderer::ImGuiOverlay::ProcessInputs(OpenXR::InputState& inputs, const VPADStatus& vpadStatus) {
    auto& isMenuOpen = VRManager::instance().XR->m_isMenuOpen;
    if (!isMenuOpen)
        return;

    auto& io = ImGui::GetIO();
    bool inGame = inputs.shared.in_game;

    bool backDown;
    bool confirmDown;
    bool pageLeft;
    bool pageRight;
    XrActionStateVector2f stick;
    if (inputs.shared.in_game) {
        stick = inputs.inGame.move;
        backDown = inputs.inGame.jump_cancel.currentState;
        confirmDown = inputs.inGame.run_interact.currentState;
        pageLeft = inputs.inGame.useLeftItem.currentState && inputs.inGame.useLeftItem.changedSinceLastSync;
        pageRight = inputs.inGame.useRightItem.currentState && inputs.inGame.useRightItem.changedSinceLastSync;

    }
    else {
        stick = inputs.inMenu.navigate;
        backDown = inputs.inMenu.back.currentState;
        confirmDown = inputs.inMenu.select.currentState;
        pageLeft = inputs.inMenu.leftTrigger.currentState && inputs.inMenu.leftTrigger.changedSinceLastSync;
        pageRight = inputs.inMenu.rightTrigger.currentState && inputs.inMenu.rightTrigger.changedSinceLastSync;
    }

    uint32_t hold = vpadStatus.hold.getLE();
    backDown |= (hold & VPAD_BUTTON_B) != 0;
    confirmDown |= (hold & VPAD_BUTTON_A) != 0;

    static bool wasBDown = false;
    bool isBDown = (hold & VPAD_BUTTON_B) != 0;
    bool bPressed = isBDown && !wasBDown;
    wasBDown = isBDown;
    
    // imgui wants us to only have state changes, and we also want to refiring DPAD inputs (used for moving the menu cursor) when held down
    constexpr float THRESHOLD_PRESS = 0.5f;
    constexpr float THRESHOLD_RELEASE = 0.3f;
    constexpr double HORIZONTAL_REFIRE_DELAY = 0.5f;
    constexpr double VERTICAL_REFIRE_DELAY = 1.0f;

    static bool dpadState[4] = { false };
    static double lastRefireTime[8] = { 0.0 }; // 0-3 Dpad, 4 B, 5 A, 6 LB, 7 RB
    double currentTime = ImGui::GetTime();

    auto updateDpadState = [&](int idx, float val, bool positive) {
        bool isPressed = dpadState[idx];
        if (positive) {
            if (!isPressed && val >= THRESHOLD_PRESS) isPressed = true;
            else if (isPressed && val <= THRESHOLD_RELEASE)
                isPressed = false;
        }
        else {
            if (!isPressed && val <= -THRESHOLD_PRESS) isPressed = true;
            else if (isPressed && val >= -THRESHOLD_RELEASE)
                isPressed = false;
        }
        dpadState[idx] = isPressed;
        return isPressed;
    };

    auto applyInput = [&](ImGuiKey key, bool isPressed, float refireDelay, int idx) {
        if (isPressed) {
            if (currentTime - lastRefireTime[idx] >= refireDelay || lastRefireTime[idx] == 0.0) {
                io.AddKeyEvent(key, true);
                lastRefireTime[idx] = currentTime;
                return true;
            }
            else {
                io.AddKeyEvent(key, false);
                return false;
            }
        }
        else {
            io.AddKeyEvent(key, false);
            lastRefireTime[idx] = 0.0;
            return false;
        }
    };

    applyInput(ImGuiKey_GamepadDpadUp, updateDpadState(0, stick.currentState.y, true) || ((hold & VPAD_BUTTON_UP) != 0), VERTICAL_REFIRE_DELAY, 0);
    applyInput(ImGuiKey_GamepadDpadDown, updateDpadState(1, stick.currentState.y, false) || ((hold & VPAD_BUTTON_DOWN) != 0), VERTICAL_REFIRE_DELAY, 1);
    applyInput(ImGuiKey_GamepadDpadLeft, updateDpadState(2, stick.currentState.x, false) || ((hold & VPAD_BUTTON_LEFT) != 0), HORIZONTAL_REFIRE_DELAY, 2);
    applyInput(ImGuiKey_GamepadDpadRight, updateDpadState(3, stick.currentState.x, true) || ((hold & VPAD_BUTTON_RIGHT) != 0), HORIZONTAL_REFIRE_DELAY, 3);

    // convert B/A to ImGui gamepad face buttons
    applyInput(ImGuiKey_GamepadFaceRight, backDown, VERTICAL_REFIRE_DELAY, 4);
    applyInput(ImGuiKey_GamepadFaceDown, confirmDown, VERTICAL_REFIRE_DELAY, 5);

    // triggers for tab switching
    bool l1 = applyInput(ImGuiKey_GamepadL1, pageLeft || ((hold & VPAD_BUTTON_L) != 0), VERTICAL_REFIRE_DELAY, 6);
    bool r1 = applyInput(ImGuiKey_GamepadR1, pageRight || ((hold & VPAD_BUTTON_R) != 0), VERTICAL_REFIRE_DELAY, 7);

    VRManager::instance().XR->m_forceTabChange = false;
    if (l1 || r1) {
        auto& currTab = VRManager::instance().XR->m_currMenuTab;
        uint8_t prevTab = currTab;
        if (l1) {
            currTab = (currTab - 1 + TOTAL_TABS) % TOTAL_TABS;
        }
        if (r1) {
            currTab = (currTab + 1) % TOTAL_TABS;
        }

        if (prevTab != currTab) {
            VRManager::instance().XR->m_forceTabChange = true;
        }
    }

    // prevent exiting menu if a popup or field is being edited
    if (ImGui::IsAnyItemActive() && ImGui::IsPopupOpen(NULL, ImGuiPopupFlags_AnyPopupId + ImGuiPopupFlags_AnyPopupLevel)) {
        return;
    }


    // if no inputs or popup is open, allow closing the menu using the cancel/back button
    bool shouldClose = false;
    if (inGame) {
        if (inputs.inGame.jump_cancel.changedSinceLastSync && inputs.inGame.jump_cancel.currentState) {
            shouldClose = true;
            inputs.inGame.jump_cancel.currentState = XR_FALSE;
        }
    }
    else {
        if (inputs.inMenu.back.changedSinceLastSync && inputs.inMenu.back.currentState) {
            shouldClose = true;
        }
    }

    if (shouldClose || bPressed) {
        VRManager::instance().XR->m_isMenuOpen = false;
    }
}

void RND_Renderer::ImGuiOverlay::DrawHelpMenu() {
    auto& isMenuOpen = VRManager::instance().XR->m_isMenuOpen;
    auto& settings = GetSettings();

    float alphaForNotify = settings.tutorialPromptShown ? 0.9f : 0.98f;
    float timeLimit = settings.tutorialPromptShown ? 14.0f : 60.0f;

    if (!settings.tutorialPromptShown) {
        if (isMenuOpen || ImGui::GetTime() > timeLimit) {
            settings.tutorialPromptShown = true;
            ImGui::SaveIniSettingsToDisk("BetterVR_settings.ini");
        }
    }

    if (!isMenuOpen && ImGui::GetTime() < timeLimit) {
        ImVec2 fullWindowWidth = ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
        ImGui::SetNextWindowBgAlpha(alphaForNotify);
        ImGui::SetNextWindowPos(fullWindowWidth * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("HelpNotify", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs)) {
            if (!settings.tutorialPromptShown) {
                ImGui::Text("First-Time Setup:");
                ImGui::Separator();
                ImGui::Text("To get started, open the BetterVR menu to configure various settings and to see the controller guide.");
                ImGui::Text("This is where you can adjust the camera mode, player height, and other options to suit your preferences.");
                ImGui::Spacing();
                ImGui::Text("You can always access this menu by holding the " ICON_KI_BUTTON_X " for 1 second.");
                ImGui::Text("(Alternatively, for Valve Index Controllers, hold " ICON_KI_BUTTON_A ". For regular game controllers, hold " ICON_KI_BUTTON_START " instead)");
                ImGui::Text("");
                ImGui::Text("TO CONTINUE: Try holding the button and open the menu");
            }
            else {
                ImGui::Text("Hold " ICON_KI_BUTTON_X " (or " ICON_KI_BUTTON_A " for Valve Index Controllers or " ICON_KI_BUTTON_START " for Xbox/Playstation etc. controllers) To Open Mod Settings");
            }
        }
        ImGui::End();
    }

    static bool wasMenuPrevOpened = false;

    if (!isMenuOpen && wasMenuPrevOpened) {
        wasMenuPrevOpened = false;
    }

    if (!isMenuOpen)
        return;

    if (isMenuOpen && !wasMenuPrevOpened) {
        ImGui::SetNextWindowFocus();
        wasMenuPrevOpened = true;
    }

    ImVec2 fullWindowWidth = ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
    ImVec2 windowWidth = fullWindowWidth * ImVec2(0.75f, 1.0f);

    // Layout helpers
    auto DrawSettingRow = [&](const char* label, auto drawWidget) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        float startPos = windowWidth.x * 0.45f;
        if (ImGui::GetCursorPosX() < startPos)
            ImGui::SetCursorPosX(startPos);

        ImGui::PushItemWidth(windowWidth.x * 0.50f);
        drawWidget();
        ImGui::PopItemWidth();
    };

    bool setTab = VRManager::instance().XR->m_forceTabChange;
    uint8_t selectedTab = VRManager::instance().XR->m_currMenuTab;

    auto DrawStyledTab = [&](const char* label, uint32_t tabIdx, ImGuiTabItemFlags extraFlags = ImGuiTabItemFlags_None) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImGui::GetStyle().FramePadding + ImVec2(0, 2.0f));
        auto tabBar = ImGui::BeginTabItem(label, nullptr, ((setTab && selectedTab == tabIdx) ? ImGuiTabItemFlags_SetSelected : 0) | extraFlags);
        ImGui::PopStyleVar();
        return tabBar;
    };

    ImGui::SetNextWindowPos(fullWindowWidth * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(windowWidth, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    bool shouldStayOpen = true;

    if (ImGui::Begin("BetterVR Settings & Help##Settings", &shouldStayOpen, ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        bool changed = false;

        ImGui::Indent(10.0f);
        ImGui::Dummy(ImVec2(10.0f, 0.0f));

        float footerHeight = ImGui::GetFrameHeight() * 1.5f;
        if (ImGui::BeginChild("Content", ImVec2(0, -footerHeight), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground)) {

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImGui::GetStyle().FramePadding + ImVec2(0, 2.0f));
            if (ImGui::BeginTabBar("HelpMenuTabs")) {
                ImGui::PopStyleVar();
                if (DrawStyledTab(ICON_KI_COG "Settings", 0)) {
                    ImGui::Separator();
                    int cameraMode = (int)settings.cameraMode.load();
                    DrawSettingRow("Camera Mode", [&]() {
                        if (ImGui::RadioButton("First Person (Recommended)", &cameraMode, 1)) {
                            settings.cameraMode = CameraMode::FIRST_PERSON;
                            changed = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::RadioButton("Third Person", &cameraMode, 0)) {
                            settings.cameraMode = CameraMode::THIRD_PERSON;
                            changed = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::RadioButton("OG", &cameraMode, 2)) {
                            settings.cameraMode = CameraMode::ORIGINAL;
                            changed = true;
                        }
                    });

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                    ImGui::Text("Camera / Player Options");
                    ImGui::PopStyleColor();
                    if (cameraMode == 0 || cameraMode == 2) {
                        float distance = settings.thirdPlayerDistance;
                        DrawSettingRow("Camera Distance", [&]() {
                            if (ImGui::SliderFloat("##CameraDistance", &distance, 0.5f, 0.65f, "%.2f")) {
                                settings.thirdPlayerDistance = distance;
                                changed = true;
                            }
                        });
                    }
                    else {
                        float height = settings.playerHeightOffset;
                        std::string heightOffsetValueStr = std::format("{0}{1:.02f} meters / {0}{2:.02f} feet", (height > 0.0f ? "+" : ""), height, height * 3.28084f);
                        DrawSettingRow("Height Offset", [&]() {
                            ImGui::PushItemWidth(windowWidth.x * 0.35f);
                            if (ImGui::SliderFloat("##HeightOffset", &height, -0.5f, 1.0f, heightOffsetValueStr.c_str())) {
                                settings.playerHeightOffset = height;
                                changed = true;
                            }
                            ImGui::PopItemWidth();
                            ImGui::SameLine();
                            if (ImGui::Button("Reset")) {
                                settings.playerHeightOffset = 0.0f;
                                changed = true;
                            }
                        });

                        //bool leftHanded = settings.leftHanded;
                        //if (ImGui::Checkbox("Left Handed Mode", &leftHanded)) {
                        //    settings.leftHanded = leftHanded ? 1 : 0;
                        //    changed = true;
                        //}
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                    ImGui::Text("Cutscenes");
                    ImGui::PopStyleColor();

                    int cutsceneMode = (int)settings.cutsceneCameraMode.load();
                    int currentCutsceneModeIdx = cutsceneMode - 1;
                    if (currentCutsceneModeIdx < 0) currentCutsceneModeIdx = 1;

                    if (settings.GetCameraMode() != CameraMode::THIRD_PERSON && GetSettings().GetCameraMode() != CameraMode::ORIGINAL) {
                        DrawSettingRow("Camera In Cutscenes", [&]() {
                            if (ImGui::Combo("##CutsceneCamera", &currentCutsceneModeIdx, "First Person (Always)\0Optimal Settings (Mix Of Third/First)\0Third Person (Always)\0\0")) {
                                settings.cutsceneCameraMode = (EventMode)(currentCutsceneModeIdx + 1);
                                changed = true;
                            }
                        });
                    }

                    bool blackBars = settings.useBlackBarsForCutscenes;
                    DrawSettingRow("Black Bars In Third-Person Cutscenes", [&]() {
                        if (ImGui::Checkbox("##BlackBars", &blackBars)) {
                            settings.useBlackBarsForCutscenes = blackBars ? 1 : 0;
                            changed = true;
                        }
                    });

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                    ImGui::Text("UI");
                    ImGui::PopStyleColor();
                    if (cameraMode == 1 || cameraMode == 2) {
                        bool guiFollow = settings.uiFollowsGaze;
                        DrawSettingRow("UI Follows Where You Look", [&]() {
                            if (ImGui::Checkbox("##UIFollow", &guiFollow)) {
                                settings.uiFollowsGaze = guiFollow ? 1 : 0;
                                changed = true;
                            }
                        });
                    }
                    else {
                        ImGui::Text("");
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                    ImGui::Text("Input");
                    ImGui::PopStyleColor();
                    if (cameraMode == 1) {
                        float deadzone = settings.stickDeadzone;
                        DrawSettingRow("Thumbstick Deadzone", [&]() {
                            ImGui::PushItemWidth(windowWidth.x * 0.35f);
                            if (ImGui::SliderFloat("##StickDeadzone", &deadzone, 0.0f, 0.5f, "%.2f")) {
                                settings.stickDeadzone = deadzone;
                                changed = true;
                            }
                            ImGui::PopItemWidth();
                            ImGui::SameLine();
                            if (ImGui::Button("Reset##Deadzone")) {
                                settings.stickDeadzone = ModSettings::kDefaultStickDeadzone;
                                changed = true;
                            }
                        });

                        float threshold = settings.axisThreshold;
                        DrawSettingRow("Stick Direction Threshold", [&]() {
                            ImGui::PushItemWidth(windowWidth.x * 0.35f);
                            if (ImGui::SliderFloat("##AxisThreshold", &threshold, 0.1f, 0.9f, "%.2f")) {
                                settings.axisThreshold = threshold;
                                changed = true;
                            }
                            ImGui::PopItemWidth();
                            ImGui::SameLine();
                            if (ImGui::Button("Reset##Threshold")) {
                                settings.axisThreshold = ModSettings::kDefaultAxisThreshold;
                                changed = true;
                            }
                        });

                        DrawSettingRow("Reset Input Thresholds", [&]() {
                            if (ImGui::Button("Reset##InputThresholds")) {
                                settings.stickDeadzone = ModSettings::kDefaultStickDeadzone;
                                settings.axisThreshold = ModSettings::kDefaultAxisThreshold;
                                changed = true;
                            }
                        });
                    }
                    else {
                        ImGui::Text("");
                    }

                    if (ImGui::CollapsingHeader("Advanced Settings")) {
                        bool crop16x9 = settings.cropFlatTo16x9;
                        DrawSettingRow("Crop VR Image To 16:9 For Cemu Window", [&]() {
                            if (ImGui::Checkbox("##Crop16x9", &crop16x9)) {
                                settings.cropFlatTo16x9 = crop16x9 ? 1 : 0;
                                changed = true;
                            }
                        });

                        bool debugOverlay = settings.ShowDebugOverlay();
                        DrawSettingRow("Show Debugging Overlays (for developers)", [&]() {
                            if (ImGui::Checkbox("##DebugOverlay", &debugOverlay)) {
                                settings.enableDebugOverlay = debugOverlay ? 1 : 0;
                                changed = true;
                            }
                        });

                        if (VRManager::instance().XR->m_capabilities.isOculusLinkRuntime) {
                            int angularFix = (int)settings.buggyAngularVelocity.load();
                            const char* angularOptions[] = { "Auto (Oculus Link)", "Forced On", "Forced Off" };
                            if (angularFix < 0 || angularFix > 2) angularFix = 0;

                            DrawSettingRow("Angular Velocity Fixer", [&]() {
                                if (ImGui::Combo("##AngularVelocity", &angularFix, angularOptions, 3)) {
                                    settings.buggyAngularVelocity = (AngularVelocityFixerMode)angularFix;
                                    changed = true;
                                }
                            });
                        }
                        else {
                            settings.buggyAngularVelocity = AngularVelocityFixerMode::AUTO;
                        }
                    }

                    ImGui::EndTabItem();
                }

                if (DrawStyledTab(ICON_KI_INFO_CIRCLE " Help & Controller Guide", 1)) {
                    ImGui::PushItemWidth(windowWidth.x * 0.5f);

                    for (const auto& imagePage : m_helpImagePages) {
                        for (const auto& image : imagePage) {
                            ImGui::PushID(&image);
                            float availWidth = ImGui::GetContentRegionAvail().x;
                            float imageHeight = (availWidth) * ((float)image.m_image->GetHeight() / (float)image.m_image->GetWidth());
                            ImVec2 imageSize = ImVec2(availWidth, imageHeight);
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

                if (DrawStyledTab(ICON_KI_PODIUM " FPS Overlay", 2)) {
                    ImGui::Dummy(ImVec2(0.0f, 10.0f));

                    auto& settings = GetSettings();

                    int performanceOverlay = settings.performanceOverlay;
                    const char* fpsOverlayOptions[] = { "Disable", "Only show in Cemu window", "Show in both Cemu and VR" };
                    if (performanceOverlay < 0 || performanceOverlay > 2) performanceOverlay = 0;

                    DrawSettingRow("Show FPS Overlay", [&]() {
                        if (ImGui::Combo("##FPSOverlay", &performanceOverlay, fpsOverlayOptions, 3)) {
                            settings.performanceOverlay = performanceOverlay;
                            changed = true;
                        }
                    });

                    if (performanceOverlay >= 0) {
                        static const int freqOptions[] = { 30, 60, 72, 80, 90, 120, 144 };
                        int currentFreq = (int)settings.performanceOverlayFrequency.load();
                        int freqIdx = 5; // Default to 90
                        for (int i = 0; i < std::size(freqOptions); i++) {
                            if (freqOptions[i] == currentFreq) {
                                freqIdx = i;
                                break;
                            }
                        }

                        DrawSettingRow("Refresh Rate Of VR Headset For Graph", [&]() {
                            if (ImGui::SliderInt("##RefreshRate", &freqIdx, 0, (int)std::size(freqOptions) - 1, std::format("{} Hz", freqOptions[freqIdx]).c_str())) {
                                settings.performanceOverlayFrequency = freqOptions[freqIdx];
                                changed = true;
                            }
                        });

                        ImGui::Dummy(ImVec2(0.0f, 10.0f));
                        // draw the FPS overlay inside of the menu with an explanation
                        EntityDebugger::DrawFPSOverlayContent(VRManager::instance().XR->GetRenderer(), true);
                    }

                    ImGui::EndTabItem();
                }

                if (DrawStyledTab(ICON_KI_HEART " Credits", 3)) {
                    ImGui::SeparatorText("Project Links");
                    ImGui::TextLinkOpenURL(ICON_KI_GITHUB " https://github.com/Crementif/BotW-BetterVR");
                    ImGui::Text("");

                    ImGui::SeparatorText("Credits");
                    ImGui::Text("Crementif: Main Developer");
                    ImGui::Text("Holydh: Inputs And Gestures");
                    ImGui::Text("Acudofy: Sword & Stab Analysis System");
                    ImGui::Text("leoetlino: Made the BotW Decomp project, which was useful while reverse engineering");
                    ImGui::Text("Exzap: Technical support and optimization help");
                    ImGui::Text("Mako Marci: Made Logo, Controller Guide And Trailer");
                    ImGui::Text("Tim, Mako Marci, Solarwolf07, Elliott Tate & Derra: Helped with QA testing, recording, feedback and support");
                    ImGui::Text("");

                    ImGui::SeparatorText("Donate To Support Development");
                    ImGui::TextWrapped("Hey there!");
                    ImGui::Text("");
                    ImGui::TextWrapped("This mod is free and open-source, but it took a lot of late nights to create.");
                    ImGui::TextWrapped("If you're enjoying the mod and want to vote on new features, you can donate here. Thanks!");
                    ImGui::TextLinkOpenURL("https://github.com/sponsors/Crementif/");

                    ImGui::Text("");
                    ImGui::Text("- Crementif");

                    ImGui::Dummy(ImVec2(0.0f, 10.0f));
                    
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            else {
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            if (changed) {
                ImGui::SaveIniSettingsToDisk("BetterVR_settings.ini");
            }
        }

        ImGui::Unindent(10.0f);
        ImGui::Separator();
        {
            ImGui::Spacing();
            const char* navText = ICON_KI_STICK_LEFT_TOP " Navigate      " ICON_KI_BUTTON_A " Select      " ICON_KI_BUTTON_B " Exit      " ICON_KI_BUTTON_L " " ICON_KI_BUTTON_R " Tabs";
            float textWidth = ImGui::CalcTextSize(navText).x;
            float availW = ImGui::GetContentRegionAvail().x;
            if (availW > textWidth) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - textWidth) * 0.5f);
            }
            ImGui::TextUnformatted(navText);
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    if (!shouldStayOpen) {
        isMenuOpen = false;
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
