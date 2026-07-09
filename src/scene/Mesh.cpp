#include "scene/Mesh.h"
#include "core/VulkanContext.h"
#include "core/Buffer.h"
#include "util/Log.h"

#include <tinygltf/tiny_gltf.h>
#include <iostream>

Mesh::Mesh(VulkanContext* context, const std::string& filepath) : m_context(context) {
    loadGltf(filepath);
    createBuffers();
}

Mesh::~Mesh() {
    // unique_ptr인 m_vertexBuffer, m_indexBuffer가 알아서 VMA 자원을 해제합니다.
}

void Mesh::loadGltf(const std::string& filepath) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // ★ 수정: 파일을 두 번 로드하던 버그 수정 (한 번만 로드하도록)
    bool ret = false;
    if (filepath.substr(filepath.find_last_of(".") + 1) == "glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    }
    else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    }

    if (!warn.empty()) std::cout << "[Mesh Warn] " << warn << "\n";
    if (!err.empty()) std::cerr << "[Mesh Error] " << err << "\n";
    if (!ret) throw std::runtime_error("Failed to parse glTF");

    std::string baseDir = filepath.substr(0, filepath.find_last_of("/\\") + 1);
    for (const auto& image : model.images) {
        std::string fullPath = baseDir + image.uri;
        m_textures.push_back(std::make_unique<Texture>(m_context, fullPath));
    }

    const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
    for (int nodeIndex : scene.nodes) {
        processNode(model, nodeIndex, glm::mat4(1.0f)); // 초기 행렬은 단위 행렬
    }

    std::cout << "[Mesh] Loaded GLTF: " << filepath << " (Vertices: " << m_vertices.size() << ", Indices: " << m_indices.size() << ")\n";
}

void Mesh::processNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parentMatrix) {
    const tinygltf::Node& node = model.nodes[nodeIndex];

    // 1. 현재 Node의 로컬 변환 행렬 계산
    glm::mat4 localMatrix(1.0f);
    if (node.matrix.size() == 16) {
        for (int i = 0; i < 16; ++i) {
            localMatrix[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
    }
    else {
        glm::mat4 T(1.0f), R(1.0f), S(1.0f);
        if (node.translation.size() == 3)
            T = glm::translate(glm::mat4(1.0f), glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        if (node.rotation.size() == 4) {
            glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            R = glm::mat4_cast(q);
        }
        if (node.scale.size() == 3)
            S = glm::scale(glm::mat4(1.0f), glm::vec3(node.scale[0], node.scale[1], node.scale[2]));

        localMatrix = T * R * S;
    }

    // 2. 부모 행렬과 곱해서 최종 글로벌 행렬 생성
    glm::mat4 globalMatrix = parentMatrix * localMatrix;
    // 노멀(법선) 벡터용 행렬 (비균율 스케일링 보정)
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(globalMatrix)));

    // 3. 만약 이 Node에 Mesh가 있다면 정점/인덱스 파싱
    if (node.mesh >= 0) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (const auto& primitive : mesh.primitives) {
            if (primitive.mode != 4 && primitive.mode != -1) continue;
            if (primitive.indices < 0) continue;

            SubMesh subMesh{};
            subMesh.vertexOffset = static_cast<int32_t>(m_vertices.size());
            subMesh.firstIndex = static_cast<uint32_t>(m_indices.size());

            if (primitive.material >= 0) {
                const tinygltf::Material& mat = model.materials[primitive.material];

                int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;

                // metallic-roughness에 baseColor가 없으면 spec-gloss 확장의 diffuseTexture 사용
                // (bistro는 KHR_materials_pbrSpecularGlossiness 워크플로우)
                if (texIndex < 0) {
                    auto it = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
                    if (it != mat.extensions.end() && it->second.Has("diffuseTexture")) {
                        const tinygltf::Value& difTex = it->second.Get("diffuseTexture");
                        if (difTex.Has("index")) {
                            texIndex = difTex.Get("index").GetNumberAsInt();
                        }
                    }
                }

                if (texIndex >= 0) {
                    subMesh.textureIndex = model.textures[texIndex].source;
                }
            }

            // --- 인덱스 파싱 (기존과 동일) ---
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const unsigned char* indexPtr = &(model.buffers[indexBufferView.buffer].data[indexAccessor.byteOffset + indexBufferView.byteOffset]);
            int indexStride = indexAccessor.ByteStride(indexBufferView) ? indexAccessor.ByteStride(indexBufferView) : tinygltf::GetComponentSizeInBytes(indexAccessor.componentType);

            for (size_t i = 0; i < indexAccessor.count; i++) {
                uint32_t indexValue = 0;
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    memcpy(&indexValue, indexPtr + i * indexStride, sizeof(uint32_t));
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    uint16_t shortVal; memcpy(&shortVal, indexPtr + i * indexStride, sizeof(uint16_t)); indexValue = shortVal;
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    uint8_t byteVal; memcpy(&byteVal, indexPtr + i * indexStride, sizeof(uint8_t)); indexValue = byteVal;
                }
                // ★ 버그 수정: 정점 풀링(device address) 방식에서는 셰이더가 SV_VertexID로
                //   전역 정점 버퍼를 직접 인덱싱한다. glTF 인덱스는 프리미티브-로컬(0-베이스)이므로
                //   vertexOffset을 더해 전역 인덱스로 변환해야 서브메쉬가 여러 개(Sponza)일 때
                //   지오메트리가 꼬이지 않는다.
                m_indices.push_back(indexValue + static_cast<uint32_t>(subMesh.vertexOffset));
            }

            // --- 정점 파싱 ---
            const unsigned char* posPtr = nullptr;
            const unsigned char* normPtr = nullptr;
            const unsigned char* uvPtr = nullptr;
            int posStride = 0, normStride = 0, uvStride = 0;
            size_t vertexCount = 0;

            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                posPtr = &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
                posStride = accessor.ByteStride(view) ? accessor.ByteStride(view) : sizeof(float) * 3;
                vertexCount = accessor.count;
            }
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                normPtr = &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
                normStride = accessor.ByteStride(view) ? accessor.ByteStride(view) : sizeof(float) * 3;
            }
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                uvPtr = &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
                uvStride = accessor.ByteStride(view) ? accessor.ByteStride(view) : sizeof(float) * 2;
            }

            for (size_t i = 0; i < vertexCount; i++) {
                Vertex v{};
                if (posPtr) {
                    float p[3]; memcpy(p, posPtr + i * posStride, sizeof(float) * 3);
                    glm::vec4 worldPos = globalMatrix * glm::vec4(p[0], p[1], p[2], 1.0f);
                    v.px = worldPos.x; v.py = worldPos.y; v.pz = worldPos.z;
                }
                if (normPtr) {
                    float n[3]; memcpy(n, normPtr + i * normStride, sizeof(float) * 3);
                    glm::vec3 normal = glm::normalize(normalMatrix * glm::vec3(n[0], n[1], n[2]));
                    v.nx = normal.x; v.ny = normal.y; v.nz = normal.z;
                }
                if (uvPtr) {
                    float u[2]; memcpy(u, uvPtr + i * uvStride, sizeof(float) * 2);
                    v.u = u[0]; v.v = u[1];
                }
                m_vertices.push_back(v);
            }
            subMesh.indexCount = static_cast<uint32_t>(m_indices.size()) - subMesh.firstIndex;
            m_subMeshes.push_back(subMesh);
        }
    }

    // 4. 자식 Node들도 순회
    for (int childIndex : node.children) {
        processNode(model, childIndex, globalMatrix);
    }
}

void Mesh::createBuffers() {
    m_indexCount = static_cast<uint32_t>(m_indices.size());

    VkDeviceSize vertexBufferSize = sizeof(Vertex) * m_vertices.size();
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();

    // 1. 임시 택배 상자(Staging Buffer) 생성 - CPU(RAM)에서 접근 가능
    Buffer stagingVertexBuffer(
        m_context,
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );
    stagingVertexBuffer.uploadData(m_vertices.data(), vertexBufferSize);

    Buffer stagingIndexBuffer(
        m_context,
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );
    stagingIndexBuffer.uploadData(m_indices.data(), indexBufferSize);

    // 2. 실제 렌더링에 쓰일 초고속 VRAM 버퍼 생성 (0을 주어 VRAM에 할당 유도)
    // ★ 주의: TRANSFER_DST_BIT 플래그를 꼭 추가해야 복사 받을 수 있습니다!
    m_vertexBuffer = std::make_unique<Buffer>(
        m_context,
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        0 // VMA 내부적으로 VRAM(Device Local)을 선호하게 됨
    );

    m_indexBuffer = std::make_unique<Buffer>(
        m_context,
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        0 // VRAM 할당
    );
    m_indexBufferHandle = m_indexBuffer->getHandle();

    // 3. Staging Buffer -> VRAM Buffer 로 초고속 복사!
    VkCommandBuffer cmd = m_context->beginSingleTimeCommands();

    VkBufferCopy vertexCopyRegion{ .size = vertexBufferSize };
    vkCmdCopyBuffer(cmd, stagingVertexBuffer.getHandle(), m_vertexBuffer->getHandle(), 1, &vertexCopyRegion);

    VkBufferCopy indexCopyRegion{ .size = indexBufferSize };
    vkCmdCopyBuffer(cmd, stagingIndexBuffer.getHandle(), m_indexBuffer->getHandle(), 1, &indexCopyRegion);

    m_context->endSingleTimeCommands(cmd);

    // 4. (선택) GPU로 데이터가 완벽히 넘어갔으니, 메모리 확보를 위해 CPU의 원본 데이터 삭제
    m_vertices.clear();
    m_indices.clear();
}
VkDeviceAddress Mesh::getVertexBufferAddress() const {
    return m_vertexBuffer->getDeviceAddress();
}