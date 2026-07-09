#include "renderer/DescriptorManager.h"
#include "core/VulkanContext.h"
#include "renderer/Texture.h"
#include "util/Log.h"

#include <array>
#include <stdexcept>

DescriptorManager::DescriptorManager(VulkanContext* context) : m_context(context) {
    VkDevice device = m_context->getDevice();

    // 1. 풀(Pool) 생성
    std::array<VkDescriptorPoolSize, 2> poolSizes = { {
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 }
    } };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, // ★ Bindless 필수 플래그
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    CHK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool));

    // 2. 레이아웃(Layout) 생성 (배열 크기 1000)
    // 래스터(fragment)와 패스트레이서(compute) 모두에서 사용하므로 ALL 스테이지
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { {
        { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000, VK_SHADER_STAGE_ALL, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_SAMPLER, 1000, VK_SHADER_STAGE_ALL, nullptr }
    } };

    VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    std::array<VkDescriptorBindingFlags, 2> bindingFlags = { flags, flags };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindingFlags.size()),
        .pBindingFlags = bindingFlags.data()
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flagsInfo,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };
    CHK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_setLayout));

    // 3. 디스크립터 세트 할당
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_setLayout
    };
    CHK(vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet));
}

DescriptorManager::~DescriptorManager() {
    VkDevice device = m_context->getDevice();
    vkDestroyDescriptorSetLayout(device, m_setLayout, nullptr);
    vkDestroyDescriptorPool(device, m_pool, nullptr);
}

void DescriptorManager::updateTextures(const std::vector<std::unique_ptr<Texture>>& textures) {
    if (textures.empty()) return;

    std::vector<VkDescriptorImageInfo> imageInfos(textures.size());
    std::vector<VkDescriptorImageInfo> samplerInfos(textures.size());

    for (size_t i = 0; i < textures.size(); i++) {
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView = textures[i]->getImageView();

        samplerInfos[i].sampler = textures[i]->getSampler();
    }

    std::array<VkWriteDescriptorSet, 2> writes = { {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_descriptorSet,
            .dstBinding = 0, // globalTextures
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = imageInfos.data()
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_descriptorSet,
            .dstBinding = 1, // globalSamplers
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(samplerInfos.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = samplerInfos.data()
        }
    } };

    vkUpdateDescriptorSets(m_context->getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}