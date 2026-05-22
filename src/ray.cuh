#ifndef ZERO_RAY_CUDA_HEADER
#define ZERO_RAY_CUDA_HEADER

#include "math.cuh"

class Ray
{
public:
    __host__ __device__ Ray() {}
    __host__ __device__ Ray(const float3 &o, const float3 &d) : m_orig(o), m_dir(d) {}
    __host__ __device__ float3 origin() const { return m_orig; }
    __host__ __device__ float3 dir() const { return m_dir; }
    __host__ __device__ float3 at(float t) const { return m_orig + t * m_dir; }

private:
    float3 m_orig;
    float3 m_dir;
};

#endif