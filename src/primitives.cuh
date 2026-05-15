#ifndef ZERO_PRIMITIVES_CUDA_HEADER
#define ZERO_PRIMITIVES_CUDA_HEADER

#include "ray.cuh"
#include "surface_interaction.cuh"
#include <cuda_runtime.h>

class Primitive
{
public:
    __device__ virtual bool Hit(const Ray &r, float tMin, float tMax, struct SurfaceInteraction &si) const = 0;
};

class Sphere : public Primitive
{
public:
    __device__ Sphere() {}
    __device__ Sphere(float cen, float r, class Material *m) : m_Center(cen), m_Radius(r), m_pMat(m) {};
    __device__ virtual bool Hit(const Ray &r, float tMin, float tMax, struct SurfaceInteraction &si) const
    {
        float3 oc = r.Origin() - m_Center;
        float a = dot(r.Direction(), r.Direction());
        float b = dot(oc, r.Direction());
        float c = dot(oc, oc) - m_Radius * m_Radius;
        float discriminant = b * b - a * c;
        if (discriminant > 0)
        {
            float temp = (-b - sqrt(discriminant)) / a;
            if (temp < tMax && temp > tMin)
            {
                si.t = temp;
                si.p = r.At(si.t);
                si.n = (si.p - m_Center) / m_Radius;
                si.pMat = m_pMat;
                return true;
            }
            temp = (-b + sqrt(discriminant)) / a;
            if (temp < tMax && temp > tMin)
            {
                si.t = temp;
                si.p = r.At(si.t);
                si.n = (si.p - m_Center) / m_Radius;
                si.pMat = m_pMat;
                return true;
            }
        }
        return false;
    }

    float m_Center;
    float m_Radius;
    class Material *m_pMat;
};

class Triangle : public Primitive
{
    float3 m_Vertex[3];
    float3 m_Normal;
    float2 m_TexCoord;
    class Material *m_pMat;
};

#endif