#include <iostream>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <cstring> // for memcpy
 
uint32_t findMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties& memProperties,
                             uint32_t typeFilter,
                             vk::MemoryPropertyFlags properties);
 
int main() {
    // 인스턴스, 디바이스, 큐 설정 (이전 글과 동일한 로직)
    vk::ApplicationInfo appInfo("MyVulkanApp", 1, "MyEngine", 1, VK_API_VERSION_1_3);
    std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
    std::vector<const char*> extensions = { "VK_EXT_debug_utils" };
    vk::InstanceCreateInfo instanceInfo({}, &appInfo, (uint32_t)layers.size(), layers.data(), (uint32_t)extensions.size(), extensions.data());
    vk::UniqueInstance instance = vk::createInstanceUnique(instanceInfo);
 
    auto physicalDevices = instance->enumeratePhysicalDevices();
    if (physicalDevices.empty()) {
        std::cerr << "No Vulkan-supported GPU found!\n";
        return 1;
    }
 
    vk::PhysicalDevice chosenDevice = VK_NULL_HANDLE;
    uint32_t computeQueueFamilyIndex = UINT32_MAX;
 
    for (auto& pd : physicalDevices) {
        auto queueFamilies = pd.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) {
                chosenDevice = pd;
                computeQueueFamilyIndex = i;
                break;
            }
        }
        if (chosenDevice) break;
    }
 
    if (!chosenDevice) {
        std::cerr << "No suitable GPU with compute capability found!\n";
        return 1;
    }
 
    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo({}, computeQueueFamilyIndex, 1, &queuePriority);
    vk::DeviceCreateInfo deviceCreateInfo({}, 1, &queueCreateInfo);
    vk::UniqueDevice device = chosenDevice.createDeviceUnique(deviceCreateInfo);
 
    vk::PhysicalDeviceMemoryProperties memProperties = chosenDevice.getMemoryProperties();
 
    // 버퍼 생성
    vk::BufferCreateInfo bufferInfo({}, sizeof(float)*1024, vk::BufferUsageFlagBits::eStorageBuffer, vk::SharingMode::eExclusive);
    vk::UniqueBuffer buffer = device->createBufferUnique(bufferInfo);
 
    // 메모리 요구사항 조회
    vk::MemoryRequirements memReq = device->getBufferMemoryRequirements(*buffer);
 
    // Host Visible 메모리 타입 인덱스 찾기 (Host에서 데이터 쓰기 가능)
    uint32_t memTypeIndex = findMemoryTypeIndex(memProperties, memReq.memoryTypeBits, 
                                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
 
    vk::MemoryAllocateInfo allocInfo(memReq.size, memTypeIndex);
    vk::UniqueDeviceMemory bufferMemory = device->allocateMemoryUnique(allocInfo);
 
    // 버퍼에 메모리 바인딩
    device->bindBufferMemory(*buffer, *bufferMemory, 0);
 
    // Host에서 데이터 쓰기
    void* dataPtr = device->mapMemory(*bufferMemory, 0, memReq.size);
    float* floatData = reinterpret_cast<float*>(dataPtr);
    for (int i = 0; i < 1024; i++) {
        floatData[i] = static_cast<float>(i);
    }
    device->unmapMemory(*bufferMemory);
 
    // 다시 읽어와서 확인
    dataPtr = device->mapMemory(*bufferMemory, 0, memReq.size);
    floatData = reinterpret_cast<float*>(dataPtr);
    std::cout << "C[0] = " << floatData[0] << ", C[100] = " << floatData[100] << "\n";
    device->unmapMemory(*bufferMemory);
 
    std::cout << "Buffer created and data transferred using Vulkan-HPP!\n";
    return 0;
}
 
uint32_t findMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties& memProperties,
                             uint32_t typeFilter,
                             vk::MemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}