#include "renderer.h"
#include "instance.h"
#include "texture.h"
#include "utils/d3d12_utils.h"


RND_Renderer::RND_Renderer(XrSession xrSession): m_session(xrSession) {
    XrSessionBeginInfo m_sessionCreateInfo = { XR_TYPE_SESSION_BEGIN_INFO };
    m_sessionCreateInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    checkXRResult(xrBeginSession(m_session, &m_sessionCreateInfo), "Failed to begin OpenXR session!");
}

RND_Renderer::~RND_Renderer() {
    xrRequestExitSession(m_session);
    if (m_session != XR_NULL_HANDLE) {
        checkXRResult(xrEndSession(m_session), "Failed to end OpenXR session!");
        m_session = XR_NULL_HANDLE;
    }
}

void RND_Renderer::StartFrame() {
    Log::print("RND_Renderer::StartFrame");
    m_isInitialized = true;

    XrFrameWaitInfo waitFrameInfo = { XR_TYPE_FRAME_WAIT_INFO };
    checkXRResult(xrWaitFrame(m_session, &waitFrameInfo, &m_frameState), "Failed to wait for next frame!");

    XrFrameBeginInfo beginFrameInfo = { XR_TYPE_FRAME_BEGIN_INFO };
    checkXRResult(xrBeginFrame(m_session, &beginFrameInfo), "Couldn't begin OpenXR frame!");

    VRManager::instance().D3D12->StartFrame();
    this->UpdateViews(m_frameState.predictedDisplayTime);

    // todo: update this as late as possible
    // VRManager::instance().XR->UpdateSpaces(m_frameState.predictedDisplayTime);

    // todo: I'm not sure if these actions should be separated like we do with the XrViews
    Log::print("Updating actions while being in the {} state", VRManager::instance().Hooks->GetFramesSinceLastCameraUpdate() >= 3 ? "menu" : "in-game");
    VRManager::instance().XR->UpdateActions(m_frameState.predictedDisplayTime, VRManager::instance().Hooks->GetFramesSinceLastCameraUpdate() >= 3);
    // todo: update this as late as possible
    // currently we only support non-AER presenting, aka we render two textures with the same pose and then we present them
    // m_layer3D->UpdatePoses(m_frameState.predictedDisplayTime);
}


void RND_Renderer::EndFrame() {
    Log::print("RND_Renderer::EndFrame");

    std::vector<XrCompositionLayerBaseHeader*> compositionLayers;

    // todo: currently ignores m_frameState.shouldRender, but that's probably fine
    XrCompositionLayerQuad layer2D = { XR_TYPE_COMPOSITION_LAYER_QUAD };
    m_presented2DLastFrame = m_layer2D && m_layer2D->HasCopied();
    // checkAssert(m_presented2DLastFrame, "2D layer should always be rendered!");
    if (m_presented2DLastFrame) {
        Log::print("Presenting 2D layer");
        // The HUD/menus aren't eye-specific, so present the most recent one for both eyes at once
        m_layer2D->StartRendering();
        m_layer2D->Render();
        layer2D = m_layer2D->FinishRendering(m_frameState.predictedDisplayTime);
        compositionLayers.emplace_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer2D));
    }

    m_presented3DLastFrame = false;
    XrCompositionLayerProjection layer3D = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    std::array<XrCompositionLayerProjectionView, 2> layer3DViews = {};
    // checkAssert( m_layer3D->HasCopied(OpenXR::EyeSide::LEFT) == m_layer3D->HasCopied(OpenXR::EyeSide::RIGHT), "3D layer should always be rendered for both eyes at once!");
    if (m_layer3D && m_layer3D->HasCopied(OpenXR::EyeSide::LEFT) && m_layer3D->HasCopied(OpenXR::EyeSide::RIGHT)) {
        Log::print("Presenting 3D layer");
        m_layer3D->StartRendering();
        m_layer3D->Render(OpenXR::EyeSide::LEFT);
        m_layer3D->Render(OpenXR::EyeSide::RIGHT);
        layer3DViews = m_layer3D->FinishRendering();
        layer3D.layerFlags = NULL;
        layer3D.space = VRManager::instance().XR->m_stageSpace;
        layer3D.viewCount = (uint32_t)layer3DViews.size();
        layer3D.views = layer3DViews.data();
        if (CemuHooks::GetFramesSinceLastCameraUpdate() <= 2) {
            m_presented3DLastFrame = true;
            compositionLayers.emplace_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer3D));
        }
    }

    XrFrameEndInfo frameEndInfo = { XR_TYPE_FRAME_END_INFO };
    frameEndInfo.displayTime = m_frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    frameEndInfo.layerCount = (uint32_t)compositionLayers.size();
    frameEndInfo.layers = compositionLayers.data();
    checkXRResult(xrEndFrame(m_session, &frameEndInfo), "Failed to render texture!");

    VRManager::instance().D3D12->EndFrame();
}

RND_Renderer::Layer3D::Layer3D(VkExtent2D extent) {
    auto viewConfs = VRManager::instance().XR->GetViewConfigurations();

    this->m_presentPipelines[OpenXR::EyeSide::LEFT] = std::make_unique<RND_D3D12::PresentPipeline<true>>(VRManager::instance().XR->GetRenderer());
    this->m_presentPipelines[OpenXR::EyeSide::RIGHT] = std::make_unique<RND_D3D12::PresentPipeline<true>>(VRManager::instance().XR->GetRenderer());

    // note: it's possible to make a swapchain that matches Cemu's internal resolution and let the headset downsample it, although I doubt there's a benefit
    this->m_swapchains[OpenXR::EyeSide::LEFT] = std::make_unique<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>>(viewConfs[0].recommendedImageRectWidth, viewConfs[0].recommendedImageRectHeight, viewConfs[0].recommendedSwapchainSampleCount);
    this->m_swapchains[OpenXR::EyeSide::RIGHT] = std::make_unique<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>>(viewConfs[1].recommendedImageRectWidth, viewConfs[1].recommendedImageRectHeight, viewConfs[1].recommendedSwapchainSampleCount);
    this->m_depthSwapchains[OpenXR::EyeSide::LEFT] = std::make_unique<Swapchain<DXGI_FORMAT_D32_FLOAT>>(viewConfs[0].recommendedImageRectWidth, viewConfs[0].recommendedImageRectHeight, viewConfs[0].recommendedSwapchainSampleCount);
    this->m_depthSwapchains[OpenXR::EyeSide::RIGHT] = std::make_unique<Swapchain<DXGI_FORMAT_D32_FLOAT>>(viewConfs[1].recommendedImageRectWidth, viewConfs[1].recommendedImageRectHeight, viewConfs[1].recommendedSwapchainSampleCount);

    this->m_presentPipelines[OpenXR::EyeSide::LEFT]->BindSettings((float)this->m_swapchains[OpenXR::EyeSide::LEFT]->GetWidth(), (float)this->m_swapchains[OpenXR::EyeSide::LEFT]->GetHeight());
    this->m_presentPipelines[OpenXR::EyeSide::RIGHT]->BindSettings((float)this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetWidth(), (float)this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetHeight());

    // initialize textures
    this->m_textures[OpenXR::EyeSide::LEFT] = std::make_unique<SharedTexture>(extent.width, extent.height, VK_FORMAT_B10G11R11_UFLOAT_PACK32, D3D12Utils::ToDXGIFormat(VK_FORMAT_B10G11R11_UFLOAT_PACK32));
    this->m_textures[OpenXR::EyeSide::RIGHT] = std::make_unique<SharedTexture>(extent.width, extent.height, VK_FORMAT_B10G11R11_UFLOAT_PACK32, D3D12Utils::ToDXGIFormat(VK_FORMAT_B10G11R11_UFLOAT_PACK32));
    this->m_depthTextures[OpenXR::EyeSide::LEFT] = std::make_unique<SharedTexture>(extent.width, extent.height, VK_FORMAT_D32_SFLOAT, D3D12Utils::ToDXGIFormat(VK_FORMAT_D32_SFLOAT));
    this->m_depthTextures[OpenXR::EyeSide::RIGHT] = std::make_unique<SharedTexture>(extent.width, extent.height, VK_FORMAT_D32_SFLOAT, D3D12Utils::ToDXGIFormat(VK_FORMAT_D32_SFLOAT));

    this->m_textures[OpenXR::EyeSide::LEFT]->d3d12GetTexture()->SetName(L"Layer3D - Left Color Texture");
    this->m_textures[OpenXR::EyeSide::RIGHT]->d3d12GetTexture()->SetName(L"Layer3D - Right Color Texture");
    this->m_depthTextures[OpenXR::EyeSide::LEFT]->d3d12GetTexture()->SetName(L"Layer3D - Left Depth Texture");
    this->m_depthTextures[OpenXR::EyeSide::RIGHT]->d3d12GetTexture()->SetName(L"Layer3D - Right Depth Texture");

    ComPtr<ID3D12CommandAllocator> cmdAllocator;
    {
        ID3D12Device* d3d12Device = VRManager::instance().D3D12->GetDevice();
        ID3D12CommandQueue* d3d12Queue = VRManager::instance().D3D12->GetCommandQueue();
        d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator));

        RND_D3D12::CommandContext<true> transitionInitialTextures(d3d12Device, d3d12Queue, cmdAllocator.Get(), [this](RND_D3D12::CommandContext<true>* context) {
            context->GetRecordList()->SetName(L"transitionInitialTextures");
            this->m_textures[OpenXR::EyeSide::LEFT]->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_COPY_DEST);
            this->m_textures[OpenXR::EyeSide::RIGHT]->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_COPY_DEST);
            this->m_depthTextures[OpenXR::EyeSide::LEFT]->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_COPY_DEST);
            this->m_depthTextures[OpenXR::EyeSide::RIGHT]->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_COPY_DEST);
            context->Signal(this->m_textures[OpenXR::EyeSide::LEFT].get(), 0);
            context->Signal(this->m_textures[OpenXR::EyeSide::RIGHT].get(), 0);
            context->Signal(this->m_depthTextures[OpenXR::EyeSide::LEFT].get(), 0);
            context->Signal(this->m_depthTextures[OpenXR::EyeSide::RIGHT].get(), 0);
        });
    }
}

RND_Renderer::Layer3D::~Layer3D() {
    for (auto& swapchain : m_swapchains) {
        swapchain.reset();
    }
}

SharedTexture* RND_Renderer::Layer3D::CopyColorToLayer(OpenXR::EyeSide side, VkCommandBuffer copyCmdBuffer, VkImage image) {
    // Log::print("[VULKAN] Copying COLOR for {} side", side == OpenXR::EyeSide::LEFT ? "left" : "right");
    m_textures[side]->CopyFromVkImage(copyCmdBuffer, image);
    m_copiedColor[side] = true;
    return m_textures[side].get();
}

SharedTexture* RND_Renderer::Layer3D::CopyDepthToLayer(OpenXR::EyeSide side, VkCommandBuffer copyCmdBuffer, VkImage image) {
    m_depthTextures[side]->CopyFromVkImage(copyCmdBuffer, image);
    m_copiedDepth[side] = true;
    return m_depthTextures[side].get();
}

void RND_Renderer::Layer3D::PrepareRendering(OpenXR::EyeSide side) {
    // Log::print("Preparing rendering for {} side", side == OpenXR::EyeSide::LEFT ? "left" : "right");
    m_swapchains[side]->PrepareRendering();
    m_depthSwapchains[side]->PrepareRendering();
}

std::optional<std::array<XrView, 2>> RND_Renderer::UpdateViews(XrTime predictedDisplayTime) {
    std::array newViews = { XrView{ XR_TYPE_VIEW }, XrView{ XR_TYPE_VIEW } };
    XrViewLocateInfo viewLocateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = predictedDisplayTime;
    viewLocateInfo.space = VRManager::instance().XR->m_stageSpace; // locate the rendering views relative to the room, not the headset center
    XrViewState viewState = { XR_TYPE_VIEW_STATE };
    uint32_t viewCount = (uint32_t)newViews.size();
    checkXRResult(xrLocateViews(VRManager::instance().XR->m_session, &viewLocateInfo, &viewState, viewCount, &viewCount, newViews.data()), "Failed to get view information!");
    if ((viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0)
        return std::nullopt; // what should occur when the orientation is invalid? keep rendering using old values?

    m_currViews = newViews;
    return m_currViews;
}

void RND_Renderer::Layer3D::StartRendering() {
    // checkAssert((this->m_textures[OpenXR::EyeSide::LEFT] == nullptr && this->m_textures[OpenXR::EyeSide::RIGHT] == nullptr) || (this->m_textures[OpenXR::EyeSide::LEFT] != nullptr && this->m_textures[OpenXR::EyeSide::RIGHT] != nullptr), "Both textures must be either null or not null");
    // checkAssert((this->m_depthTextures[OpenXR::EyeSide::LEFT] == nullptr && this->m_depthTextures[OpenXR::EyeSide::RIGHT] == nullptr) || (this->m_depthTextures[OpenXR::EyeSide::LEFT] != nullptr && this->m_depthTextures[OpenXR::EyeSide::RIGHT] != nullptr), "Both depth textures must be either null or not null");

    this->m_swapchains[OpenXR::EyeSide::LEFT]->StartRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->StartRendering();
    this->m_swapchains[OpenXR::EyeSide::RIGHT]->StartRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->StartRendering();
}

void RND_Renderer::Layer3D::Render(OpenXR::EyeSide side) {
    ID3D12Device* device = VRManager::instance().D3D12->GetDevice();
    ID3D12CommandQueue* queue = VRManager::instance().D3D12->GetCommandQueue();
    ID3D12CommandAllocator* allocator = VRManager::instance().D3D12->GetFrameAllocator();

    RND_D3D12::CommandContext<false> renderSharedTexture(device, queue, allocator, [this, side](RND_D3D12::CommandContext<false>* context) {
        context->GetRecordList()->SetName(L"RenderSharedTexture");
        Log::print("[D3D12] Waiting for 3D layer's {} side to be 1", side == OpenXR::EyeSide::LEFT ? "left" : "right");
        context->WaitFor(m_textures[side].get(), SEMAPHORE_TO_D3D12);
        context->WaitFor(m_depthTextures[side].get(), SEMAPHORE_TO_D3D12);
        m_textures[side]->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_depthTextures[side]->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        m_presentPipelines[side]->BindAttachment(0, m_textures[side]->d3d12GetTexture());
        m_presentPipelines[side]->BindAttachment(1, m_depthTextures[side]->d3d12GetTexture(), DXGI_FORMAT_R32_FLOAT);
        m_presentPipelines[side]->BindTarget(0, m_swapchains[side]->GetTexture(), m_swapchains[side]->GetFormat());
        m_presentPipelines[side]->BindDepthTarget(m_depthSwapchains[side]->GetTexture(), m_depthSwapchains[side]->GetFormat());
        m_presentPipelines[side]->Render(context->GetRecordList(), m_swapchains[side]->GetTexture());

        m_textures[side]->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_COPY_DEST);
        m_depthTextures[side]->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_COPY_DEST);
        context->Signal(m_textures[side].get(), SEMAPHORE_TO_VULKAN);
        context->Signal(m_depthTextures[side].get(), SEMAPHORE_TO_VULKAN);
        Log::print("[D3D12 - 3D Layer] Signalling for 3D layer's {} side to be 0", side == OpenXR::EyeSide::LEFT ? "left" : "right");
    });
    // Log::print("[D3D12 - 3D Layer] Rendering finished");
}

const std::array<XrCompositionLayerProjectionView, 2>& RND_Renderer::Layer3D::FinishRendering() {
    this->m_swapchains[OpenXR::EyeSide::LEFT]->FinishRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->FinishRendering();
    this->m_swapchains[OpenXR::EyeSide::RIGHT]->FinishRendering();
    this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->FinishRendering();

    this->m_copiedColor[OpenXR::EyeSide::LEFT] = false;
    this->m_copiedColor[OpenXR::EyeSide::RIGHT] = false;
    this->m_copiedDepth[OpenXR::EyeSide::LEFT] = false;
    this->m_copiedDepth[OpenXR::EyeSide::RIGHT] = false;

    // clang-format off
    m_projectionViews[OpenXR::EyeSide::LEFT] = {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
        .next = &m_projectionViewsDepthInfo[OpenXR::EyeSide::LEFT],
        .pose = VRManager::instance().XR->GetRenderer()->GetPose(OpenXR::EyeSide::LEFT).value(),
        .fov = VRManager::instance().XR->GetRenderer()->GetFOV(OpenXR::EyeSide::LEFT).value(),
        .subImage = {
            .swapchain = this->m_swapchains[OpenXR::EyeSide::LEFT]->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_swapchains[OpenXR::EyeSide::LEFT]->GetWidth(),
                    .height = (int32_t)this->m_swapchains[OpenXR::EyeSide::LEFT]->GetHeight()
                }
            }
        }
    };
    m_projectionViewsDepthInfo[OpenXR::EyeSide::LEFT] = {
        .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
        .subImage = {
            .swapchain = this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->GetWidth(),
                    .height = (int32_t)this->m_depthSwapchains[OpenXR::EyeSide::LEFT]->GetHeight()
                }
            },
        },
        .minDepth = -1.0f,
        .maxDepth = 1.0f,
        .nearZ = 1.0f,
        .farZ = 25000.0f,
    };
    m_projectionViews[OpenXR::EyeSide::RIGHT] = {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
        .next = &m_projectionViewsDepthInfo[OpenXR::EyeSide::RIGHT],
        .pose = VRManager::instance().XR->GetRenderer()->GetPose(OpenXR::EyeSide::RIGHT).value(),
        .fov = VRManager::instance().XR->GetRenderer()->GetFOV(OpenXR::EyeSide::RIGHT).value(),
        .subImage = {
            .swapchain = this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetWidth(),
                    .height = (int32_t)this->m_swapchains[OpenXR::EyeSide::RIGHT]->GetHeight()
                }
            }
        }
    };
    m_projectionViewsDepthInfo[OpenXR::EyeSide::RIGHT] = {
        .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
        .subImage = {
            .swapchain = this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->GetWidth(),
                    .height = (int32_t)this->m_depthSwapchains[OpenXR::EyeSide::RIGHT]->GetHeight()
                }
            },
        },
        .minDepth = -1.0f,
        .maxDepth = 1.0f,
        .nearZ = 1.0f,
        .farZ = 25000.0f,
    };
    // clang-format on
    return m_projectionViews;
}


RND_Renderer::Layer2D::Layer2D(VkExtent2D extent) {
    auto viewConfs = VRManager::instance().XR->GetViewConfigurations();

    this->m_presentPipeline = std::make_unique<RND_D3D12::PresentPipeline<false>>(VRManager::instance().XR->GetRenderer());

    // note: it's possible to make a swapchain that matches Cemu's internal resolution and let the headset downsample it, although I doubt there's a benefit
    this->m_swapchain = std::make_unique<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>>(viewConfs[0].recommendedImageRectWidth, viewConfs[0].recommendedImageRectHeight, viewConfs[0].recommendedSwapchainSampleCount);

    this->m_presentPipeline->BindSettings((float)this->m_swapchain->GetWidth(), (float)this->m_swapchain->GetHeight());

    // initialize textures
    this->m_texture = std::make_unique<SharedTexture>(extent.width, extent.height, VK_FORMAT_A2B10G10R10_UNORM_PACK32, D3D12Utils::ToDXGIFormat(VK_FORMAT_A2B10G10R10_UNORM_PACK32));
    this->m_texture->d3d12GetTexture()->SetName(L"Layer2D - Color Texture");

    ComPtr<ID3D12CommandAllocator> cmdAllocator;
    {
        ID3D12Device* d3d12Device = VRManager::instance().D3D12->GetDevice();
        ID3D12CommandQueue* d3d12Queue = VRManager::instance().D3D12->GetCommandQueue();
        d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator));

        RND_D3D12::CommandContext<true> transitionInitialTextures(d3d12Device, d3d12Queue, cmdAllocator.Get(), [this](RND_D3D12::CommandContext<true>* context) {
            context->GetRecordList()->SetName(L"transitionInitialTextures");
            this->m_texture->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_COPY_DEST);
            context->Signal(this->m_texture.get(), 0);
        });
    }
}

RND_Renderer::Layer2D::~Layer2D() {
    m_swapchain.reset();
}

SharedTexture* RND_Renderer::Layer2D::CopyColorToLayer(VkCommandBuffer copyCmdBuffer, VkImage image) {
    m_swapchain->PrepareRendering();
    m_texture->CopyFromVkImage(copyCmdBuffer, image);
    m_copiedColor = true;
    return m_texture.get();
}

void RND_Renderer::Layer2D::StartRendering() {
    m_swapchain->StartRendering();
}

void RND_Renderer::Layer2D::Render() {
    ID3D12Device* device = VRManager::instance().D3D12->GetDevice();
    ID3D12CommandQueue* queue = VRManager::instance().D3D12->GetCommandQueue();
    ID3D12CommandAllocator* allocator = VRManager::instance().D3D12->GetFrameAllocator();

    RND_D3D12::CommandContext<false> renderSharedTexture(device, queue, allocator, [this](RND_D3D12::CommandContext<false>* context) {
        context->GetRecordList()->SetName(L"RenderSharedTexture");

        // wait for both since we only have one 2D swap buffer to render to
        // fixme: Why do we signal to the global command list instead of the local one?!
        Log::print("[D3D12 - 2D Layer] Waiting for 2D layer's texture to be 1");
        context->WaitFor(m_texture.get(), SEMAPHORE_TO_D3D12);
        m_texture->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        m_presentPipeline->BindAttachment(0, m_texture->d3d12GetTexture());
        m_presentPipeline->BindTarget(0, m_swapchain->GetTexture(), m_swapchain->GetFormat());
        m_presentPipeline->Render(context->GetRecordList(), m_swapchain->GetTexture());

        m_texture->d3d12TransitionLayout(context->GetRecordList(), D3D12_RESOURCE_STATE_COPY_DEST);
        context->Signal(m_texture.get(), SEMAPHORE_TO_VULKAN);
        Log::print("[D3D12 - 2D Layer] Signalling for 2D layer's texture to be 0");
    });
    m_copiedColor = false;
    Log::print("[D3D12 - 2D Layer] Rendering finished");
}

XrCompositionLayerQuad RND_Renderer::Layer2D::FinishRendering(XrTime predictedDisplayTime) {
    this->m_swapchain->FinishRendering();

    XrSpaceLocation spaceLocation = { XR_TYPE_SPACE_LOCATION };
    xrLocateSpace(VRManager::instance().XR->m_headSpace, VRManager::instance().XR->m_stageSpace, predictedDisplayTime, &spaceLocation);
    glm::quat headOrientation = glm::quat(spaceLocation.pose.orientation.w, spaceLocation.pose.orientation.x, spaceLocation.pose.orientation.y, spaceLocation.pose.orientation.z);
    glm::vec3 headPosition = glm::vec3(spaceLocation.pose.position.x, spaceLocation.pose.position.y, spaceLocation.pose.position.z);

    if (CemuHooks::GetSettings().UIFollowsLookingDirection()) {
        m_currentOrientation = glm::slerp(m_currentOrientation, headOrientation, LERP_SPEED);
        glm::vec3 forwardDirection = headOrientation * glm::vec3(0.0f, 0.0f, -1.0f);

        // calculate new position forwards
        glm::vec3 targetPosition = headPosition + (DISTANCE * forwardDirection);

        // calculate rightward direction
        glm::vec3 rightDirection = glm::normalize(glm::cross(forwardDirection, glm::vec3(0.0f, 1.0f, 0.0f)));

        // recalculate up direction using right and forward direction
        glm::vec3 upDirection = glm::cross(rightDirection, forwardDirection);
        glm::quat userFacingOrientation = glm::quatLookAt(forwardDirection, upDirection);

        spaceLocation.pose.orientation = { userFacingOrientation.x, userFacingOrientation.y, userFacingOrientation.z, userFacingOrientation.w };
        spaceLocation.pose.position = { targetPosition.x, targetPosition.y, targetPosition.z };
    }
    else {
        spaceLocation.pose.position.z -= DISTANCE;
        spaceLocation.pose.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
    }

    const float aspectRatio = (float)this->m_texture->d3d12GetTexture()->GetDesc().Width / (float)this->m_texture->d3d12GetTexture()->GetDesc().Height;

    const float width = aspectRatio > 1.0f ? aspectRatio : 1.0f;
    const float height = aspectRatio <= 1.0f ? 1.0f / aspectRatio : 1.0f;

    // todo: change space to head space if we want to follow the head
    constexpr float MENU_SIZE = 1.0f;
    // clang-format off
    return {
        .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
        .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
        .space = VRManager::instance().XR->m_stageSpace,
        .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
        .subImage = {
            .swapchain = this->m_swapchain->GetHandle(),
            .imageRect = {
                .offset = { 0, 0 },
                .extent = {
                    .width = (int32_t)this->m_swapchain->GetWidth(),
                    .height = (int32_t)this->m_swapchain->GetHeight()
                }
            }
        },
        .pose = spaceLocation.pose,
        .size = { width * MENU_SIZE, height * MENU_SIZE }
    };
    // clang-format on
}