#ifndef __ZERO_SWAPCHAIN_HEADER__
#define __ZERO_SWAPCHAIN_HEADER__

#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <vector>

class VulkanContext;
class Window;

class Swapchain {
public:
    Swapchain(VulkanContext* context, Window* window);
    ~Swapchain();

    void recreate(uint32_t width, uint32_t height);

    // Getters
    VkSwapchainKHR getHandle() const { return m_swapchain; }
    VkFormat getImageFormat() const { return m_imageFormat; }
    VkExtent2D getExtent() const { return m_extent; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }

    VkImage getImage(uint32_t index) const { return m_images[index]; }
    VkImageView getImageView(uint32_t index) const { return m_imageViews[index]; }

    VkImage getDepthImage() const { return m_depthImage; }
    VkImageView getDepthImageView() const { return m_depthImageView; }
    VkFormat getDepthFormat() const { return m_depthFormat; }

private:
    void create(uint32_t width, uint32_t height);
    void cleanup();
    VkFormat findDepthFormat();
    void createDepthBuffer();

private:
    VulkanContext* m_context;
    Window* m_window;

    VkSwapchainKHR m_swapchain{ VK_NULL_HANDLE };
    VkFormat m_imageFormat;
    VkExtent2D m_extent;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    // Depth buffer resources
    VkFormat m_depthFormat;
    VkImage m_depthImage{ VK_NULL_HANDLE };
    VmaAllocation m_depthImageAllocation{ VK_NULL_HANDLE };
    VkImageView m_depthImageView{ VK_NULL_HANDLE };
};

#endif