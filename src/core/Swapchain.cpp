#include "core/Swapchain.h"
#include "core/VulkanContext.h"
#include "core/Window.h"
#include "util/Log.h"

#include <array>
#include <algorithm>

Swapchain::Swapchain(VulkanContext* context, Window* window)
    : m_context(context), m_window(window)
{
    // 최적의 Depth 포맷을 미리 찾아둡니다.
    m_depthFormat = findDepthFormat();

    // 초기 해상도로 스왑체인 생성
    int width, height;
    SDL_GetWindowSize(m_window->getNativeHandle(), &width, &height);
    create(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

Swapchain::~Swapchain() {
    cleanup();
}

void Swapchain::cleanup() {
    VkDevice device = m_context->getDevice();
    VmaAllocator allocator = m_context->getAllocator();

    // 1. Depth 자원 해제
    if (m_depthImageView) {
        vkDestroyImageView(device, m_depthImageView, nullptr);
    }
    if (m_depthImage) {
        vmaDestroyImage(allocator, m_depthImage, m_depthImageAllocation);
    }

    // 2. 스왑체인 Image View 해제
    for (auto imageView : m_imageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();

    // 3. 스왑체인 본체 해제
    if (m_swapchain) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
    }
}

void Swapchain::recreate(uint32_t width, uint32_t height) {
    VkDevice device = m_context->getDevice();

    // GPU가 모든 작업을 마칠 때까지 대기 (자원 해제 전 필수)
    CHK(vkDeviceWaitIdle(device));

    // 기존 스왑체인 핸들을 백업하고 자원(뷰, 뎁스)만 지웁니다.
    VkSwapchainKHR oldSwapchain = m_swapchain;

    VmaAllocator allocator = m_context->getAllocator();
    vkDestroyImageView(device, m_depthImageView, nullptr);
    vmaDestroyImage(allocator, m_depthImage, m_depthImageAllocation);
    for (auto imageView : m_imageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();

    // oldSwapchain을 활용하여 새로 생성 (드라이버 레벨에서 메모리 재사용 최적화)
    create(width, height);

    // 새로운 스왑체인이 성공적으로 만들어지면, 이전 스왑체인 파괴
    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
    }
}

void Swapchain::create(uint32_t width, uint32_t height) {
    VkPhysicalDevice physicalDevice = m_context->getPhysicalDevice();
    VkDevice device = m_context->getDevice();
    VkSurfaceKHR surface = m_context->getSurface();

    // 1. Surface Capabilities 확인
    VkSurfaceCapabilitiesKHR surfaceCaps{};
    CHK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    // Extent(해상도) 결정
    if (surfaceCaps.currentExtent.width != 0xFFFFFFFF) {
        m_extent = surfaceCaps.currentExtent;
    }
    else {
        m_extent = { width, height };
        m_extent.width = std::clamp(m_extent.width, surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.width);
        m_extent.height = std::clamp(m_extent.height, surfaceCaps.minImageExtent.height, surfaceCaps.maxImageExtent.height);
    }

    // 2. 포맷 및 Present 모드 고정 (SRGB / FIFO)
    m_imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync On (가장 안정적)

    // 버퍼 개수 (Double 또는 Triple Buffering)
    uint32_t imageCount = surfaceCaps.minImageCount + 1;
    if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount) {
        imageCount = surfaceCaps.maxImageCount;
    }

    // 3. 스왑체인 생성 정보
    VkSwapchainCreateInfoKHR swapchainCI{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = m_imageFormat,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = m_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = surfaceCaps.currentTransform, // 보통 IDENTITY
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = m_swapchain // 재생성 시 여기에 기존 핸들이 들어감
    };

    // 큐 패밀리가 1개라고 가정 (Graphics와 Present가 같은 큐)
    swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    CHK(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &m_swapchain));

    // 4. 스왑체인 이미지 핸들 및 Image View 획득
    CHK(vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, nullptr));
    m_images.resize(imageCount);
    CHK(vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, m_images.data()));

    m_imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_imageFormat,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        CHK(vkCreateImageView(device, &viewCI, nullptr, &m_imageViews[i]));
    }

    // 5. Depth Buffer 생성
    createDepthBuffer();
}

void Swapchain::createDepthBuffer() {
    VkDevice device = m_context->getDevice();
    VmaAllocator allocator = m_context->getAllocator();

    VkImageCreateInfo depthImageCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = m_depthFormat,
        .extent = { m_extent.width, m_extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo allocCI{
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    CHK(vmaCreateImage(allocator, &depthImageCI, &allocCI, &m_depthImage, &m_depthImageAllocation, nullptr));

    VkImageViewCreateInfo depthViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_depthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_depthFormat,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    CHK(vkCreateImageView(device, &depthViewCI, nullptr, &m_depthImageView));
}

VkFormat Swapchain::findDepthFormat() {
    VkPhysicalDevice physicalDevice = m_context->getPhysicalDevice();

    // 선호하는 Depth 포맷 리스트
    std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT
    };

    for (VkFormat format : candidates) {
        VkFormatProperties2 formatProps{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
        vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProps);

        if (formatProps.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    throw std::runtime_error("[Swapchain] Failed to find a supported depth format!");
}