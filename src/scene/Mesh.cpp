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

    if (!loader.LoadASCIIFromFile(&model, &err, &warn, filepath)) {
        if (!loader.LoadBinaryFromFile(&model, &err, &warn, filepath)) {
            throw std::runtime_error("[Mesh] Failed to load GLTF: " + err);
        }
    }

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

    // 기존의 for문 내부를 이렇게 바꾸세요
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            // 1. 인덱스/정점 정보가 있는 프리미티브인지 확인
            if (primitive.indices < 0) continue;

            SubMesh subMesh{};
            subMesh.vertexOffset = static_cast<int32_t>(m_vertices.size());
            subMesh.firstIndex = static_cast<uint32_t>(m_indices.size());

            // --- 1. 인덱스(Index) 데이터 파싱 (hardcode [0] 제거) ---
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

            const void* dataPtr = &(indexBuffer.data[indexAccessor.byteOffset + indexBufferView.byteOffset]);

            // 인덱스 로딩 (이전 로직 유지)
            if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                const uint32_t* buf = static_cast<const uint32_t*>(dataPtr);
                for (size_t i = 0; i < indexAccessor.count; i++) m_indices.push_back(buf[i]);
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
                for (size_t i = 0; i < indexAccessor.count; i++) m_indices.push_back(buf[i]);
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
                for (size_t i = 0; i < indexAccessor.count; i++) m_indices.push_back(buf[i]);
            }

            // --- 2. 정점(Vertex) 데이터 파싱 (hardcode [0] 제거) ---
            const float* positionBuffer = nullptr;
            const float* normalBuffer = nullptr;
            const float* uvBuffer = nullptr;
            size_t vertexCount = 0;

            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                positionBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                vertexCount = accessor.count;
            }
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                normalBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                uvBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }

            for (size_t i = 0; i < vertexCount; i++) {
                Vertex v{};
                v.position = glm::vec3(positionBuffer[i * 3 + 0], positionBuffer[i * 3 + 1], positionBuffer[i * 3 + 2]);
                if (normalBuffer) v.normal = glm::vec3(normalBuffer[i * 3 + 0], normalBuffer[i * 3 + 1], normalBuffer[i * 3 + 2]);
                if (uvBuffer) v.uv = glm::vec2(uvBuffer[i * 2 + 0], uvBuffer[i * 2 + 1]);
                m_vertices.push_back(v);
            }

            subMesh.indexCount = static_cast<uint32_t>(m_indices.size()) - subMesh.firstIndex;
            m_subMeshes.push_back(subMesh);
        }
    }


    std::cout << "[Mesh] Loaded GLTF: " << filepath << " (Vertices: " << m_vertices.size() << ", Indices: " << m_indices.size() << ")\n";
}

void Mesh::createBuffers() {
    m_indexCount = static_cast<uint32_t>(m_indices.size());

    VkDeviceSize vertexBufferSize = sizeof(Vertex) * m_vertices.size();
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();

    // 1. Vertex Buffer 생성 (BDA 주소 사용 + CPU 접근 가능하게 매핑)
    m_vertexBuffer = std::make_unique<Buffer>(
        m_context,
        vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, // ★ BDA 및 Storage Buffer (Slang용)
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );
    m_vertexBuffer->uploadData(m_vertices.data(), vertexBufferSize);

    // 2. Index Buffer 생성 (인덱스는 기존 방식대로 바인딩)
    m_indexBuffer = std::make_unique<Buffer>(
        m_context,
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );
    m_indexBuffer->uploadData(m_indices.data(), indexBufferSize);
    m_indexBufferHandle = m_indexBuffer->getHandle();
}

VkDeviceAddress Mesh::getVertexBufferAddress() const {
    return m_vertexBuffer->getDeviceAddress();
}