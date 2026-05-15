#ifndef ZERO_SURFACE_INTERACTION_CUDA_HEADER
#define ZERO_SURFACE_INTERACTION_CUDA_HEADER

#include "math.cuh"

struct SurfaceInteraction 
{
    float t;
    float3 p;
    float3 n;
    class Material* pMat;
};

#endif