#ifndef ZERO_STRUCTS_CUDA_HEADER
#define ZERO_STRUCTS_CUDA_HEADER

#include <cuda_runtime.h>
#include <vector_types.h>

struct Vertex {
    float3 position;
    float3 normal;
    float2 texcoord;
};

struct MaterialData {
    float4 baseColorFactor;
    float3 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    int baseColorTextureIndex;
};

struct DeviceScene {
    Vertex* vertices;
    uint32_t* indices;
    MaterialData* materials;
    
    int numVertices;
    int numIndices;
    int numMaterials;
};

struct LightData {
    float3 color;
    float intensity;
    float3 position;
    float3 direction;
    float range;
};

#endif // ZERO_STRUCTS_CUDA_HEADER