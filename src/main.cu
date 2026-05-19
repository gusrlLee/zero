#include <iostream>
#include <time.h>
#include <cuda_runtime.h>

#include "utils.cuh"
#include "math.cuh"

#include "color.cuh"
#include "ray.cuh"
#include "camera.cuh"

#include "scene.cuh"

__device__ Color Li(const Ray &r)
{
    float3 u = normalize(r.Direction());
    float t = 0.5f * (u.y + 1.0f);
    return (1.0f - t) * make_float3(1.0, 1.0, 1.0) + t * make_float3(0.5, 0.7, 1.0);
}

__global__ void RenderKernel(Color *fb, Camera cam, int width, int height)
{
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;
    if ((i >= width) || (j >= height))
        return;

    int pIdx = (height - j - 1) * width + i;

    float u = float(i) / float(width);
    float v = float(j) / float(height);
    Ray ray = cam.generateRay(u, v);
    
    fb[pIdx] = Li(ray);
}

int main()
{
    Scene scn;
    if (!scn.Load("../assets/DamagedHelmet/glTF/DamagedHelmet.gltf"))
    {
        printf("Failed to load glTF file...\n");
        return -1;
    }
    scn.Clear();

    int width = 1200;
    int height = 600;

    int tx = 8;
    int ty = 8;

    std::cout << "Rendering a " << width << " x " << height << " image." << std::endl;
    std::cout << "Kernel size = [" << tx << " x " << ty << "] blocks." << std::endl;

    int numPixels = width * height;
    size_t fSize = numPixels * sizeof(Color);

    Color *frame;
    CHECK_CUDA_ERROR(cudaMallocManaged((void **)&frame, fSize));

    float3 lookfrom = make_float3(0.0f, 0.0f, 0.0f);
    float3 lookat = make_float3(0.0f, 0.0f, -1.0f);
    float3 vup = make_float3(0.0f, 1.0f, 0.0f);
    float vfov = 90.0f;
    float aspectRatio = float(width) / float(height);

    Camera cam = Camera(lookfrom, lookat, vup, vfov, aspectRatio);

    clock_t startTime, endTime;
    startTime = clock();

    dim3 blocksDim(width / tx + 1, height / ty + 1);
    dim3 threadsDim(tx, ty);

    RenderKernel<<<blocksDim, threadsDim>>>(frame, cam, width, height);
    CHECK_CUDA_ERROR(cudaGetLastError());
    CHECK_CUDA_ERROR(cudaDeviceSynchronize());

    endTime = clock();
    auto frameTime = ((double)(endTime - startTime)) / CLOCKS_PER_SEC;
    std::cout << "Took : " << frameTime << " secs." << std::endl;

    DownloadImage(frame, "test.png", width, height, 3);

    CHECK_CUDA_ERROR(cudaFree(frame));
    return 0;
}