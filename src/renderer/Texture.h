#ifndef __ZERO_TEXTURE_HEADER__
#define __ZERO_TEXTURE_HEADER__

#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <string>
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
    void createTextureImage(const std::string& filepath);
    void createImageView();
    void createSampler();

    VulkanContext* m_context;
    VkImage m_image{ VK_NULL_HANDLE };
    VmaAllocation m_allocation{ VK_NULL_HANDLE };
    VkImageView m_imageView{ VK_NULL_HANDLE };
    VkSampler m_sampler{ VK_NULL_HANDLE };
};

#endif