#ifndef ZERO_PRIMITIVES_CUDA_HEADER
#define ZERO_PRIMITIVES_CUDA_HEADER

#include <cuda_runtime.h>
#include "ray.cuh"

struct SurfaceInteraction 
{
    float t;
    float3 p;
    float3 n;
    class Material* pMat;
};

class Primitive
{
public:
    __device__ virtual bool Hit(const Ray &r, float tMin, float tMax, struct SurfaceInteraction &si) const = 0;
};

class Sphere : public Primitive
{
public:
    __device__ Sphere() {}
    __device__ Sphere(float3 cen, float r, class Material *m) : m_Center(cen), m_Radius(r), m_pMat(m) {};
    __device__ virtual bool Hit(const Ray &r, float tMin, float tMax, struct SurfaceInteraction &si) const
    {
        float3 oc = r.origin() - m_Center;
        float a = dot(r.dir(), r.dir());
        float b = dot(oc, r.dir());
        float c = dot(oc, oc) - m_Radius * m_Radius;
        float discriminant = b * b - a * c;
        if (discriminant > 0)
        {
            float temp = (-b - sqrt(discriminant)) / a;
            if (temp < tMax && temp > tMin)
            {
                si.t = temp;
                si.p = r.at(si.t);
                si.n = (si.p - m_Center) / m_Radius;
                si.pMat = m_pMat;
                return true;
            }
            temp = (-b + sqrt(discriminant)) / a;
            if (temp < tMax && temp > tMin)
            {
                si.t = temp;
                si.p = r.at(si.t);
                si.n = (si.p - m_Center) / m_Radius;
                si.pMat = m_pMat;
                return true;
            }
        }
        return false;
    }

    float3 m_Center;
    float m_Radius;
    class Material *m_pMat;
};

class Triangle : public Primitive
{
public:
    __host__ __device__ Triangle() {}
    __host__ __device__ Triangle(float3 v1, float3 v2, float3 v3, float2 uv, class Material *m) : m_Vertex{v1, v2, v3}, m_TexCoord(uv) 
    {
        m_Normal = normalize(cross(m_Vertex[1] - m_Vertex[0], m_Vertex[2] - m_Vertex[0]));
    }

    __device__ virtual bool Hit(const Ray &r, float tMin, float tMax, struct SurfaceInteraction &si) const 
    {
        float3 e1 = m_Vertex[1] - m_Vertex[0];
        float3 e2 = m_Vertex[2] - m_Vertex[0];
        float3 pvec = cross(r.dir(), e2);
        float det = dot(e1, pvec);
        if (det > -1e-8f && det < 1e-8f) return false;

        float invDet = 1.0f / det;
        float3 tvec = r.origin() - m_Vertex[0];
        float u = dot(tvec, pvec) * invDet;
        if (u < 0.0f || u > 1.0f) return false;

        float3 qvec = cross(tvec, e1);
        float v = dot(r.dir(), qvec) * invDet;
        if (v < 0.0f || u + v > 1.0f) return false;

        float t = dot(e2, qvec) * invDet;
        if (t < tMax && t > tMin)
        {
            si.t = t;
            si.p = r.at(si.t);
            si.n = m_Normal;
            si.pMat = m_pMat;
            return true;
        }

        return false;
    }

private:
    float3 m_Vertex[3];
    float3 m_Normal;
    float2 m_TexCoord;
    class Material *m_pMat;
};

#endif