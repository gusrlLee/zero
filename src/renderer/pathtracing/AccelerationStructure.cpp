#include "renderer/pathtracing/AccelerationStructure.h"
#include "core/VulkanContext.h"
#include "core/Buffer.h"
#include "scene/Mesh.h"
#include "util/Log.h"

AccelerationStructure::AccelerationStructure(VulkanContext* context, Mesh* mesh)
    : m_context(context) {
    // 스크래치 버퍼 정렬 요구사항 질의
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &asProps
    };
    vkGetPhysicalDeviceProperties2(context->getPhysicalDevice(), &props2);
    m_scratchAlignment = asProps.minAccelerationStructureScratchOffsetAlignment;

    buildBLAS(mesh);
    buildTLAS();
}

AccelerationStructure::~AccelerationStructure() {
    VkDevice device = m_context->getDevice();
    if (m_tlas) vkDestroyAccelerationStructureKHR(device, m_tlas, nullptr);
    if (m_blas) vkDestroyAccelerationStructureKHR(device, m_blas, nullptr);
}

std::unique_ptr<Buffer> AccelerationStructure::createScratch(VkDeviceSize size, VkDeviceAddress& outAlignedAddress) {
    // 정렬을 위해 여유분을 더해 할당한 뒤 주소를 올림 정렬
    auto scratch = std::make_unique<Buffer>(
        m_context, size + m_scratchAlignment,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);
    VkDeviceAddress base = scratch->getDeviceAddress();
    outAlignedAddress = (base + m_scratchAlignment - 1) & ~(VkDeviceAddress(m_scratchAlignment) - 1);
    return scratch;
}

void AccelerationStructure::buildBLAS(Mesh* mesh) {
    VkDevice device = m_context->getDevice();

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    geometry.geometry.triangles = VkAccelerationStructureGeometryTrianglesDataKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = mesh->getVertexBufferAddress() },
        .vertexStride = mesh->getVertexStride(),
        .maxVertex = mesh->getVertexCount() - 1,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = mesh->getIndexBufferAddress() }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    uint32_t triangleCount = mesh->getTriangleCount();
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &triangleCount, &sizeInfo);

    // BLAS를 담을 버퍼 + 객체 생성
    m_blasBuffer = std::make_unique<Buffer>(
        m_context, sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = m_blasBuffer->getHandle(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    CHK(vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &m_blas));

    VkDeviceAddress scratchAddr = 0;
    auto scratch = createScratch(sizeInfo.buildScratchSize, scratchAddr);

    buildInfo.dstAccelerationStructure = m_blas;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{ .primitiveCount = triangleCount };
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    VkCommandBuffer cmd = m_context->beginSingleTimeCommands();
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    m_context->endSingleTimeCommands(cmd); // 내부적으로 queueWaitIdle

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = m_blas
    };
    m_blasAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);
}

void AccelerationStructure::buildTLAS() {
    VkDevice device = m_context->getDevice();

    // 단일 인스턴스: 항등 변환 (정점이 이미 월드좌표)
    VkTransformMatrixKHR identity{ {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0}
    } };
    VkAccelerationStructureInstanceKHR instance{
        .transform = identity,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = m_blasAddress
    };

    m_instanceBuffer = std::make_unique<Buffer>(
        m_context, sizeof(instance),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_instanceBuffer->uploadData(&instance, sizeof(instance));

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    geometry.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = m_instanceBuffer->getDeviceAddress() }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    uint32_t instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instanceCount, &sizeInfo);

    m_tlasBuffer = std::make_unique<Buffer>(
        m_context, sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = m_tlasBuffer->getHandle(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    CHK(vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &m_tlas));

    VkDeviceAddress scratchAddr = 0;
    auto scratch = createScratch(sizeInfo.buildScratchSize, scratchAddr);

    buildInfo.dstAccelerationStructure = m_tlas;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{ .primitiveCount = 1 };
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    VkCommandBuffer cmd = m_context->beginSingleTimeCommands();
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    m_context->endSingleTimeCommands(cmd);
}
