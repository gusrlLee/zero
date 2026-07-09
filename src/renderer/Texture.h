#ifndef __ZERO_TEXTURE_HEADER__
#define __ZERO_TEXTURE_HEADER__

#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <string>
#include <vector>
#include <memory>

class VulkanContext;

class Texture {
public:
    Texture(VulkanContext* context, const std::string& filepath);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }

private:
    // 확장자에 따라 로더 분기: .ktx2 → KTX2/Basis, 그 외 → stb_image
    void createTextureImage(const std::string& filepath);
    void loadFromStb(const std::string& filepath);   // jpg/png 등
    void loadFromKtx2(const std::string& filepath);  // .ktx2 (Basis Universal)

    // m_format / m_mipLevels를 미리 세팅한 뒤, 스테이징 버퍼의 데이터를 VkImage로 업로드.
    // regions는 mip 레벨별 복사 영역 목록.
    void uploadPixels(uint32_t width, uint32_t height, const void* data, size_t dataSize,
                      const std::vector<VkBufferImageCopy>& regions);

    void createImageView();
    void createSampler();

    VulkanContext* m_context;
    VkImage m_image{ VK_NULL_HANDLE };
    VmaAllocation m_allocation{ VK_NULL_HANDLE };
    VkImageView m_imageView{ VK_NULL_HANDLE };
    VkSampler m_sampler{ VK_NULL_HANDLE };

    VkFormat m_format{ VK_FORMAT_R8G8B8A8_SRGB };
    uint32_t m_mipLevels{ 1 };
};

#endif
