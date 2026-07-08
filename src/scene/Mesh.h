#ifndef __ZERO_MESH_HEADER__
#define __ZERO_MESH_HEADER__

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "core/Buffer.h"

#include <vector>
#include <memory>
#include <string>

class VulkanContext;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct SubMesh {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset; // 정점 버퍼 안에서의 시작점
};

class Mesh {
public:
    Mesh(VulkanContext* context, const std::string& filepath);
    ~Mesh();

    const std::vector<SubMesh>& getSubMeshes() const { return m_subMeshes; }
    VkBuffer getVertexBuffer() const { return m_vertexBuffer->getHandle(); }
    VkBuffer getIndexBuffer() const { return m_indexBufferHandle;  }
    uint64_t getVertexBufferAddress() const;

private:
    void loadGltf(const std::string& filepath);
    void createBuffers();

private:
    VulkanContext* m_context;

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<SubMesh> m_subMeshes; // 서브 메쉬 리스트

    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;

    uint32_t m_indexCount = 0;
    VkBuffer m_indexBufferHandle{ VK_NULL_HANDLE };
};

#endif