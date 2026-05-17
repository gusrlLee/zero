#ifndef ZERO_CAMERA_CUDA_HEADER
#define ZERO_CAMERA_CUDA_HEADER

#include "math.cuh"
#include "ray.cuh"

class Camera
{
public:
    __host__ __device__ Camera() {}
    __host__ __device__ Camera(
        float3 lookfrom, 
        float3 lookat, 
        float3 vup, 
        float vfov,        
        float aRatio 
    )
    {
        float theta = vfov * PI / 180.0f;
        float h = tanf(theta / 2.0f);
        float viewport_height = 2.0f * h;
        float viewport_width = aRatio * viewport_height;

        w = normalize(lookfrom - lookat); 
        u = normalize(cross(vup, w));
        v = cross(w, u);

        m_Position = lookfrom;
        m_Horizontal = viewport_width * u;
        m_Vertical = viewport_height * v;
        m_LowerLeftCorner = m_Position - (m_Horizontal / 2.0f) - (m_Vertical / 2.0f) - w;
    }

    __host__ __device__ inline Ray generateRay(float s, float t) const
    {
        float3 dir = normalize(m_LowerLeftCorner + s * m_Horizontal + t * m_Vertical - m_Position);
        Ray r = Ray(m_Position, dir);
        return r;
    }

private:
    float3 m_Position;        
    float3 m_LowerLeftCorner;
    float3 m_Horizontal;
    float3 m_Vertical;

    float3 u, v, w;
};

#endif
