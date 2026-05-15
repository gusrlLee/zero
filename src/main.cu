#include <iostream>
#include <time.h>
#include <cuda_runtime.h>

#include "utils.cuh"
#include "maths.cuh"

__global__ void Render(float3 *fb, int width, int height)
{
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;
    if((i >= width) || (j >= height)) return;
    int pIdx = (height - j - 1) * width + i;
    fb[pIdx] = make_float3( 255.99 * float(i) / width, 255.99 * float(j) / height, 255.99 * 0.2f);
}

int main()
{
    int width = 1200;
    int height = 600;

    int tx = 8;
    int ty = 8;

    std::cout << "Rendering a " << width << " x " << height << " image." << std::endl;
    std::cout << "Kernel size = [" << tx << " x " << ty << "] blocks." << std::endl;

    int numPixels = width * height;
    size_t fbSize = numPixels * sizeof(float3);

    float3 *fb;
    CHECK_CUDA_ERROR(cudaMallocManaged((void **)&fb, fbSize));

    clock_t startTime, endTime;
    startTime = clock();

    dim3 blocksDim(width / tx + 1, height / ty + 1);
    dim3 threadsDim(tx, ty);

    Render<<<blocksDim, threadsDim>>>(fb, width, height);
    CHECK_CUDA_ERROR(cudaGetLastError());
    CHECK_CUDA_ERROR(cudaDeviceSynchronize());

    endTime = clock();
    auto frameTime = ((double)(endTime - startTime)) / CLOCKS_PER_SEC;
    std::cout << "Took : " << frameTime << " secs." << std::endl;

    Download(fb, "test.png", width, height, 3);

    CHECK_CUDA_ERROR(cudaFree(fb));
    return 0;
}