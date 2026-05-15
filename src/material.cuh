#ifndef ZERO_METERIAL_CUDA_HEADER
#define ZERO_METERIAL_CUDA_HEADER

#include <cuda_runtime.h>
#include <curand_kernel.h>

#include "math.cuh"
#include "ray.cuh"

class Material
{
public:
    __device__ virtual bool f(const Ray &wi, const struct SurfaceInteraction &si, float3 &attenuation, Ray &wo, curandState *randState) const = 0;
};



#endif
