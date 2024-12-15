#include "vulkan.h"
#include "instance.h"


RND_Vulkan::ImGuiOverlay::ImGuiOverlay(VkCommandBuffer cb, uint32_t width, uint32_t height, VkFormat format) {
    m_context = ImGui::CreateContext();

    // get queue
    VkQueue queue = nullptr;
    VRManager::instance().VK->GetDeviceDispatch()->GetDeviceQueue(VRManager::instance().VK->GetDevice(), 0, 0, &queue);

    // create descriptor pool
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
        .format = format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
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
    ImGui::GetIO().DisplaySize = ImVec2((float)width, (float)height);

    // load vulkan functions
    checkAssert(ImGui_ImplVulkan_LoadFunctions([](const char* funcName, void* data_queue) {
        VkDevice device = VRManager::instance().VK->GetDevice();
        PFN_vkVoidFunction addr = VRManager::instance().VK->GetDeviceDispatch()->GetDeviceProcAddr(device, funcName);

        if (addr == nullptr) {
            // Log::print("Failed to load function {}", funcName);
            addr = VRManager::instance().VK->GetInstanceDispatch()->GetInstanceProcAddr(VRManager::instance().VK->GetInstance(), funcName);
            Log::print("Loaded function {} at {} using instance", funcName, (void*)addr);
        }
        else {
            Log::print("Loaded function {} at {}", funcName, (void*)addr);
        }

        return addr;
    }), "Failed to load vulkan functions for ImGui");

    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = VRManager::instance().VK->m_instance,
        .PhysicalDevice = VRManager::instance().VK->m_physicalDevice,
        .Device = VRManager::instance().VK->m_device,
        .QueueFamily = 0,
        .Queue = queue,
        .DescriptorPool = m_descriptorPool,
        .RenderPass = m_renderPass,
        .MinImageCount = (uint32_t)m_framebuffers.size(),
        .ImageCount = (uint32_t)m_framebuffers.size(),
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,

        .UseDynamicRendering = false,

        .Allocator = nullptr,
        .CheckVkResultFn = nullptr,
        .MinAllocationSize = 1024 * 1024
    };
    checkAssert(ImGui_ImplVulkan_Init(&init_info), "Failed to initialize ImGui");

    for (auto& framebuffer : m_framebuffers) {
        framebuffer = std::make_unique<VulkanFramebuffer>(width, height, format, m_renderPass);
    }

    Log::print("Initializing fonts for ImGui...");
    ImGui_ImplVulkan_CreateFontsTexture(cb);
    Log::print("Fonts initialized for ImGui");

    // find HWND that starts with Cemu in its title
    HWND m_cemuWindow = NULL;
    DWORD pid = GetCurrentProcessId();
    struct {
        HWND hwnd;
        DWORD pid;
    } data = { m_cemuWindow, pid };
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        // get the process id of the window
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);

        if (windowPid == ((decltype(data)*)lParam)->pid) {
            // get the title of the window
            char title[256];
            GetWindowText(hwnd, title, sizeof(title));

            // check if the title starts with Cemu
            if (strncmp(title, "Cemu", 4) == 0) {
                ((decltype(data)*)lParam)->hwnd = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, (LPARAM)&data);

    // find the most nested child window since that's the rendering window
    HWND childWindow = GetWindow(m_cemuWindow, GW_CHILD);
    while (childWindow != nullptr) {
        m_cemuWindow = childWindow;
        childWindow = GetWindow(m_cemuWindow, GW_CHILD);
    }

    m_cemuRenderWindow = data.hwnd;

    m_mainFramebuffer = std::make_unique<VulkanTexture>(width, height, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    m_hudFramebuffer = std::make_unique<VulkanTexture>(width, height, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    // create sampler
    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = -1000.0f;
    samplerInfo.maxLod = 1000.0f;
    checkVkResult(VRManager::instance().VK->GetDeviceDispatch()->CreateSampler(VRManager::instance().VK->GetDevice(), &samplerInfo, nullptr, &m_sampler), "Failed to create sampler for ImGui");
}

RND_Vulkan::ImGuiOverlay::~ImGuiOverlay() {
    if (m_mainFramebufferDescriptorSet != VK_NULL_HANDLE)
        ImGui_ImplVulkan_RemoveTexture(m_mainFramebufferDescriptorSet);
    if (m_mainFramebuffer != nullptr)
        m_mainFramebuffer.reset();
    if (m_hudFramebufferDescriptorSet != VK_NULL_HANDLE)
        ImGui_ImplVulkan_RemoveTexture(m_hudFramebufferDescriptorSet);
    if (m_hudFramebuffer != nullptr)
        m_hudFramebuffer.reset();

    if (m_sampler != VK_NULL_HANDLE)
        VRManager::instance().VK->GetDeviceDispatch()->DestroySampler(VRManager::instance().VK->GetDevice(), m_sampler, nullptr);
    if (m_renderPass != VK_NULL_HANDLE)
        VRManager::instance().VK->GetDeviceDispatch()->DestroyRenderPass(VRManager::instance().VK->GetDevice(), m_renderPass, nullptr);
    if (m_descriptorPool != VK_NULL_HANDLE)
        VRManager::instance().VK->GetDeviceDispatch()->DestroyDescriptorPool(VRManager::instance().VK->GetDevice(), m_descriptorPool, nullptr);

    ImGui_ImplVulkan_Shutdown();
    ImGui::DestroyContext(m_context);
}

void RND_Vulkan::ImGuiOverlay::BeginFrame() {
    checkAssert(alreadyStartedFrameOnce == false, "BeginFrame called before EndFrame");
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    if (m_mainFramebufferDescriptorSet == VK_NULL_HANDLE) {
        m_mainFramebufferDescriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_mainFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    if (m_hudFramebufferDescriptorSet == VK_NULL_HANDLE) {
        m_hudFramebufferDescriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_hudFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }


    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    // calculate width minus the retina scaling
    ImVec2 viewportPanelSize = ImGui::GetIO().DisplaySize;
    viewportPanelSize.x = viewportPanelSize.x / ImGui::GetIO().DisplayFramebufferScale.x;
    viewportPanelSize.y = viewportPanelSize.y / ImGui::GetIO().DisplayFramebufferScale.y;

    // center position using aspect ratio
    ImVec2 centerPos = ImVec2((viewportPanelSize.x - viewportPanelSize.y * m_mainFramebufferAspectRatio) / 2, 0);
    ImVec2 centerSize = ImVec2(viewportPanelSize.y * m_mainFramebufferAspectRatio, viewportPanelSize.y);

    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(viewport->WorkSize);
        // ImGui::Begin("HUD Background", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::Begin("HUD Background");

        // m_hudFramebufferDescriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_hudFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // m_hudFramebufferDescriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_hudFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
        ImGui::Image((ImTextureID)m_hudFramebufferDescriptorSet, ImVec2(1280, 720));
        ImGui::End();
    }

    {
        ImGui::SetNextWindowPos(centerPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("3D Scene Background");
        // ImGui::Begin("3D Scene Background", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // m_mainFramebufferDescriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_mainFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        ImGui::Image((ImTextureID)m_mainFramebufferDescriptorSet, centerSize);

        ImGui::End();
    }

    // ImGui::ShowDemoWindow();
    alreadyStartedFrameOnce = true;
}

void RND_Vulkan::ImGuiOverlay::Draw3DLayerAsBackground(VkCommandBuffer cb, VkImage srcImage, float aspectRatio) {
    // copy the images from the 3D layer to the imgui ones so that we can draw them the next frame as a background
    m_mainFramebuffer->vkPipelineBarrier(cb);
    m_mainFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
    m_mainFramebuffer->vkClear(cb, { 1.0f, 0.0f, 0.0f, 1.0f });
    m_mainFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
    m_mainFramebuffer->vkPipelineBarrier(cb);

    // m_mainFramebuffer->vkPipelineBarrier(cb);
    // m_mainFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // m_mainFramebuffer->vkCopyFromImage(cb, srcImage);
    // m_mainFramebuffer->vkPipelineBarrier(cb);
    // m_mainFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);

    if (m_mainFramebufferDescriptorSet == VK_NULL_HANDLE) {
        m_mainFramebufferDescriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_mainFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
    }
    m_mainFramebufferAspectRatio = aspectRatio;
}

void RND_Vulkan::ImGuiOverlay::DrawHUDLayerAsBackground(VkCommandBuffer cb, VkImage srcImage) {
    // copy the images from the 2D layer to the imgui ones so that we can draw them the next frame as a background
    m_hudFramebuffer->vkPipelineBarrier(cb);
    m_hudFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
    m_hudFramebuffer->vkClear(cb, { 0.0f, 1.0f, 1.0f, 1.0f });
    m_hudFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
    m_hudFramebuffer->vkPipelineBarrier(cb);

    // m_hudFramebuffer->vkPipelineBarrier(cb);
    // m_hudFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // m_hudFramebuffer->vkCopyFromImage(cb, srcImage);
    // m_hudFramebuffer->vkPipelineBarrier(cb);
    // m_hudFramebuffer->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
    //
    // if (m_hudFramebufferDescriptorSet == VK_NULL_HANDLE) {
    //     m_hudFramebufferDescriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_hudFramebuffer->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
    // }
}

void RND_Vulkan::ImGuiOverlay::Render() {
    ImGui::Render();
}

void RND_Vulkan::ImGuiOverlay::DrawOverlayToImage(VkCommandBuffer cb, VkImage destImage) {
    if (!alreadyStartedFrameOnce) {
        Log::print("DrawOverlayToImage called before BeginFrame");
        return;
    }

    auto* dispatch = VRManager::instance().VK->GetDeviceDispatch();

    // transition framebuffer to color attachment
    m_framebuffers[m_framebufferIdx]->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    m_framebuffers[m_framebufferIdx]->vkPipelineBarrier(cb);

    // start render pass
    VkClearValue clearValue = { .color = { 0.0f, 0.0f, 0.0f, 1.0f } };

    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_renderPass,
        .framebuffer = m_framebuffers[m_framebufferIdx]->GetFramebuffer(),
        .renderArea = {
            .offset = { 0, 0 },
            .extent = { (uint32_t)ImGui::GetIO().DisplaySize.x, (uint32_t)ImGui::GetIO().DisplaySize.y }
        },
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    dispatch->CmdBeginRenderPass(cb, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // render imgui
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);

    // end render pass
    dispatch->CmdEndRenderPass(cb);

    // copy from framebuffer to destination image
    m_framebuffers[m_framebufferIdx]->vkPipelineBarrier(cb);
    m_framebuffers[m_framebufferIdx]->vkTransitionLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // copy image
    m_framebuffers[m_framebufferIdx]->vkCopyToImage(cb, destImage);
    m_framebuffers[m_framebufferIdx]->vkPipelineBarrier(cb);

    m_framebufferIdx++;
    if (m_framebufferIdx >= m_framebuffers.size())
        m_framebufferIdx = 0;

    alreadyStartedFrameOnce = false;
}


void RND_Vulkan::ImGuiOverlay::UpdateControls() {
    POINT p;
    GetCursorPos(&p);

    ScreenToClient(m_cemuRenderWindow, &p);

    // scale mouse position with the texture size
    uint32_t framebufferWidth = (uint32_t)ImGui::GetIO().DisplaySize.x;
    uint32_t framebufferHeight = (uint32_t)ImGui::GetIO().DisplaySize.y;

    // calculate how many client side pixels are used on the border since its not a 16:9 aspect ratio
    RECT rect;
    GetClientRect(m_cemuRenderWindow, &rect);
    uint32_t nonCenteredWindowWidth = rect.right - rect.left;
    uint32_t nonCenteredWindowHeight = rect.bottom - rect.top;

    // calculate window size without black bars due to a non-16:9 aspect ratio
    uint32_t windowWidth = nonCenteredWindowWidth;
    uint32_t windowHeight = nonCenteredWindowHeight;
    if (nonCenteredWindowWidth * 9 > nonCenteredWindowHeight * 16) {
        windowWidth = nonCenteredWindowHeight * 16 / 9;
    }
    else {
        windowHeight = nonCenteredWindowWidth * 9 / 16;
    }

    // calculate the black bars
    uint32_t blackBarWidth = (nonCenteredWindowWidth - windowWidth) / 2;
    uint32_t blackBarHeight = (nonCenteredWindowHeight - windowHeight) / 2;

    // the actual window is centered, so add offsets to both x and y on both sides
    p.x = p.x - blackBarWidth;
    p.y = p.y - blackBarHeight;

    // ImGui::GetIO().DisplaySize = ImVec2((float)windowWidth, (float)windowHeight);
    ImGui::GetIO().DisplayFramebufferScale = ImVec2((float)framebufferWidth / (float)windowWidth, (float)framebufferHeight / (float)windowHeight);

    ImGui::GetIO().MousePos = ImVec2((float)p.x, (float)p.y);
    ImGui::GetIO().MouseDown[0] = GetAsyncKeyState(VK_LBUTTON) & 0x8000;
    ImGui::GetIO().MouseDown[1] = GetAsyncKeyState(VK_RBUTTON) & 0x8000;
    ImGui::GetIO().MouseDown[2] = GetAsyncKeyState(VK_MBUTTON) & 0x8000;
    return;
}