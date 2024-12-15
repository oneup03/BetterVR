#pragma once

class SharedTexture;

class BaseVulkanTexture {
    friend class SharedTexture;
    friend class VulkanTexture;
public:
    BaseVulkanTexture(uint32_t width, uint32_t height, VkFormat vkFormat): m_width(width), m_height(height), m_vkFormat(vkFormat) {}
    virtual ~BaseVulkanTexture();

    void vkPipelineBarrier(VkCommandBuffer cmdBuffer);
    void vkTransitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout);

    void vkClear(VkCommandBuffer cmdBuffer, VkClearColorValue color);
    void vkCopyToImage(VkCommandBuffer cmdBuffer, VkImage dstImage);
    void vkCopyFromImage(VkCommandBuffer cmdBuffer, VkImage srcImage);
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
    VkImage m_vkImage = VK_NULL_HANDLE;
    VkDeviceMemory m_vkMemory = VK_NULL_HANDLE;
    VkImageLayout m_vkCurrLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    uint32_t m_width;
    uint32_t m_height;
    VkFormat m_vkFormat;
};

class VulkanTexture : public BaseVulkanTexture {
    friend class VulkanFramebuffer;
public:
    VulkanTexture(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage);
    ~VulkanTexture() override;

    VkImageView GetImageView() const { return m_vkImageView; }

private:
    VkImageView m_vkImageView = VK_NULL_HANDLE;
};

class VulkanFramebuffer : public VulkanTexture {
public:
    VulkanFramebuffer(uint32_t width, uint32_t height, VkFormat format, VkRenderPass renderPass);
    ~VulkanFramebuffer() override;

    VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
private:
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
};

class Texture {
public:
    Texture(uint32_t width, uint32_t height, DXGI_FORMAT format);
    virtual ~Texture();

    void d3d12SignalFence(uint64_t value);
    void d3d12WaitForFence(uint64_t value);
    void d3d12TransitionLayout(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES state);
    bool CopyFromSharedTexture(ID3D12GraphicsCommandList* cmdList, SharedTexture* texture);

    ID3D12Resource* d3d12GetTexture() const { return m_d3d12Texture.Get(); }
    DXGI_FORMAT d3d12GetFormat() const { return m_d3d12Format; }

protected:
    DXGI_FORMAT m_d3d12Format;
    HANDLE m_d3d12TextureHandle = nullptr;
    ComPtr<ID3D12Resource> m_d3d12Texture;
    D3D12_RESOURCE_STATES m_currState = D3D12_RESOURCE_STATE_COMMON;

    HANDLE m_d3d12FenceHandle = nullptr;
    ComPtr<ID3D12Fence> m_d3d12Fence;
    uint64_t m_fenceValue = 0;
};

class SharedTexture : public Texture, public BaseVulkanTexture {
public:
    SharedTexture(uint32_t width, uint32_t height, VkFormat vkFormat, DXGI_FORMAT d3d12Format);
    ~SharedTexture() override;

    void CopyFromVkImage(VkCommandBuffer cmdBuffer, VkImage srcImage);
    const VkSemaphore& GetSemaphore() const { return m_vkSemaphore; }

private:
    VkSemaphore m_vkSemaphore = VK_NULL_HANDLE;
};