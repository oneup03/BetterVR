#pragma once

#include "d3d12.h"
#include "openxr.h"

#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/gtc/quaternion.hpp>


class RND_Renderer {
public:
    explicit RND_Renderer(XrSession xrSession);
    ~RND_Renderer();

    void StartFrame();
    void EndFrame();

    class Layer3D {
    public:
        Layer3D();
        ~Layer3D();

        void InitTextures(VkExtent2D extent);

        enum class Status3D {
            UNINITIALIZED,
            NOT_RENDERING,
            LEFT_PREPARING,
            LEFT_BINDING_COLOR,
            LEFT_BINDING_DEPTH,
            // between these two Cemu will start a new frame, but this code prepares it for both eyes at once
            RIGHT_PREPARING,
            RIGHT_BINDING_COLOR,
            RIGHT_BINDING_DEPTH,
            RENDERING,
        };

        void PrepareRendering(OpenXR::EyeSide side);
        void UpdatePredictedTime(OpenXR::EyeSide side, XrTime time) { m_predictedTimes[side] = time; };
        void UpdatePoses(OpenXR::EyeSide side);
        class SharedTexture* CopyColorToLayer(VkCommandBuffer copyCmdBuffer, VkImage image);
        class SharedTexture* CopyDepthToLayer(VkCommandBuffer copyCmdBuffer, VkImage image);
        void StartRendering();
        void Render(OpenXR::EyeSide side);
        const std::array<XrCompositionLayerProjectionView, 2>& FinishRendering();

        [[nodiscard]] Status3D GetStatus() const { return m_status; }
        [[nodiscard]] XrFovf GetFOV(OpenXR::EyeSide side) const { return m_currViews[side].fov; }
        [[nodiscard]] XrPosef GetPose(OpenXR::EyeSide side) const { return m_currViews[side].pose; }
        [[nodiscard]] float GetAspectRatio(OpenXR::EyeSide side) const { return m_swapchains[side]->GetWidth() / (float)m_swapchains[side]->GetHeight(); }

    private:
        std::array<std::unique_ptr<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>>, 2> m_swapchains;
        std::array<std::unique_ptr<Swapchain<DXGI_FORMAT_D32_FLOAT>>, 2> m_depthSwapchains;
        std::array<std::unique_ptr<RND_D3D12::PresentPipeline<true>>, 2> m_presentPipelines;
        std::array<std::unique_ptr<SharedTexture>, 2> m_textures;
        std::array<std::unique_ptr<SharedTexture>, 2> m_depthTextures;
        std::array<XrCompositionLayerProjectionView, 2> m_projectionViews = {};
        std::array<XrCompositionLayerDepthInfoKHR, 2> m_projectionViewsDepthInfo = {};

        Status3D m_status = Status3D::UNINITIALIZED;
        std::array<XrTime, 2> m_predictedTimes = { 0, 0};
        std::array<XrView, 2> m_currViews = { XrView{ XR_TYPE_VIEW }, XrView{ XR_TYPE_VIEW } };
    };

    class Layer2D {
    public:
        Layer2D();
        ~Layer2D();

        enum class Status2D {
            UNINITIALIZED,
            NOT_RENDERING,
            PREPARING,
            BINDING_COLOR,
            RENDERING,
        };

        void InitTextures(VkExtent2D extent);

        void PrepareRendering();
        void UpdatePredictedTime(XrTime time) { m_predictedTime = time; };
        [[nodiscard]] class SharedTexture* CopyColorToLayer(VkCommandBuffer copyCmdBuffer, VkImage image);
        void StartRendering();
        void Render();
        XrCompositionLayerQuad FinishRendering();

        Status2D GetStatus() const { return m_status; }

    private:
        std::unique_ptr<Swapchain<DXGI_FORMAT_R8G8B8A8_UNORM_SRGB>> m_swapchain;
        std::unique_ptr<RND_D3D12::PresentPipeline<false>> m_presentPipeline;
        std::unique_ptr<SharedTexture> m_texture;

        Status2D m_status = Status2D::UNINITIALIZED;
        XrTime m_predictedTime = 0;

        static constexpr float DISTANCE = 2.0f;
        static constexpr float LERP_SPEED = 0.05f;
        glm::quat m_currentOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    };

    Layer3D m_layer3D;
    Layer2D m_layer2D;

protected:
    XrSession m_session;
    XrFrameState m_frameState = { XR_TYPE_FRAME_STATE };
};
