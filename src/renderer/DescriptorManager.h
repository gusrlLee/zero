#ifndef __ZERO_DESCRIPTOR_MANAGER_HEADER__
#define __ZERO_DESCRIPTOR_MANAGER_HEADER__

#include <volk/volk.h>
#include <vector>
#include <memory>

class VulkanContext;
class Texture;

class DescriptorManager {
public:
    DescriptorManager(VulkanContext* context);
    ~DescriptorManager();

    void updateTextures(const std::vector<std::unique_ptr<Texture>>& textures);

    VkDescriptorSetLayout getLayout() const { return m_setLayout; }
    VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }

private:
    VulkanContext* m_context;

    VkDescriptorPool m_pool{ VK_NULL_HANDLE };
    VkDescriptorSetLayout m_setLayout{ VK_NULL_HANDLE };
    VkDescriptorSet m_descriptorSet{ VK_NULL_HANDLE };
};

#endif