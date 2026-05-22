#ifndef ZERO_SCENE_CUDA_HEADER
#define ZERO_SCENE_CUDA_HEADER

#include <iostream>
#include <vector>
#include <string>

#include "structs.cuh"
#include "aggregate.cuh"

class Scene 
{
public:
    Scene() {}
    ~Scene() {}

    void Clear()
    {
        m_Vertices.clear();
        m_Indices.clear();
    }

    bool LoadObjFile(const std::string filename);

private:
    std::vector<float3> m_Vertices;
    std::vector<uint32_t> m_Indices;
    std::vector<unsigned int> m_MatIndices;
};

#endif