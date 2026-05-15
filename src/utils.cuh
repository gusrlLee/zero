#ifndef ZERO_UTILS_CUDA_HEADER
#define ZERO_UTILS_CUDA_HEADER

#include <iostream>
#include <cuda_runtime.h>

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include "stb/stb_image_write.h"
#endif

#include "color.cuh"

#define CHECK_CUDA_ERROR(val) checkCuda( (val), #val, __FILE__, __LINE__ )

void checkCuda(cudaError_t result, char const *const func, const char *const file, int const line) 
{
    if (result) {
        std::cerr << "CUDA error = " << static_cast<unsigned int>(result) << " at " <<
        file << ":" << line << " '" << func << "' \n";
        // Make sure we call CUDA Device Reset before exiting
        cudaDeviceReset();
        exit(99);
    }
}

void Download(Color* data, const char* fName, int width, int height, int channel)
{
    unsigned char* pixels = new unsigned char[width * height * 3];

    for (int i = 0; i < width * height; i++)
    {
        pixels[i * 3 + 0] = (unsigned char)data[i].rInt();
        pixels[i * 3 + 1] = (unsigned char)data[i].gInt();
        pixels[i * 3 + 2] = (unsigned char)data[i].bInt();
    }
    
    if (stbi_write_png(fName, width, height, channel, pixels, width * channel))
    {
        printf("Saved image to '%s\n", fName);
    }
    else
    {
        printf("Failed to save image to '%s'\n", fName);
    }

    delete[] pixels;
}

#endif // ZERO_UTILS_CUDA_HEADER