#ifndef __ZERO_BUFFER_HEADER__
#define __ZERO_BUFFER_HEADER__

#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>

class VulkanContext;

class Buffer {
public:
    // size: 버퍼 크기, usage: 버퍼 용도, vmaFlags: CPU 접근 권한 등
    Buffer(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaFlags);
    ~Buffer();

    VkBuffer getHandle() const { return m_buffer; }

    // ★ 셰이더로 넘겨줄 핵심 64비트 GPU 메모리 주소
    VkDeviceAddress getDeviceAddress() const { return m_deviceAddress; }

    void* getMappedData() const { return m_mappedData; }

    // CPU 데이터를 GPU 버퍼로 복사하는 헬퍼 함수
    void uploadData(const void* data, size_t size, size_t offset = 0);

private:
    VulkanContext* m_context;
    VkBuffer m_buffer{ VK_NULL_HANDLE };
    VmaAllocation m_allocation{ VK_NULL_HANDLE };
    VkDeviceAddress m_deviceAddress{ 0 };
    void* m_mappedData{ nullptr };
    VkDeviceSize m_size{ 0 };
};

#endif