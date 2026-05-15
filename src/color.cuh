#ifndef ZERO_COLOR_CUDA_HEADER 
#define ZERO_COLOR_CUDA_HEADER 

#include <cuda_runtime.h>
#include "math.cuh"

class Color 
{
public:
    __host__ __device__ inline Color() : rgb(make_float3(0.0f)) {}
    __host__ __device__ inline Color(float c) : rgb(make_float3(c)) {}
    __host__ __device__ inline Color(float r, float g, float b) : rgb(make_float3(r, g, b)) {}
    __host__ __device__ inline Color(float3 v) : rgb(v) {}

    __host__ __device__ inline float r() const { return rgb.x; }
    __host__ __device__ inline float g() const { return rgb.y; }
    __host__ __device__ inline float b() const { return rgb.z; }

    __host__ __device__ inline int rInt() const { return rgb.x * 255.99; }
    __host__ __device__ inline int gInt() const { return rgb.y * 255.99; }
    __host__ __device__ inline int bInt() const { return rgb.z * 255.99; }

    __host__ __device__ inline Color operator+(const Color& c) const { return Color(rgb + c.rgb); }
    __host__ __device__ inline Color operator-(const Color& c) const { return Color(rgb - c.rgb); }
    __host__ __device__ inline Color operator*(const Color& c) const { return Color(rgb * c.rgb); } // Element-wise 곱 (Albedo * Radiance 등)
    __host__ __device__ inline Color operator*(float s) const { return Color(rgb * s); }
    __host__ __device__ inline Color operator/(float s) const { return Color(rgb / s); }

    __host__ __device__ inline Color& operator+=(const Color& c) { rgb += c.rgb; return *this; }
    __host__ __device__ inline Color& operator*=(const Color& c) { rgb *= c.rgb; return *this; }
    __host__ __device__ inline Color& operator*=(float s) { rgb *= s; return *this; }
    __host__ __device__ inline Color& operator/=(float s) { rgb /= s; return *this; }

    __host__ __device__ inline float Luminance() const 
    {
        return 0.2126f * rgb.x + 0.7152f * rgb.y + 0.0722f * rgb.z;
    }
    __host__ __device__ inline Color toSRGB() const 
    {
        float invGamma = 1.0f / 2.2f;
        return Color(powf(rgb.x, invGamma), powf(rgb.y, invGamma), powf(rgb.z, invGamma));
    }

    __host__ __device__ inline Color clamp(float minVal = 0.0f, float maxVal = 1.0f) const 
    {
        return Color(
            fmaxf(minVal, fminf(rgb.x, maxVal)),
            fmaxf(minVal, fminf(rgb.y, maxVal)),
            fmaxf(minVal, fminf(rgb.z, maxVal))
        );
    }

private:
    float3 rgb;
};

__host__ __device__ inline Color operator*(float s, const Color& c) 
{
    return c * s;
}

#endif