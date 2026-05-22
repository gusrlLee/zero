#ifndef ZERO_AGGREGATE_CUDA_HEADER
#define ZERO_AGGREGATE_CUDA_HEADER

#include <cuda_runtime.h>
#include <iostream>
#include <vector>

#include "ray.cuh"
#include "primitives.cuh"

class Aggregate
{
public:
    __device__ virtual bool Trace(const Ray &r, float tmin, float tmax, SurfaceInteraction &si) const = 0;
};

class List : public Aggregate
{
public:
    List() {}
    List(Primitive **primitives, int numPrimitives) : m_Primitives(primitives), m_NumPrimitives(numPrimitives) {}

    __device__ bool Trace(const Ray &r, float tmin, float tmax, SurfaceInteraction &si) const
    {
        SurfaceInteraction tmp_si;
        bool hit_anything = false;
        float closest_so_far = tmax;

        for (int i = 0; i < m_NumPrimitives; i++)
        {
            if (m_Primitives[i]->Hit(r, tmin, closest_so_far, tmp_si))
            {
                hit_anything = true;
                closest_so_far = tmp_si.t;
                si = tmp_si;
            }
        }
        return hit_anything;
    }

    Primitive **m_Primitives;
    int m_NumPrimitives;
};

#endif