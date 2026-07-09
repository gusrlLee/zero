#include "renderer/pathtracing/PathTracer.h"
#include "renderer/pathtracing/AccelerationStructure.h"
#include "core/VulkanContext.h"
#include "core/Swapchain.h"
#include "scene/Camera.h"
#include "scene/Mesh.h"
#include "renderer/ShaderCompiler.h"
#include "renderer/DescriptorManager.h"
#include "util/Log.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <vector>

PathTracer::PathTracer(VulkanContext* context, Swapchain* swapchain, Mesh* mesh, Camera* camera)
    : m_context(context), m_swapchain(swapchain), m_camera(camera), m_mesh(mesh) {
    m_shaderCompiler = std::make_unique<ShaderCompiler>(m_context);
}

PathTracer::~PathTracer() {
    VkDevice device = m_context->getDevice();
    if (m_computePipeline) vkDestroyPipeline(device, m_computePipeline, nullptr);
    if (m_computePipelineLayout) vkDestroyPipelineLayout(device, m_computePipelineLayout, nullptr);
    if (m_computeSetLayout) vkDestroyDescriptorSetLayout(device, m_computeSetLayout, nullptr);
    if (m_computePool) vkDestroyDescriptorPool(device, m_computePool, nullptr);

    if (m_tonemapPipeline) vkDestroyPipeline(device, m_tonemapPipeline, nullptr);
    if (m_tonemapPipelineLayout) vkDestroyPipelineLayout(device, m_tonemapPipelineLayout, nullptr);
    if (m_tonemapSetLayout) vkDestroyDescriptorSetLayout(device, m_tonemapSetLayout, nullptr);
    if (m_tonemapPool) vkDestroyDescriptorPool(device, m_tonemapPool, nullptr);

    destroyAccumImage();
}

void PathTracer::init() {
    m_descriptorManager = std::make_unique<DescriptorManager>(m_context);
    m_descriptorManager->updateTextures(m_mesh->getTextures());

    m_accel = std::make_unique<AccelerationStructure>(m_context, m_mesh);

    VkExtent2D extent = m_swapchain->getExtent();
    m_width = extent.width;
    m_height = extent.height;
    createAccumImage(m_width, m_height);

    createDescriptors();
    updateDescriptors();
    createComputePipeline();
    createTonemapPipeline();
}

void PathTracer::createAccumImage(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };
    CHK(vmaCreateImage(m_context->getAllocator(), &imageInfo, &allocInfo, &m_accumImage, &m_accumAllocation, nullptr));

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_accumImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    CHK(vkCreateImageView(m_context->getDevice(), &viewInfo, nullptr, &m_accumView));

    m_accumLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_frameIndex = 0;
}

void PathTracer::destroyAccumImage() {
    VkDevice device = m_context->getDevice();
    if (m_accumView) { vkDestroyImageView(device, m_accumView, nullptr); m_accumView = VK_NULL_HANDLE; }
    if (m_accumImage) { vmaDestroyImage(m_context->getAllocator(), m_accumImage, m_accumAllocation); m_accumImage = VK_NULL_HANDLE; }
}

void PathTracer::createDescriptors() {
    VkDevice device = m_context->getDevice();

    // --- compute set (set 1): TLAS + 누적 storage image ---
    std::array<VkDescriptorPoolSize, 2> computePoolSizes = { {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    } };
    VkDescriptorPoolCreateInfo computePoolCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(computePoolSizes.size()),
        .pPoolSizes = computePoolSizes.data()
    };
    CHK(vkCreateDescriptorPool(device, &computePoolCI, nullptr, &m_computePool));

    std::array<VkDescriptorSetLayoutBinding, 2> computeBindings = { {
        { 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }
    } };
    VkDescriptorSetLayoutCreateInfo computeLayoutCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(computeBindings.size()),
        .pBindings = computeBindings.data()
    };
    CHK(vkCreateDescriptorSetLayout(device, &computeLayoutCI, nullptr, &m_computeSetLayout));

    VkDescriptorSetAllocateInfo computeAlloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_computePool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_computeSetLayout
    };
    CHK(vkAllocateDescriptorSets(device, &computeAlloc, &m_computeSet));

    // --- tonemap set: 누적 이미지를 sampled image로 ---
    VkDescriptorPoolSize tonemapPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 };
    VkDescriptorPoolCreateInfo tonemapPoolCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &tonemapPoolSize
    };
    CHK(vkCreateDescriptorPool(device, &tonemapPoolCI, nullptr, &m_tonemapPool));

    VkDescriptorSetLayoutBinding tonemapBinding{ 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
    VkDescriptorSetLayoutCreateInfo tonemapLayoutCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &tonemapBinding
    };
    CHK(vkCreateDescriptorSetLayout(device, &tonemapLayoutCI, nullptr, &m_tonemapSetLayout));

    VkDescriptorSetAllocateInfo tonemapAlloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_tonemapPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_tonemapSetLayout
    };
    CHK(vkAllocateDescriptorSets(device, &tonemapAlloc, &m_tonemapSet));
}

void PathTracer::updateDescriptors() {
    VkDevice device = m_context->getDevice();

    // compute: TLAS
    VkAccelerationStructureKHR tlas = m_accel->getTLAS();
    VkWriteDescriptorSetAccelerationStructureKHR asWriteInfo{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };
    VkWriteDescriptorSet asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWriteInfo,
        .dstSet = m_computeSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    // compute: 누적 storage image
    VkDescriptorImageInfo accumStorageInfo{
        .imageView = m_accumView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkWriteDescriptorSet storageWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_computeSet,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &accumStorageInfo
    };

    // tonemap: 누적 sampled image
    VkDescriptorImageInfo accumSampledInfo{
        .imageView = m_accumView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkWriteDescriptorSet tonemapWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_tonemapSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &accumSampledInfo
    };

    std::array<VkWriteDescriptorSet, 3> writes = { asWrite, storageWrite, tonemapWrite };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void PathTracer::createComputePipeline() {
    VkDevice device = m_context->getDevice();

    VkShaderModule computeShader = m_shaderCompiler->compileToShaderModule("../shaders/pathtracing/pathtrace.slang", "computeMain");

    std::array<VkDescriptorSetLayout, 2> setLayouts = {
        m_descriptorManager->getLayout(), // set 0: bindless 텍스처
        m_computeSetLayout                 // set 1: TLAS + 누적 이미지
    };
    VkPushConstantRange pushRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PTPushConstants) };
    VkPipelineLayoutCreateInfo layoutCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    CHK(vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_computePipelineLayout));

    VkComputePipelineCreateInfo pipelineCI{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = computeShader,
            .pName = "main"
        },
        .layout = m_computePipelineLayout
    };
    CHK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_computePipeline));

    vkDestroyShaderModule(device, computeShader, nullptr);
}

void PathTracer::createTonemapPipeline() {
    VkDevice device = m_context->getDevice();

    VkShaderModule vertShader = m_shaderCompiler->compileToShaderModule("../shaders/pathtracing/fullscreen.slang", "vertexMain");
    VkShaderModule fragShader = m_shaderCompiler->compileToShaderModule("../shaders/pathtracing/fullscreen.slang", "fragmentMain");

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = { {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertShader, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragShader, .pName = "main" }
    } };

    VkPipelineLayoutCreateInfo layoutCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_tonemapSetLayout
    };
    CHK(vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_tonemapPipelineLayout));

    VkPipelineVertexInputStateCreateInfo vertexInput{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{ .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dynamicStates.data() };
    VkPipelineViewportStateCreateInfo viewportState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1 };
    VkPipelineRasterizationStateCreateInfo rasterizer{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.0f };
    VkPipelineMultisampleStateCreateInfo multisampling{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
    VkPipelineColorBlendAttachmentState colorBlendAttachment{ .blendEnable = VK_FALSE, .colorWriteMask = 0xF };
    VkPipelineColorBlendStateCreateInfo colorBlending{ .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &colorBlendAttachment };

    VkFormat colorFormat = m_swapchain->getImageFormat();
    VkPipelineRenderingCreateInfo renderingCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat
    };

    VkGraphicsPipelineCreateInfo pipelineCI{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCI,
        .stageCount = 2,
        .pStages = stages.data(),
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = m_tonemapPipelineLayout
    };
    CHK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_tonemapPipeline));

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

void PathTracer::onResize(uint32_t width, uint32_t height) {
    // ZeroApp이 vkDeviceWaitIdle 후 호출 → 안전하게 재생성
    destroyAccumImage();
    createAccumImage(width, height);
    updateDescriptors();
    m_frameIndex = 0;
}

void PathTracer::recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) {
    glm::mat4 view = m_camera->getViewMatrix();
    glm::mat4 proj = m_camera->getProjectionMatrix();
    glm::mat4 viewProj = proj * view;

    // 카메라가 움직였으면 누적 리셋
    if (viewProj != m_prevViewProj) {
        m_frameIndex = 0;
        m_prevViewProj = viewProj;
    }

    // 1. 누적 이미지: 이전 레이아웃 -> GENERAL (compute write)
    VkImageMemoryBarrier2 toGeneral{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .oldLayout = m_accumLayout,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = m_accumImage,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    if (m_accumLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
        toGeneral.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        toGeneral.srcAccessMask = 0;
    }
    VkDependencyInfo dep1{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toGeneral };
    vkCmdPipelineBarrier2(cmd, &dep1);

    // 2. 패스트레이싱 compute dispatch
    PTPushConstants pc{
        .invViewProj = glm::inverse(viewProj),
        .cameraPos = glm::vec4(m_camera->getPosition(), 1.0f),
        .sunDirection = glm::vec4(m_sunDirection, 0.0f),
        .sunColor = glm::vec4(m_sunColor, m_sunIntensity),
        .ambient = glm::vec4(m_ambientColor, m_ambientIntensity),
        .vertexAddr = m_mesh->getVertexBufferAddress(),
        .indexAddr = m_mesh->getIndexBufferAddress(),
        .materialAddr = m_mesh->getMaterialBufferAddress(),
        .triMaterialAddr = m_mesh->getTriMaterialBufferAddress(),
        .frameIndex = m_frameIndex,
        .width = m_width,
        .height = m_height,
        .maxBounces = static_cast<uint32_t>(m_maxBounces),
        .sunAngularRadius = glm::radians(m_sunAngularRadiusDeg)
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
    std::array<VkDescriptorSet, 2> sets = { m_descriptorManager->getDescriptorSet(), m_computeSet };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0,
        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, m_computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch(cmd, (m_width + 7) / 8, (m_height + 7) / 8, 1);

    // 3. 누적 이미지: GENERAL -> SHADER_READ_ONLY (compute write -> fragment sample)
    VkImageMemoryBarrier2 toRead{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = m_accumImage,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo dep2{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toRead };
    vkCmdPipelineBarrier2(cmd, &dep2);
    m_accumLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 4. 스왑체인: UNDEFINED -> COLOR_ATTACHMENT
    VkImageMemoryBarrier2 swapToAttachment{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .image = m_swapchain->getImage(imageIndex),
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo dep3{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &swapToAttachment };
    vkCmdPipelineBarrier2(cmd, &dep3);

    // 5. 톤맵 풀스크린 패스 -> 스왑체인
    VkExtent2D extent = m_swapchain->getExtent();
    VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_swapchain->getImageView(imageIndex),
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE
    };
    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { {0, 0}, extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment
    };
    vkCmdBeginRendering(cmd, &renderingInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tonemapPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tonemapPipelineLayout, 0, 1, &m_tonemapSet, 0, nullptr);
    VkViewport viewport{ 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{ {0, 0}, extent };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);

    // 스왑체인은 ATTACHMENT_OPTIMAL로 남겨둠 (ZeroApp이 UI + present 처리)
    m_frameIndex++;
}

void PathTracer::onUI() {
    if (ImGui::Begin("Path Tracer")) {
        ImGui::Text("Accumulated samples: %u", m_frameIndex);
        ImGui::Text("Resolution: %u x %u", m_width, m_height);

        bool dirty = false;
        ImGui::SeparatorText("Sun");
        dirty |= ImGui::DragFloat3("Direction", &m_sunDirection.x, 0.01f, -1.0f, 1.0f);
        dirty |= ImGui::ColorEdit3("Color##sun", &m_sunColor.x);
        dirty |= ImGui::DragFloat("Intensity##sun", &m_sunIntensity, 0.05f, 0.0f, 50.0f);
        dirty |= ImGui::DragFloat("Angular radius (deg)", &m_sunAngularRadiusDeg, 0.05f, 0.05f, 20.0f);

        ImGui::SeparatorText("Environment / bounces");
        dirty |= ImGui::ColorEdit3("Sky color", &m_ambientColor.x);
        dirty |= ImGui::DragFloat("Sky intensity", &m_ambientIntensity, 0.01f, 0.0f, 5.0f);
        dirty |= ImGui::SliderInt("Max bounces", &m_maxBounces, 1, 12);

        if (dirty) m_frameIndex = 0; // 파라미터 변경 시 누적 리셋
    }
    ImGui::End();
}
