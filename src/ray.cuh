#ifndef ZERO_RAY_CUDA_HEADER
#define ZERO_RAY_CUDA_HEADER

#include "math.cuh"

class Ray
{
public:
    __host__ __device__ Ray() {}
    __host__ __device__ Ray(const float3 &origin, const float3 &direction) : m_Origin(origin), m_Direction(direction) {}
    __host__ __device__ float3 Origin() const { return m_Origin; }
    __host__ __device__ float3 Direction() const { return m_Direction; }
    __host__ __device__ float3 At(float t) const { return m_Origin + t * m_Direction; }

private:
    float3 m_Origin;
    float3 m_Direction;
};

#endif