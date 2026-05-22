#ifndef ZERO_METERIAL_CUDA_HEADER
#define ZERO_METERIAL_CUDA_HEADER

#include <cuda_runtime.h>
#include <curand_kernel.h>

#include "math.cuh"
#include "ray.cuh"
#include "primitives.cuh"

class Material
{
public:
    __device__ virtual bool f(const Ray &wi, const SurfaceInteraction &si, float3 &attenuation, Ray &wo, curandState *randState) const = 0;
};

class Lambertian : public Material
{
public:
    __device__ Lambertian(const float3 &a) : m_Albedo(a) {}
    __device__ virtual bool f(const Ray &wi, const SurfaceInteraction &si, float3 &attenuation, Ray &wo, curandState *randState) const 
    {
        float3 randomVector = normalize(make_random_float3(randState));
        float3 target = si.p + si.n;
    }

    float3 m_Albedo;
};

#endif
