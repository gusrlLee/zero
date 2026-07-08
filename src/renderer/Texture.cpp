#include "renderer/Texture.h"
#include "core/VulkanContext.h"
#include "core/Buffer.h"
#include "util/Log.h"

#include <tinygltf/stb_image.h>

#include <stdexcept>
#include <iostream>

Texture::Texture(VulkanContext* context, const std::string& filepath)
    : m_context(context) {
    createTextureImage(filepath);
    createImageView();
    createSampler();
}

Texture::~Texture() {
    VkDevice device = m_context->getDevice();
    if (m_sampler) vkDestroySampler(device, m_sampler, nullptr);
    if (m_imageView) vkDestroyImageView(device, m_imageView, nullptr);
    if (m_image) vmaDestroyImage(m_context->getAllocator(), m_image, m_allocation);
}

void Texture::createTextureImage(const std::string& filepath) {
    int texWidth, texHeight, texChannels;

    // 1. 이미지 로드 (STBI_rgb_alpha를 통해 강제로 4채널 RGBA로 변환)
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("[Texture] Failed to load texture image: " + filepath);
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    // 2. 스테이징 버퍼 생성 및 CPU 데이터 복사
    Buffer stagingBuffer(
        m_context,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );
    stagingBuffer.uploadData(pixels, imageSize);

    // RAM에 있는 데이터는 지워도 됨
    stbi_image_free(pixels);

    // 3. VRAM에 진짜 텍스처(VkImage) 공간 생성
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB, // PBR 베이스 컬러는 SRGB가 표준
        .extent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };
    CHK(vmaCreateImage(m_context->getAllocator(), &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr));

    // 4. 스테이징 버퍼에서 VkImage로 복사
    VkCommandBuffer cmd = m_context->beginSingleTimeCommands();

    // (A) 이미지를 '복사 받기 좋은 상태(TRANSFER_DST)'로 레이아웃 변환
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // (B) 실제 복사 명령
    VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset = {0, 0, 0},
        .imageExtent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 }
    };
    vkCmdCopyBufferToImage(cmd, stagingBuffer.getHandle(), m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // (C) 이미지를 '셰이더가 읽기 좋은 상태(SHADER_READ_ONLY)'로 최종 변환
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_context->endSingleTimeCommands(cmd);

    std::cout << "[Texture] Loaded to VRAM: " << filepath << " (" << texWidth << "x" << texHeight << ")\n";
}

void Texture::createImageView() {
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    CHK(vkCreateImageView(m_context->getDevice(), &viewInfo, nullptr, &m_imageView));
}

void Texture::createSampler() {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_context->getPhysicalDevice(), &properties);

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy, // GPU가 지원하는 최대 이방성 필터링
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    CHK(vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &m_sampler));
}