#include "scene.cuh"
#include <filesystem>


bool Scene::LoadObjFile(const std::string filename)
{
    std::filesystem::path objPath(filename.c_str());
    if (!std::filesystem::exists(objPath))
    {
        std::cerr << "[Error]: File not found - " << filename << std::endl;
        return false;
    }

    std::string objFp = objPath.string();
    std::string mtlDir = objPath.parent_path().string() + "/";

    tinyobj::ObjReaderConfig cfg;
    cfg.mtl_search_path = mtlDir;
    cfg.triangulate = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filename, cfg))
    {
        if (!reader.Error().empty())
        {
            std::cerr << "TinyObjReader Error: " << reader.Error() << std::endl;
        }
        return false;
    }

    if (!reader.Warning().empty())
    {
        std::cerr << "TinyObjReader Warning: " << reader.Warning() << std::endl;
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &materials = reader.GetMaterials();

    for (const auto &shape : shapes)
    {
        size_t idxOffset = 0;
        for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); face++)
        {
            int fv = shape.mesh.num_face_vertices[face];

            float3 vertices[3];
            float2 uvs[3];

            for (int i = 0; i < 3; i++)
            {
                tinyobj::index_t idx = shape.mesh.indices[idxOffset + i];

                // get vertices info 
                vertices[i] = make_float3(
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]);

                // get texture coordinate info
                if (idx.texcoord_index >= 0)
                {
                    uvs[i] = make_float2(
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        attrib.texcoords[2 * idx.texcoord_index + 1]
                    );
                }
                else
                {
                    uvs[i] = make_float2(0, 0);
                }
            }

            int matId = shape.mesh.material_ids[face];
            if (matId < 0 || matId >= materials.size())
            {
                matId = 0;
            }

            printf("material index = %d\n", matId);
            printf("material illum idx = %f %f %f\n", 
                materials[matId].diffuse[0],
                materials[matId].diffuse[1],
                materials[matId].diffuse[2]
            );
            printf("material emissive idx = %f %f %f\n", 
                materials[matId].emission[0],
                materials[matId].emission[1],
                materials[matId].emission[2]
            );
            printf("material metallic idx = %f\n", materials[matId].metallic);
            printf("material roughness idx = %f\n", materials[matId].roughness);
            printf("%f %f %f\n", materials[matId].specular[0], materials[matId].specular[1], materials[matId].specular[2]);

            

            idxOffset += fv;
        }
    }

    return true;
}
