#include "renderer/raster/PbrRenderer.h"
#include "core/VulkanContext.h"
#include "core/Swapchain.h"
#include "core/Buffer.h"
#include "renderer/ShaderCompiler.h"
#include "util/Log.h"

#include <array>
#include <vector>

PbrRenderer::PbrRenderer(VulkanContext* context, Swapchain* swapchain)
    : m_context(context), m_swapchain(swapchain) {
    m_shaderCompiler = std::make_unique<ShaderCompiler>(m_context);
}

PbrRenderer::~PbrRenderer() {
    VkDevice device = m_context->getDevice();
    if (m_pipeline) vkDestroyPipeline(device, m_pipeline, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
}

void PbrRenderer::init() {
    VkDevice device = m_context->getDevice();

    m_camera = std::make_unique<Camera>(45.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
    m_mesh = std::make_unique<Mesh>(m_context, "../assets/DamagedHelmet/glTF/DamagedHelmet.gltf"); 

    m_cameraBuffer = std::make_unique<Buffer>(
        m_context, sizeof(ShaderCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // 1. Slang 셰이더 컴파일 (동일한 파일에서 각각의 Entry Point 추출)
    VkShaderModule vertShader = m_shaderCompiler->compileToShaderModule("../shaders/raster/shader.slang", "vertexMain");
    VkShaderModule fragShader = m_shaderCompiler->compileToShaderModule("../shaders/raster/shader.slang", "fragmentMain");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = { {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 
            .stage = VK_SHADER_STAGE_VERTEX_BIT,   
            .module = vertShader, 
            .pName = "main" 
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT, 
            .module = fragShader, 
            .pName = "main" 
        }
    }};

    // 2. 파이프라인 레이아웃 (Push Constants 설정)
    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = sizeof(PushConstants) // 16 bytes
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0, // 아직 Descriptor Set 없음
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
    CHK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &m_pipelineLayout));

    // 3. 고정 기능 상태 (Fixed-function state) 세팅
    // 버텍스 입력 (Shader 안에서 하드코딩 하므로 입력 데이터가 없음)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{ .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };

    // Viewport / Scissor (Dynamic State로 설정하여 창 크기 변환 시 파이프라인 재생성 방지)
    std::vector<VkDynamicState> dynamicStates = { 
        VK_DYNAMIC_STATE_VIEWPORT, 
        VK_DYNAMIC_STATE_SCISSOR 
    };

    VkPipelineDynamicStateCreateInfo dynamicState{ 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, 
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), 
        .pDynamicStates = dynamicStates.data() 
    };

    VkPipelineViewportStateCreateInfo viewportState{ 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, 
        .viewportCount = 1, 
        .scissorCount = 1 
    };

    // 래스터라이저 (컬링, 폴리곤 모드)
    VkPipelineRasterizationStateCreateInfo rasterizer{ 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, 
        .polygonMode = VK_POLYGON_MODE_FILL, 
        .cullMode = VK_CULL_MODE_NONE, 
        .frontFace = VK_FRONT_FACE_CLOCKWISE, 
        .lineWidth = 1.0f 
    };

    VkPipelineMultisampleStateCreateInfo multisampling{ 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, 
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT 
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil{ 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, 
        .depthTestEnable = VK_TRUE, 
        .depthWriteEnable = VK_TRUE, 
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL 
    };

    // 컬러 블렌딩 (알파 블렌딩 끄기)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{ 
        .blendEnable = VK_FALSE, 
        .colorWriteMask = 0xF /* RGBA 모두 쓰기 */ 
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{ 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, 
        .attachmentCount = 1, 
        .pAttachments = &colorBlendAttachment 
    };

    // 4. Dynamic Rendering 정보 연결 (RenderPass 대신 사용)
    VkFormat colorFormat = m_swapchain->getImageFormat();
    VkFormat depthFormat = m_swapchain->getDepthFormat();

    VkPipelineRenderingCreateInfo renderingCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat,
        .stencilAttachmentFormat = depthFormat
    };

    // 5. 최종 그래픽스 파이프라인 생성
    VkGraphicsPipelineCreateInfo pipelineCI{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCI, // ★ 핵심: Dynamic Rendering 연결
        .stageCount = 2,
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = m_pipelineLayout
    };
    CHK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_pipeline));

    // 셰이더 모듈은 파이프라인 생성 후 삭제해도 됩니다.
    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    m_cameraBuffer = std::make_unique<Buffer>(
        m_context,
        sizeof(ShaderCameraData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );
}

void PbrRenderer::onResize(uint32_t width, uint32_t height) {
    // Dynamic Viewport/Scissor를 사용하므로 파이프라인 재생성이 필요 없습니다!
}

void PbrRenderer::recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkImage swapchainImage = m_swapchain->getImage(imageIndex);
    VkImageView swapchainImageView = m_swapchain->getImageView(imageIndex);
    VkImage depthImage = m_swapchain->getDepthImage();
    VkImageView depthImageView = m_swapchain->getDepthImageView();
    VkExtent2D extent = m_swapchain->getExtent();

    // 1. [Sync2 Barrier] Color & Depth 쓰기 가능 상태로 전이
    std::array<VkImageMemoryBarrier2, 2> barriers = { {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .image = swapchainImage,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .image = depthImage,
            // ★ 수정: DEPTH_BIT 뒤에 | VK_IMAGE_ASPECT_STENCIL_BIT 를 추가합니다!
            .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 }
        }
    }};

    VkDependencyInfo dependencyInfoToAttachment{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = barriers.data()
    };
    vkCmdPipelineBarrier2(cmd, &dependencyInfoToAttachment);

    // 2. [Dynamic Rendering] 시작
    VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchainImageView,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = { {0.05f, 0.05f, 0.05f, 1.0f} } }
    };

    VkRenderingAttachmentInfo depthAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depthImageView,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {.depthStencil = { 1.0f, 0 } }
    };

    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { {0, 0}, extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pDepthAttachment = &depthAttachment
    };

    // ----------------------------------------------------
    // ★ 실제 그리기 (Draw) 로직 시작
    // ----------------------------------------------------
    vkCmdBeginRendering(cmd, &renderingInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport{ 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{ {0, 0}, extent };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // 1. 카메라 업데이트 (살짝 위에서 아래로 내려다보도록 위치 조정)
    glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = m_camera->getProjectionMatrix(); // Camera 클래스에서 계산된 투영 행렬 사용

    ShaderCameraData camData{ proj * view };
    m_cameraBuffer->uploadData(&camData, sizeof(ShaderCameraData));

    PushConstants pc{
        .cameraAddress = m_cameraBuffer->getDeviceAddress(),
        .vertexBufferAddress = m_mesh->getVertexBufferAddress()
    };

    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pc);
    vkCmdBindIndexBuffer(cmd, m_mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
    for (const auto& subMesh : m_mesh->getSubMeshes()) {
        vkCmdDrawIndexed(cmd, subMesh.indexCount, 1, subMesh.firstIndex, subMesh.vertexOffset, 0);
    }

    vkCmdEndRendering(cmd);

    // 3. [Sync2 Barrier] Present용 상태로 전이
    VkImageMemoryBarrier2 barrierToPresent{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = swapchainImage,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VkDependencyInfo dependencyInfoToPresent{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrierToPresent
    };
    vkCmdPipelineBarrier2(cmd, &dependencyInfoToPresent);
}

void PbrRenderer::onUI() {}