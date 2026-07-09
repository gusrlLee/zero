#ifndef __ZERO_PATH_TRACER_HEADER__
#define __ZERO_PATH_TRACER_HEADER__

#include "renderer/IRenderer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <memory>

class VulkanContext;
class Swapchain;
class Camera;
class Mesh;
class ShaderCompiler;
class DescriptorManager;
class AccelerationStructure;

// pathtrace.slang의 push constant와 레이아웃 일치 (192바이트)
struct PTPushConstants {
    glm::mat4 invViewProj;      // 0
    glm::vec4 cameraPos;        // 64
    glm::vec4 sunDirection;     // 80  (빛 진행 방향)
    glm::vec4 sunColor;         // 96  rgb, w=intensity
    glm::vec4 ambient;          // 112 rgb, w=intensity (환경광)
    uint64_t vertexAddr;        // 128
    uint64_t indexAddr;         // 136
    uint64_t materialAddr;      // 144
    uint64_t triMaterialAddr;   // 152
    uint32_t frameIndex;        // 160
    uint32_t width;             // 164
    uint32_t height;            // 168
    uint32_t maxBounces;        // 172
    float sunAngularRadius;     // 176
    uint32_t _pad[3];           // 180
};

// 하드웨어 레이트레이싱(ray query) 기반 진행형 패스트레이서.
// compute 셰이더가 HDR 누적 이미지에 샘플을 쌓고, 풀스크린 톤맵 패스로 스왑체인에 출력.
class PathTracer : public IRenderer {
public:
    PathTracer(VulkanContext* context, Swapchain* swapchain, Mesh* mesh, Camera* camera);
    ~PathTracer() override;

    void init() override;
    void onResize(uint32_t width, uint32_t height) override;
    void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) override;
    void onUI() override;

    Camera* getCamera() const override { return m_camera; }

private:
    void createAccumImage(uint32_t width, uint32_t height);
    void destroyAccumImage();
    void createDescriptors();
    void updateDescriptors();
    void createComputePipeline();
    void createTonemapPipeline();

    VulkanContext* m_context;
    Swapchain* m_swapchain;
    Camera* m_camera; // non-owning
    Mesh* m_mesh;     // non-owning

    std::unique_ptr<ShaderCompiler> m_shaderCompiler;
    std::unique_ptr<DescriptorManager> m_descriptorManager;      // bindless 텍스처 (set 0)
    std::unique_ptr<AccelerationStructure> m_accel;

    // 누적 이미지 (RGBA32F)
    VkImage m_accumImage{ VK_NULL_HANDLE };
    VmaAllocation m_accumAllocation{ VK_NULL_HANDLE };
    VkImageView m_accumView{ VK_NULL_HANDLE };
    VkImageLayout m_accumLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
    uint32_t m_width{ 0 }, m_height{ 0 };

    // compute (set 1): TLAS + 누적 storage image
    VkDescriptorPool m_computePool{ VK_NULL_HANDLE };
    VkDescriptorSetLayout m_computeSetLayout{ VK_NULL_HANDLE };
    VkDescriptorSet m_computeSet{ VK_NULL_HANDLE };
    VkPipelineLayout m_computePipelineLayout{ VK_NULL_HANDLE };
    VkPipeline m_computePipeline{ VK_NULL_HANDLE };

    // 톤맵 (풀스크린): 누적 이미지 샘플 -> 스왑체인
    VkDescriptorPool m_tonemapPool{ VK_NULL_HANDLE };
    VkDescriptorSetLayout m_tonemapSetLayout{ VK_NULL_HANDLE };
    VkDescriptorSet m_tonemapSet{ VK_NULL_HANDLE };
    VkPipelineLayout m_tonemapPipelineLayout{ VK_NULL_HANDLE };
    VkPipeline m_tonemapPipeline{ VK_NULL_HANDLE };

    // 누적 상태
    uint32_t m_frameIndex{ 0 };
    glm::mat4 m_prevViewProj{ 1.0f };

    // 라이팅/PT 파라미터 (ImGui)
    glm::vec3 m_sunDirection{ -0.4f, -1.0f, -0.3f };
    glm::vec3 m_sunColor{ 1.0f, 0.98f, 0.92f };
    float m_sunIntensity{ 4.0f };
    glm::vec3 m_ambientColor{ 0.5f, 0.6f, 0.75f };
    float m_ambientIntensity{ 0.4f };
    int m_maxBounces{ 4 };
    float m_sunAngularRadiusDeg{ 1.5f };
};

#endif
