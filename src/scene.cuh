#ifndef ZERO_SCENE_CUDA_HEADER
#define ZERO_SCENE_CUDA_HEADER

#include <iostream>
#include <vector>
#include <string>

#include "math.cuh"
#include "structs.cuh"

#ifndef TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS
#define TINYGLTF3_ENABLE_STB_IMAGE
#include "tinygltf/tiny_gltf_v3.h"
#endif

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"
#endif

class Scene
{
public:
    Scene() {}
    ~Scene() {}

    void Clear()
    {
        m_Vertices.clear();
        m_Indices.clear();
        m_Materials.clear();
    }

    const float *GetPointer(const tinygltf3::Model &model, int aIdx)
    {
        if (aIdx < 0)
            return nullptr;

        const tg3_accessor &acc = model->accessors[aIdx];
        const tg3_buffer_view &bv = model->buffer_views[acc.buffer_view];
        const tg3_buffer &buf = model->buffers[bv.buffer];

        return (const float *)(buf.data.data + acc.byte_offset + bv.byte_offset);
    }

    bool LoadObjFile(const std::string filename)
    {
        tinyobj::ObjReaderConfig cfg;
        cfg.triangulate = true;
        cfg.mtl_search_path = "./";

        tinyobj::ObjReader reader;
        if (!reader.ParseFromFile(filename, cfg))
        {
            if (!reader.Error().empty())
            {
                std::cerr << "Tinyobjloader = " << reader.Error() << std::endl;
                return false;
            }
        }

        auto &attrib = reader.GetAttrib();
        auto &shapes = reader.GetShapes();
        auto &materials = reader.GetMaterials();

        for (const auto &m : materials)
        {
            MaterialData mat;
            mat.baseColorFactor = make_float4(m.diffuse[0], m.diffuse[1], m.diffuse[2], 1.0f);
            mat.emissiveFactor = make_float3(m.emission[0], m.emission[1], m.emission[2]);

            mat.metallicFactor = m.metallic;
            mat.roughnessFactor = m.roughness;
            mat.baseColorTextureIndex = -1;
            m_Materials.push_back(mat);
        }

        if (m_Materials.empty())
        {
            MaterialData defaultMat;
            defaultMat.baseColorFactor = make_float4(0.8f, 0.8f, 0.8f, 1.0f);
            defaultMat.emissiveFactor = make_float3(0.0f);
            defaultMat.metallicFactor = 0.0f;
            defaultMat.roughnessFactor = 0.5f;

            defaultMat.baseColorTextureIndex = -1;
            m_Materials.push_back(defaultMat);
        }

        // Loop over shapes
        for (size_t s = 0; s < shapes.size(); s++)
        {
            // Loop over faces(polygon)
            size_t index_offset = 0;
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
            {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
                
                Vertex vertex;
                // Loop over vertices in the face.
                for (size_t v = 0; v < fv; v++)
                {

                    // access to vertex
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                    vertex.position = make_float3(
                        attrib.vertices[3 * idx.vertex_index + 0],
                        attrib.vertices[3 * idx.vertex_index + 1],
                        attrib.vertices[3 * idx.vertex_index + 2]
                    );

                }
                index_offset += fv;

                // per-face material
                shapes[s].mesh.material_ids[f];
            }
        }
    }

    bool LoadGltfFile(const std::string filename)
    {
        Clear();

        tinygltf3::Model model;
        tinygltf3::ErrorStack errors;

        if (tinygltf3::parse_file(model, errors, filename.c_str()) != TG3_OK)
        {
            std::cout << "Failed to load " << filename << std::endl;
            return false;
        }

        for (size_t i = 0; i < model->materials_count; i++)
        {
            const tg3_material &mat = model->materials[i];
            const auto &pbr = mat.pbr_metallic_roughness;

            MaterialData material;

            material.baseColorFactor = make_float4(
                (float)pbr.base_color_factor[0],
                (float)pbr.base_color_factor[1],
                (float)pbr.base_color_factor[2],
                (float)pbr.base_color_factor[3]);

            material.metallicFactor = (float)pbr.metallic_factor;
            material.roughnessFactor = (float)pbr.roughness_factor;

            if (pbr.base_color_texture.index >= 0)
            {
                material.baseColorTextureIndex = pbr.base_color_texture.index;

                const tg3_texture &tex = model->textures[material.baseColorTextureIndex];
                if (tex.source >= 0)
                {
                    const tg3_image &img = model->images[tex.source];
                    printf("Texture Name : %s, width : %d, height : %d\n", img.name.data ? img.name.data : "unnamed", img.width, img.height);
                }
            }
            else
            {
                material.baseColorTextureIndex = -1;
            }
            m_Materials.push_back(material);
        }

        std::cout << "Material count = " << m_Materials.size() << std::endl;

        int posIdx = -1;
        int normIdx = -1;
        int uvIdx = -1;

        for (size_t m = 0; m < model->meshes_count; m++)
        {
            const tg3_mesh &mesh = model->meshes[m];

            for (size_t p = 0; p < mesh.primitives_count; p++)
            {
                const tg3_primitive &prim = mesh.primitives[p];
                for (size_t a = 0; a < prim.attributes_count; a++)
                {
                    if (tg3_str_equals_cstr(prim.attributes[a].key, "POSITION") == 1)
                        posIdx = prim.attributes[a].value;
                    if (tg3_str_equals_cstr(prim.attributes[a].key, "NORMAL") == 1)
                        normIdx = prim.attributes[a].value;
                    if (tg3_str_equals_cstr(prim.attributes[a].key, "TEXCOORD_0") == 1)
                        uvIdx = prim.attributes[a].value;
                }

                const float *posPtr = (const float *)GetPointer(model, posIdx);
                const float *normPtr = (const float *)GetPointer(model, normIdx);
                const float *uvPtr = (const float *)GetPointer(model, uvIdx);

                uint64_t vCnt = model->accessors[posIdx].count;

                for (size_t idx = 0; idx < vCnt; idx++)
                {
                    Vertex v;
                    if (posPtr)
                    {
                        v.position = make_float3(posPtr[idx * 3], posPtr[idx * 3 + 1], posPtr[idx * 3 + 2]);
                    }

                    if (normPtr)
                    {
                        v.normal = make_float3(normPtr[idx * 3], normPtr[idx * 3 + 1], normPtr[idx * 3 + 2]);
                    }

                    if (uvPtr)
                    {
                        v.texcoord = make_float2(uvPtr[idx * 2], uvPtr[idx * 2 + 1]);
                    }
                    m_Vertices.push_back(v);
                }
                if (prim.indices >= 0)
                {
                    const tg3_accessor &acc = model->accessors[prim.indices];
                    const void *idxPtr = GetPointer(model, prim.indices);

                    for (size_t i = 0; i < acc.count; i++)
                    {
                        uint32_t index = 0;
                        if (acc.component_type == 5123)
                        {
                            index = ((const uint16_t *)idxPtr)[i];
                        }
                        else if (acc.component_type == 5125)
                        {
                            index = ((const uint32_t *)idxPtr)[i];
                        }
                        m_Indices.push_back(index);
                    }
                }
            }
        }

        std::cout << "Vertices count = " << m_Vertices.size() << std::endl;
        std::cout << "Indices size = " << m_Indices.size() << std::endl;

        return true;
    }

private:
    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;
    std::vector<MaterialData> m_Materials;
};

#endif
