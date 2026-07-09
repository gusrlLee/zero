#ifndef __ZERO_ACCELERATION_STRUCTURE_HEADER__
#define __ZERO_ACCELERATION_STRUCTURE_HEADER__

#include <volk/volk.h>
#include <memory>

class VulkanContext;
class Mesh;
class Buffer;

// 하드웨어 레이트레이싱용 가속 구조: 메시로부터 BLAS를 만들고,
// 단일 인스턴스(항등 변환, 정점이 이미 월드공간)로 TLAS를 만든다.
class AccelerationStructure {
public:
    AccelerationStructure(VulkanContext* context, Mesh* mesh);
    ~AccelerationStructure();

    AccelerationStructure(const AccelerationStructure&) = delete;
    AccelerationStructure& operator=(const AccelerationStructure&) = delete;

    VkAccelerationStructureKHR getTLAS() const { return m_tlas; }

private:
    void buildBLAS(Mesh* mesh);
    void buildTLAS();
    std::unique_ptr<Buffer> createScratch(VkDeviceSize size, VkDeviceAddress& outAlignedAddress);

    VulkanContext* m_context;

    VkAccelerationStructureKHR m_blas{ VK_NULL_HANDLE };
    VkAccelerationStructureKHR m_tlas{ VK_NULL_HANDLE };
    VkDeviceAddress m_blasAddress{ 0 };

    std::unique_ptr<Buffer> m_blasBuffer;
    std::unique_ptr<Buffer> m_tlasBuffer;
    std::unique_ptr<Buffer> m_instanceBuffer;

    uint32_t m_scratchAlignment{ 256 };
};

#endif
