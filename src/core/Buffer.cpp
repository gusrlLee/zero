#include "core/Buffer.h"
#include "core/VulkanContext.h"
#include "util/Log.h"
#include <cstring>
#include <stdexcept>

Buffer::Buffer(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaFlags)
    : m_context(context), m_size(size) {

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = vmaFlags,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    VmaAllocationInfo allocResultInfo{};
    CHK(vmaCreateBuffer(m_context->getAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_allocation, &allocResultInfo));

    m_mappedData = allocResultInfo.pMappedData;

    VkBufferDeviceAddressInfo bdaInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = m_buffer
    };
    m_deviceAddress = vkGetBufferDeviceAddress(m_context->getDevice(), &bdaInfo);
}

Buffer::~Buffer() {
    if (m_buffer) {
        vmaDestroyBuffer(m_context->getAllocator(), m_buffer, m_allocation);
    }
}

void Buffer::uploadData(const void* data, size_t uploadSize, size_t offset) {
    if (!m_mappedData) {
        throw std::runtime_error("[Buffer] Cannot upload data to unmapped buffer!");
    }
    if (offset + uploadSize > m_size) {
        throw std::runtime_error("[Buffer] Upload size exceeds buffer capacity!");
    }
    memcpy(static_cast<char*>(m_mappedData) + offset, data, uploadSize);
}