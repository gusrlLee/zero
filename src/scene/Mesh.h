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

// GPU 머티리얼 (셰이더의 GpuMaterial과 레이아웃 일치, std430/scalar 호환).
// 텍스처 인덱스는 bindless 배열(=model.images) 인덱스이며 없으면 -1.
struct GpuMaterial {
    glm::vec4 baseColorFactor{ 1, 1, 1, 1 };
    glm::vec4 emissiveFactor{ 0, 0, 0, 1 }; // rgb = emissive, w = strength
    float metallic = 1.0f;
    float roughness = 1.0f;
    int32_t baseColorTex = -1;
    int32_t metalRoughTex = -1;
    int32_t normalTex = -1;
    int32_t emissiveTex = -1;
    int32_t occlusionTex = -1;
    int32_t _pad = 0;
};

struct SubMesh {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;

    uint32_t materialIndex = 0; // m_materials 인덱스
};

class Mesh {
public:
    Mesh(VulkanContext* context, const std::string& filepath);
    ~Mesh();

    const std::vector<SubMesh>& getSubMeshes() const { return m_subMeshes; }
    VkBuffer getVertexBuffer() const { return m_vertexBuffer->getHandle(); }
    VkBuffer getIndexBuffer() const { return m_indexBufferHandle;  }
    uint64_t getVertexBufferAddress() const;
    uint64_t getIndexBufferAddress() const;
    uint64_t getMaterialBufferAddress() const;
    uint64_t getTriMaterialBufferAddress() const; // 삼각형별 머티리얼 인덱스 (레이트레이싱용)

    const std::vector<std::unique_ptr<Texture>>& getTextures() const { return m_textures; }
    size_t getMaterialCount() const { return m_materials.size(); }

    // 레이트레이싱 가속 구조(BLAS) 빌드 입력용
    uint32_t getVertexCount() const { return m_vertexCount; }
    uint32_t getTriangleCount() const { return m_triangleCount; }
    uint32_t getVertexStride() const { return sizeof(Vertex); }

private:
    void loadGltf(const std::string& filepath);
    void parseMaterials(const tinygltf::Model& model);
    void createBuffers();
    void processNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parentMatrix);

private:
    VulkanContext* m_context;

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<SubMesh> m_subMeshes; // 서브 메쉬 리스트
    std::vector<GpuMaterial> m_materials;

    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;
    std::unique_ptr<Buffer> m_materialBuffer;
    std::unique_ptr<Buffer> m_triMaterialBuffer;

    uint32_t m_indexCount = 0;
    uint32_t m_vertexCount = 0;
    uint32_t m_triangleCount = 0;
    VkBuffer m_indexBufferHandle{ VK_NULL_HANDLE };
    std::vector<std::unique_ptr<Texture>> m_textures;
};

#endif