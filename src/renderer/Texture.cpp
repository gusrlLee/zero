#include "renderer/Texture.h"
#include "core/VulkanContext.h"
#include "core/Buffer.h"
#include "util/Log.h"

#include <tinygltf/stb_image.h>
#include <ktx.h>

#include <stdexcept>
#include <iostream>
#include <algorithm>

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

// 확장자로 로더 분기
void Texture::createTextureImage(const std::string& filepath) {
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == "ktx2" || ext == "ktx") {
        loadFromKtx2(filepath);
    }
    else {
        loadFromStb(filepath);
    }
}

// ------------------------------------------------------------
// stb_image 경로 (jpg/png 등, 압축 안 된 RGBA, mip 없음)
// ------------------------------------------------------------
void Texture::loadFromStb(const std::string& filepath) {
    int texWidth, texHeight, texChannels;

    // STBI_rgb_alpha를 통해 강제로 4채널 RGBA로 변환
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("[Texture] Failed to load texture image: " + filepath);
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;

    m_format = VK_FORMAT_R8G8B8A8_SRGB; // PBR 베이스 컬러는 SRGB가 표준
    m_mipLevels = 1;

    VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset = {0, 0, 0},
        .imageExtent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 }
    };

    uploadPixels(static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), pixels, imageSize, { region });

    stbi_image_free(pixels);
    std::cout << "[Texture] Loaded (stb): " << filepath << " (" << texWidth << "x" << texHeight << ")\n";
}

// ------------------------------------------------------------
// KTX2 / Basis Universal 경로 (ETC1S/UASTC를 GPU 포맷으로 트랜스코딩, mip 포함)
// ------------------------------------------------------------
void Texture::loadFromKtx2(const std::string& filepath) {
    ktxTexture2* kTex = nullptr;
    KTX_error_code result = ktxTexture2_CreateFromNamedFile(
        filepath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTex);
    if (result != KTX_SUCCESS) {
        throw std::runtime_error("[Texture] Failed to load KTX2: " + filepath +
            " (code " + std::to_string(result) + ")");
    }

    // Basis Universal(ETC1S/UASTC)로 슈퍼압축된 경우 GPU 포맷으로 트랜스코딩.
    // BC7은 데스크톱 GPU에서 사실상 항상 지원되므로 기본 타깃으로 사용.
    if (ktxTexture2_NeedsTranscoding(kTex)) {
        result = ktxTexture2_TranscodeBasis(kTex, KTX_TTF_BC7_RGBA, 0);
        if (result != KTX_SUCCESS) {
            ktxTexture_Destroy(ktxTexture(kTex));
            throw std::runtime_error("[Texture] Failed to transcode KTX2 to BC7: " + filepath);
        }
    }

    // 트랜스코딩 후 실제 VkFormat이 vkFormat 필드에 채워짐 (sRGB 여부 포함).
    m_format = static_cast<VkFormat>(kTex->vkFormat);
    m_mipLevels = kTex->numLevels;

    ktx_uint8_t* data = ktxTexture_GetData(ktxTexture(kTex));
    ktx_size_t dataSize = ktxTexture_GetDataSize(ktxTexture(kTex));

    // mip 레벨별 복사 영역 구성 (레벨별 오프셋은 라이브러리가 관리)
    std::vector<VkBufferImageCopy> regions;
    regions.reserve(m_mipLevels);
    for (uint32_t level = 0; level < m_mipLevels; ++level) {
        ktx_size_t offset = 0;
        if (ktxTexture_GetImageOffset(ktxTexture(kTex), level, 0, 0, &offset) != KTX_SUCCESS) {
            ktxTexture_Destroy(ktxTexture(kTex));
            throw std::runtime_error("[Texture] ktxTexture_GetImageOffset failed: " + filepath);
        }

        uint32_t mipWidth = std::max(1u, kTex->baseWidth >> level);
        uint32_t mipHeight = std::max(1u, kTex->baseHeight >> level);

        regions.push_back(VkBufferImageCopy{
            .bufferOffset = static_cast<VkDeviceSize>(offset),
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 },
            .imageOffset = {0, 0, 0},
            .imageExtent = { mipWidth, mipHeight, 1 }
        });
    }

    uploadPixels(kTex->baseWidth, kTex->baseHeight, data, dataSize, regions);

    std::cout << "[Texture] Loaded (ktx2): " << filepath << " (" << kTex->baseWidth << "x"
        << kTex->baseHeight << ", " << m_mipLevels << " mips, fmt " << m_format << ")\n";

    ktxTexture_Destroy(ktxTexture(kTex));
}

// ------------------------------------------------------------
// 공통 업로드: 스테이징 버퍼 → VkImage (모든 mip 레벨을 한 번에 복사)
// ------------------------------------------------------------
void Texture::uploadPixels(uint32_t width, uint32_t height, const void* data, size_t dataSize,
                           const std::vector<VkBufferImageCopy>& regions) {
    // 1. 스테이징 버퍼 생성 및 CPU 데이터 복사
    Buffer stagingBuffer(
        m_context,
        dataSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );
    stagingBuffer.uploadData(data, dataSize);

    // 2. VRAM VkImage 생성 (m_format, m_mipLevels 반영)
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = m_format,
        .extent = { width, height, 1 },
        .mipLevels = m_mipLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };
    CHK(vmaCreateImage(m_context->getAllocator(), &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr));

    VkCommandBuffer cmd = m_context->beginSingleTimeCommands();

    // (A) 전체 mip 레벨을 TRANSFER_DST로 전이
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, m_mipLevels, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // (B) mip 레벨별 복사
    vkCmdCopyBufferToImage(cmd, stagingBuffer.getHandle(), m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(regions.size()), regions.data());

    // (C) 셰이더 읽기 가능 상태로 최종 전이
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_context->endSingleTimeCommands(cmd);
}

void Texture::createImageView() {
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, m_mipLevels, 0, 1 }
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
        .maxLod = static_cast<float>(m_mipLevels), // 로드된 mip 전체를 사용
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    CHK(vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &m_sampler));
}
