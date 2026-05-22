// common include 
#include <iostream>
#include <vector>
#include <filesystem>
#include <time.h>
#include <string>

#include <cuda_runtime.h>
#include <curand_kernel.h>

// library include 
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

// my math header 
#include "math.cuh"

#define PI 3.1415926535897932385
#define INV_PI 0.31830988618379067154

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

// Structure 
///////////////////////////////////////////////////////// 
struct Ray 
{
    float3 orig, dir;
    __host__ __device__ Ray() {}
    __host__ __device__ Ray(const float3 &o, const float3 &d) : orig(o), dir(d) {}
    __host__ __device__ float3 at(float t) const { return orig + t * dir; }
};

struct Camera
{
    float3 origin;
    float3 lower_left_corner;
    float3 horizontal;
    float3 vertical;

    __host__ __device__ Camera() {}
    __host__ __device__ Camera(float3 lookfrom, float3 lookat, float3 vup, float vfov, float aspectratio)
    {
        float theta = vfov * (PI / 180.0f);
        float h = tan(theta / 2.0f);

        float vHight = 2.0f * h;
        float vWidth = aspectratio * vHight;

        float3 w = normalize(lookfrom - lookat);
        float3 u = normalize(cross(vup, w));
        float3 v = cross(w, u);

        origin = lookfrom;
        horizontal = vWidth * u;
        vertical = vHight * v;

        lower_left_corner = origin - horizontal / 2.0f - vertical / 2.0f - w;
    }
};

struct SurfaceInteraction
{
    uint32_t intsersected_triangle_index;
    float t;       // t-value of intersection
    float3 p;        // intersected point
    float3 n;        // normal vector of intersected point
    uint32_t material_index; // material index
};

struct Triangle 
{
    float3 v0, v1, v2;
    float3 e1, e2, n;
    uint32_t material_index;
    bool is_light = false;
    
    __host__ __device__ Triangle() {}
    __host__ __device__ Triangle(float3 vertex0, float3 vertex1, float3 vertex2) : v0(vertex0), v1(vertex1), v2(vertex2) 
    {
        e1 = v1 - v0;
        e2 = v2 - v0;
        n = normalize(cross(e1, e2));
    }

    __host__ __device__ float3 normal() const { return n; }
    __host__ __device__ float3 edge1() const { return e1; }
    __host__ __device__ float3 edge2() const { return e2; }
    __host__ __device__ float3 centroid() const { return (v0 + v1 + v2) / 3.0f; }
    __host__ __device__ float area() const { return length(cross(e1, e2)) / 2.0f; }

    __host__ __device__ bool hit(const Ray &r, float tMin, float tMax, SurfaceInteraction &si) const 
    {
        float3 v0v1 = v1 - v0;
        float3 v0v2 = v2 - v0;
        float3 pvec = cross(r.dir, v0v2);

        float det = dot(v0v1, pvec);

        if (fabs(det) < 1e-8f)
            return false;
        float invDet = 1.0f / det;

        float3 tvec = r.orig - v0;
        float u = dot(tvec, pvec) * invDet;
        if (u < 0.0f || u > 1.0f)
            return false;

        float3 qvec = cross(tvec, v0v1);
        float v = dot(r.dir, qvec) * invDet;
        if (v < 0.0f || u + v > 1.0f)
            return false;

        float t = dot(v0v2, qvec) * invDet;

        if (t < tMax && t > tMin)
        {
            si.t = t;
            si.p = r.at(t);
            si.n = dot(r.dir, n) < 0.0f ? n : make_float3(-n.x, -n.y, -n.z);
            si.material_index = material_index;
            return true;
        }

        return false;
    }

    __device__ void onSample(float3 sample, float &pdf, curandState *rand_state)
    {
        float triangle_area = area();
        pdf = 1.0f / triangle_area;

    }

    __device__ float pdf()
    {
        return 1.0f / area();
    }
};

enum MaterialType
{
    eLAMBERTIAN,
    eDIELECTRIC,
    eSPECULAR,
    eDIFFUSE_LIGHT,
};

struct Material
{
    MaterialType type;
    float3 albedo, emission;
    __host__ __device__ Material() : albedo(make_float3(0, 0, 0)), emission(make_float3(0, 0, 0)) {}
};

struct Scene 
{
    std::vector<Triangle> triangles;
    std::vector<Material> materials;
    std::vector<Triangle> lights;
};

__global__ void initRandState(int width, int height, curandState *rand_state)
{
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;

    if ((i >= width) || (j >= height))
        return;

    int pIdx = j * width + i;
    curand_init(1984 + pIdx, 0, 0, &rand_state[pIdx]);
}

__device__ float3 randomInUnitSphere(curandState *localRandState) {
    float3 p;
    do {
        // -1.0 ~ 1.0 사이의 랜덤 값 추출
        float r1 = curand_uniform(localRandState);
        float r2 = curand_uniform(localRandState);
        float r3 = curand_uniform(localRandState);
        p = 2.0f * make_float3(r1, r2, r3) - make_float3(1.0f, 1.0f, 1.0f);
    } while (dot(p, p) >= 1.0f); 
    return p;
}

Scene loadObjFile(const std::string filename)
{
    Scene scn;
    std::filesystem::path objPath(filename.c_str());
    if (!std::filesystem::exists(objPath))
    {
        std::cerr << "[Error]: File not found - " << filename << std::endl;
    }

    std::string objFp = objPath.string();
    std::string mtlDir = objPath.parent_path().string() + "/";

    tinyobj::ObjReaderConfig cfg;
    cfg.mtl_search_path = mtlDir;
    cfg.triangulate = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filename, cfg))
    {
        if (!reader.Error().empty())
        {
            std::cerr << "TinyObjReader Error: " << reader.Error() << std::endl;
        }
    }

    if (!reader.Warning().empty())
    {
        std::cerr << "TinyObjReader Warning: " << reader.Warning() << std::endl;
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &materials = reader.GetMaterials();

    for (const auto &mat : materials)
    {
        Material m;

        float light_intensity = mat.emission[0] + mat.emission[1] + mat.emission[2];
        if (light_intensity > 0.0)
        {
            m.type = eDIFFUSE_LIGHT;
        }
        else
        {
            m.type = eLAMBERTIAN;
        }

        m.albedo = make_float3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
        m.emission = make_float3(mat.emission[0], mat.emission[1], mat.emission[2]);
        scn.materials.push_back(m);
    }

    if (scn.materials.empty())
    {
        std::cerr << "Materials of scene is empty." << std::endl;
    }

    for (const auto &shape : shapes)
    {
        size_t idxOffset = 0;
        for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); face++)
        {
            int fv = shape.mesh.num_face_vertices[face];

            float3 vertices[3];

            for (int i = 0; i < 3; i++)
            {
                tinyobj::index_t idx = shape.mesh.indices[idxOffset + i];

                // get vertices info 
                vertices[i] = make_float3(
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]);
            }

            int matId = shape.mesh.material_ids[face];
            if (matId < 0 || matId >= scn.materials.size())
            {
                matId = 0;
            }

            float light_intensity = materials[matId].emission[0] + materials[matId].emission[1] + materials[matId].emission[2];

            Triangle tri;
            if (light_intensity > 0.0f)
            {
                tri = Triangle(vertices[0], vertices[1], vertices[2]);
                tri.material_index = matId;
                tri.is_light = true;
                scn.triangles.push_back(tri);
                scn.lights.push_back(tri);
            }
            else
            {
                tri = Triangle(vertices[0], vertices[1], vertices[2]);
                tri.material_index = matId;
                tri.is_light = false;
                scn.triangles.push_back(tri);
            }
            
#if 0 // for tiny obj loader check
            printf("material index = %d\n", matId);
            printf("material illum idx = %f %f %f\n", 
                materials[matId].diffuse[0],
                materials[matId].diffuse[1],
                materials[matId].diffuse[2]
            );
            printf("material emissive idx = %f %f %f\n", 
                materials[matId].emission[0],
                materials[matId].emission[1],
                materials[matId].emission[2]
            );
            printf("material metallic idx = %f\n", materials[matId].metallic);
            printf("material roughness idx = %f\n", materials[matId].roughness);
            printf("%f %f %f\n", materials[matId].specular[0], materials[matId].specular[1], materials[matId].specular[2]);
#endif

            idxOffset += fv;
        }
    }

    std::cout << "scn triangle = " << scn.triangles.size() << std::endl;
    std::cout << "scn materials = " << scn.materials.size() << std::endl;
    std::cout << "scn lights = " << scn.lights.size() << std::endl;

    return scn;
}

__device__ inline float powerHeuristic(float a, float b)
{
    float a2 = a * a;
    float b2 = b * b;
    return a2 / (a2 + b2 + 1e-5f);
}

__device__ bool traceShadowRay(
    const Ray& r, 
    SurfaceInteraction &si,
    Triangle *triangle_list, 
    int num_triangles,
    float tmax, 
    float tmin
)
{
    float closest_so_far = tmax;

    for (int i = 0; i < num_triangles; i++)
    {
        if (triangle_list[i].hit(r, tmin, closest_so_far, si))
        {
            closest_so_far = si.t;
            return true; // this is shadow ray 
        }
    }
    return false; // this is not shadow ray 
}

__device__ bool traceRay(
    const Ray& r, 
    SurfaceInteraction &si,
    Triangle *triangles, 
    int num_triangles,
    float tmax, 
    float tmin
)
{
    SurfaceInteraction temp_si;
    float anyHit = false;
    float closest_so_far = tmax;

    for ( int i = 0; i < num_triangles; i++ )
    {
        if (triangles[i].hit(r, tmin, closest_so_far, temp_si))
        {
            anyHit = true;
            closest_so_far = temp_si.t;
            si = temp_si;
            si.intsersected_triangle_index = i;
        }
    }

    return anyHit;
}

__device__ float3 Li(
    const Ray &r,
    int max_depth,
    Triangle *triangles, int num_triangles,
    Material *materials, int num_materials,
    Triangle *lights, int num_lights,
    curandState *rand_state
)
{
    float3 color = make_float3(0.0f, 0.0f, 0.0f);
    float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
    Ray ray = r;

    float prev_bsdf_pdf = 0.0f;
    bool is_specular_bounce = true;

    for (int depth = 0; depth < max_depth; depth++)
    {
        SurfaceInteraction si;
        if (!traceRay(ray, si, triangles, num_triangles, 1000.0f, 0.00001f))
        {
            return color;
        }

        Triangle &triangle = triangles[si.intsersected_triangle_index];
        Material &material = materials[triangle.material_index];

        color += throughput * material.emission;

        if (triangle.is_light)
        {
            if (is_specular_bounce)
            {
                color += throughput * material.emission;
            }
            else
            {
                float light_dist_sq = si.t * si.t;
                float cos_light = fabs(dot(si.n, -ray.dir));

                float light_area_pdf = 1.0f / (num_lights * triangle.area());
                float light_pdf_w = light_area_pdf * light_dist_sq / cos_light;
                float w = powerHeuristic(prev_bsdf_pdf, light_pdf_w);
                color += throughput * material.emission * w;
            }

            break;
        }

        if (num_lights > 0)
        {
            int light_index = curand(rand_state) % num_lights;
            Triangle &light_tri = lights[light_index];
            Material &light_mat = materials[light_tri.material_index];

            float r1 = curand_uniform(rand_state);
            float r2 = curand_uniform(rand_state);

            float sqrt_r1 = sqrtf(r1);
            float u_bary = 1.0f - sqrt_r1;
            float v_bary = r2 * sqrt_r1;
            float w_bary = 1.0f - u_bary - v_bary;

            float3 light_p = u_bary * light_tri.v0 + v_bary * light_tri.v1 + w_bary * light_tri.v2;
            float3 light_n = light_tri.n; // 광원의 법선

            float3 light_dir = light_p - si.p;
            float light_dist_sq = dot(light_dir, light_dir);
            float light_dist = sqrtf(light_dist_sq);
            light_dir = normalize(light_dir);

            float cos_light = dot(light_n, -light_dir); // 빛 표면의 코사인 각
            float cos_surface = dot(si.n, light_dir);   // 현재 표면의 코사인 각

            // 빛의 앞면이고 현재 표면의 위쪽으로 빛이 들어올 때만 계산
            if (cos_light > 0.001f && cos_surface > 0.001f)
            {
                SurfaceInteraction shadow_si;
                Ray shadow_ray(si.p, light_dir);
                
                // 빛까지 가려짐이 없는지 검사 (light_dist 보다 조금 작게 해서 Self-intersection 방지)
                if (!traceShadowRay(shadow_ray, shadow_si, triangles, num_triangles, light_dist - 0.001f, 0.001f))
                {
                    float light_area_pdf = 1.0f / (num_lights * light_tri.area());
                    float light_pdf_w = light_area_pdf * light_dist_sq / cos_light;
                    
                    float bsdf_pdf_w = cos_surface * INV_PI; // Lambertian PDF
                    float weight_light = powerHeuristic(light_pdf_w, bsdf_pdf_w);
                    
                    float3 f_cos = material.albedo * INV_PI * cos_surface; // BRDF * cos(theta)
                    
                    // L += f_cos * emission * MIS_weight / pdf
                    color += throughput * f_cos * light_mat.emission * weight_light / light_pdf_w;
                }
            }
        }

        float3 w = si.n;
        float3 a = (fabs(w.x) > 0.9f) ? make_float3(0, 1, 0) : make_float3(1, 0, 0);
        float3 v_vec = normalize(cross(w, a));
        float3 u_vec = cross(w, v_vec);

        // Cosine 분포 기반 방향 샘플링
        float r1 = curand_uniform(rand_state);
        float r2 = curand_uniform(rand_state);
        float r_radius = sqrtf(r1);
        float phi = 2.0f * PI * r2;
        float local_x = r_radius * cosf(phi);
        float local_y = r_radius * sinf(phi);
        float local_z = sqrtf(fmaxf(0.0f, 1.0f - r1)); // cos(theta)와 동일

        float3 new_ray_direction = normalize(local_x * u_vec + local_y * v_vec + local_z * w);

        // 다음 루프의 MIS 계산을 위해 현재 생성한 광선의 PDF 값 갱신
        prev_bsdf_pdf = local_z * INV_PI; // cos(theta) / PI
        is_specular_bounce = false;       // 난반사를 했으므로 false로 변경

        // Lambertian 특성상 (Albedo / PI) * cos_theta / (cos_theta / PI) = Albedo 만 남음
        throughput = throughput * material.albedo; 
        ray = Ray(si.p, new_ray_direction);
    }
    return color;
}

__global__ void PathTracer(
    float3 *frame_buffer,
    int width, 
    int height, 
    int spp,
    Camera cam,
    Triangle *triangles, int num_triangles,
    Material *materials, int num_materials,
    Triangle *lights, int num_lights,
    curandState *rand_state
)
{
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;
    if ((i >= width) || (j >= height))
        return;

    int pixel_index = (height - j - 1) * width + i;
    float u = float(i) / float(width);
    float v = float(j) / float(height);
    curandState local_rand_state = rand_state[pixel_index];

    float3 result = make_float3(0.0, 0.0, 0.0);

    for (int s = 0; s < spp; s++)
    {
        float u = (float(i) + curand_uniform(&local_rand_state)) / float(width);
        float v = (float(j) + curand_uniform(&local_rand_state)) / float(height);

        Ray ray = Ray(cam.origin, normalize(cam.lower_left_corner + u * cam.horizontal + v * cam.vertical - cam.origin));    
        result += Li(ray, 5, triangles, num_triangles, materials, num_materials, lights, num_lights, &local_rand_state);
    }

    result /= float(spp);
    rand_state[pixel_index] = local_rand_state;
    frame_buffer[pixel_index] = result;
}

int main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        printf("Please input spp ex) ./zero.exe 16\n");
        return -1;
    }

    int width = 1000;
    int height = 1000;

    int tx = 8;
    int ty = 8;
    int num_pixels = width * height;
    int spp = atoi(argv[1]);

    Scene scn = loadObjFile("../assets/CornellBox/CornellBox-Original.obj");

    float aspect_ratio = float(width) / float(height);
    Camera cam(make_float3(0.0f, 1.0f, 4.0f), make_float3(0.0f, 1.0f, 0.0f), make_float3(0.0f, 1.0f, 0.0f), 40.0f, aspect_ratio);

    int triangles_list_size = scn.triangles.size();
    int lights_list_size = scn.lights.size();
    int materials_list_size = scn.materials.size();
    size_t triangle_list_memory_size = triangles_list_size * sizeof(Triangle);
    size_t material_list_memory_size = materials_list_size * sizeof(Material);
    size_t lights_list_memory_size = lights_list_size * sizeof(Triangle);
    size_t frame_buffer_memory_size = width * height * sizeof(float3);
    size_t rand_state_memory_size = width * height * sizeof(curandState);

    Triangle *device_triangles;
    Material *device_materials;
    Triangle *device_lights;
    float3 *device_frame_buffer;
    curandState *device_rand_state;

    CHECK_CUDA_ERROR(cudaMalloc((void **)&device_triangles, triangle_list_memory_size));
    CHECK_CUDA_ERROR(cudaMalloc((void **)&device_materials, material_list_memory_size));
    CHECK_CUDA_ERROR(cudaMalloc((void **)&device_lights, lights_list_memory_size));
    CHECK_CUDA_ERROR(cudaMalloc((void **)&device_frame_buffer, frame_buffer_memory_size));
    CHECK_CUDA_ERROR(cudaMalloc((void **)&device_rand_state, rand_state_memory_size));

    CHECK_CUDA_ERROR(cudaMemcpy(device_triangles, scn.triangles.data(), triangle_list_memory_size, cudaMemcpyHostToDevice));
    CHECK_CUDA_ERROR(cudaMemcpy(device_lights, scn.lights.data(), lights_list_memory_size, cudaMemcpyHostToDevice));
    CHECK_CUDA_ERROR(cudaMemcpy(device_materials, scn.materials.data(), material_list_memory_size, cudaMemcpyHostToDevice));

    dim3 blocks(width / tx + 1, height / ty + 1);
    dim3 threads(tx, ty);

    initRandState<<<blocks, threads>>>(width, height, device_rand_state);
    cudaDeviceSynchronize();

    PathTracer<<<blocks, threads>>>(device_frame_buffer, width, height, spp, cam, device_triangles, triangles_list_size, device_materials, materials_list_size, device_lights, lights_list_size, device_rand_state);
    cudaDeviceSynchronize();

    std::vector<float3> frame(width * height);
    CHECK_CUDA_ERROR(cudaMemcpy(frame.data(), device_frame_buffer, frame_buffer_memory_size, cudaMemcpyDeviceToHost));
    cudaDeviceSynchronize();
    
    std::vector<unsigned char> image(width * height * 3);
    for (int i = 0; i < width * height; ++i)
    {
        float3 pixel = frame[i];

        float r = sqrt(pixel.x);
        float g = sqrt(pixel.y);
        float b = sqrt(pixel.z);

        // Gamma Correction
        int ir = int(255.99f * (r > 1.0f ? 1.0f : (r < 0.0f ? 0.0f : r)));
        int ig = int(255.99f * (g > 1.0f ? 1.0f : (g < 0.0f ? 0.0f : g)));
        int ib = int(255.99f * (b > 1.0f ? 1.0f : (b < 0.0f ? 0.0f : b)));

        image[i * 3 + 0] = static_cast<unsigned char>(ir);
        image[i * 3 + 1] = static_cast<unsigned char>(ig);
        image[i * 3 + 2] = static_cast<unsigned char>(ib);
    }

    if (stbi_write_png("results.png", width, height, 3, image.data(), width * 3))
    {
        std::cout << "[Info] Image saved successfully: results.png" << std::endl;
    }
    else
    {
        std::cerr << "[Error] Failed to save image!" << std::endl;
    }

    CHECK_CUDA_ERROR(cudaFree(device_frame_buffer));
    CHECK_CUDA_ERROR(cudaFree(device_rand_state));
    CHECK_CUDA_ERROR(cudaFree(device_triangles));
    CHECK_CUDA_ERROR(cudaFree(device_materials));

    return 0;
}