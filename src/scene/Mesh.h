#ifndef __ZERO_MESH_HEADER__
#define __ZERO_MESH_HEADER__

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tinygltf/tiny_gltf.h>
#include <vulkan/vulkan.h>

#include "core/Buffer.h"
#include "renderer/Texture.h"

#include <vector>
#include <memory>
#include <string>

class VulkanContext;

struct Vertex {
    float px, py, pz; // position
    float nx, ny, nz; // normal
    float u, v;       // uv
};

struct SubMesh {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;

    int32_t textureIndex = -1;
};

class Mesh {
public:
    Mesh(VulkanContext* context, const std::string& filepath);
    ~Mesh();

    const std::vector<SubMesh>& getSubMeshes() const { return m_subMeshes; }
    VkBuffer getVertexBuffer() const { return m_vertexBuffer->getHandle(); }
    VkBuffer getIndexBuffer() const { return m_indexBufferHandle;  }
    uint64_t getVertexBufferAddress() const;

    const std::vector<std::unique_ptr<Texture>>& getTextures() const { return m_textures; }

private:
    void loadGltf(const std::string& filepath);
    void createBuffers();
    void processNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parentMatrix);

private:
    VulkanContext* m_context;

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<SubMesh> m_subMeshes; // 서브 메쉬 리스트

    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;

    uint32_t m_indexCount = 0;
    VkBuffer m_indexBufferHandle{ VK_NULL_HANDLE };
    std::vector<std::unique_ptr<Texture>> m_textures;
};

#endif