// shaders/pathtracing/pathtrace.slang
// 하드웨어 레이트레이싱(ray query) 기반 진행형 패스트레이서.
// Cook-Torrance GGX BRDF + Next Event Estimation(태양) + Multiple Importance Sampling + Russian roulette.

static const float PI = 3.14159265358979323846;

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct GpuMaterial {
    float4 baseColorFactor;
    float4 emissiveFactor;   // rgb, w = strength
    float  metallic;
    float  roughness;
    int    baseColorTex;
    int    metalRoughTex;
    int    normalTex;
    int    emissiveTex;
    int    occlusionTex;
    int    _pad;
};

struct PushConstants {
    float4x4 invViewProj;
    float4 cameraPos;
    float4 sunDirection;    // xyz = 빛 진행 방향
    float4 sunColor;        // rgb, w = intensity
    float4 ambient;         // rgb, w = intensity (환경광)
    Vertex      *vertices;
    uint        *indices;
    GpuMaterial *materials;
    uint        *triMaterials;
    uint  frameIndex;
    uint  width;
    uint  height;
    uint  maxBounces;
    float sunAngularRadius;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

// set 0 : bindless 텍스처 (래스터와 공유)
[[vk::binding(0, 0)]] Texture2D    globalTextures[];
[[vk::binding(1, 0)]] SamplerState globalSamplers[];

// set 1 : TLAS + 누적 이미지
[[vk::binding(0, 1)]] RaytracingAccelerationStructure tlas;
[[vk::binding(1, 1)]] RWTexture2D<float4> accumImage;

// ---------------- 유틸 ----------------
uint pcgHash(inout uint state) {
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
float rnd(inout uint state) { return (pcgHash(state) & 0xFFFFFFu) / float(0x1000000); }

float luminance(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

float3 tangentToWorld(float3 v, float3 N) {
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    return v.x * T + v.y * B + v.z * N;
}

float4 sampleTex(int idx, float2 uv, float4 fallback) {
    if (idx < 0) return fallback;
    uint i = NonUniformResourceIndex(uint(idx));
    return globalTextures[i].SampleLevel(globalSamplers[i], uv, 0.0);
}

float misWeight(float a, float b) { return (a * a) / (a * a + b * b); } // power heuristic

// ---------------- 미세면 BRDF ----------------
float D_GGX(float NoH, float rough) {
    float a = rough * rough;
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-7);
}
float G_SchlickGGX(float NoX, float k) { return NoX / (NoX * (1.0 - k) + k); }
float G_Smith(float NoV, float NoL, float rough) {
    float r = rough + 1.0;
    float k = (r * r) / 8.0;
    return G_SchlickGGX(NoV, k) * G_SchlickGGX(NoL, k);
}
float3 F_Schlick(float VoH, float3 F0) { return F0 + (1.0 - F0) * pow(saturate(1.0 - VoH), 5.0); }

// 주어진 방향 L에 대한 BRDF 값과 pdf
float3 evalBRDF(float3 N, float3 V, float3 L, float3 albedo, float metallic, float rough, out float pdf) {
    pdf = 0.0;
    float NoL = dot(N, L);
    float NoV = dot(N, V);
    if (NoL <= 0.0 || NoV <= 0.0) return float3(0.0);

    float3 H = normalize(V + L);
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    float3 F0 = lerp(float3(0.04), albedo, metallic);
    float  D = D_GGX(NoH, rough);
    float  G = G_Smith(NoV, NoL, rough);
    float3 F = F_Schlick(VoH, F0);

    float3 spec = (D * G * F) / max(4.0 * NoV * NoL, 1e-4);
    float3 diff = (1.0 - metallic) * albedo / PI;
    float3 f = diff + spec;

    float pSpec = clamp(luminance(F0), 0.1, 0.9);
    float pdfSpec = D * NoH / max(4.0 * VoH, 1e-4);
    float pdfDiff = NoL / PI;
    pdf = pSpec * pdfSpec + (1.0 - pSpec) * pdfDiff;
    return f;
}

// BRDF 중요도 표본추출
void sampleBRDF(float3 N, float3 V, float3 albedo, float metallic, float rough,
                inout uint seed, out float3 wi, out float3 f, out float pdf) {
    float3 F0 = lerp(float3(0.04), albedo, metallic);
    float pSpec = clamp(luminance(F0), 0.1, 0.9);

    if (rnd(seed) < pSpec) {
        // GGX NDF 표본추출
        float a = rough * rough;
        float u1 = rnd(seed), u2 = rnd(seed);
        float phi = 2.0 * PI * u1;
        float cosT = sqrt((1.0 - u2) / (1.0 + (a * a - 1.0) * u2));
        float sinT = sqrt(saturate(1.0 - cosT * cosT));
        float3 Hl = float3(cos(phi) * sinT, sin(phi) * sinT, cosT);
        float3 H = tangentToWorld(Hl, N);
        wi = reflect(-V, H);
    } else {
        // 코사인 가중 반구
        float u1 = rnd(seed), u2 = rnd(seed);
        float r = sqrt(u1);
        float phi = 2.0 * PI * u2;
        float3 Ll = float3(r * cos(phi), r * sin(phi), sqrt(saturate(1.0 - u1)));
        wi = tangentToWorld(Ll, N);
    }
    f = evalBRDF(N, V, wi, albedo, metallic, rough, pdf);
}

// 원뿔(태양 원반) 내 균일 표본추출
float3 sampleCone(float3 dir, float cosThetaMax, inout uint seed, out float pdf) {
    float u1 = rnd(seed), u2 = rnd(seed);
    float cosTheta = (1.0 - u1) + u1 * cosThetaMax;
    float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
    float phi = 2.0 * PI * u2;
    float3 local = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    pdf = 1.0 / (2.0 * PI * (1.0 - cosThetaMax));
    return tangentToWorld(local, dir);
}

// ---------------- 레이 질의 ----------------
bool traceClosest(float3 origin, float3 dir, out uint primIndex, out float2 bary) {
    primIndex = 0;
    bary = float2(0.0);
    RayDesc ray;
    ray.Origin = origin; ray.Direction = dir; ray.TMin = 1e-3; ray.TMax = 1e30;
    RayQuery<RAY_FLAG_NONE> q;
    q.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray);
    while (q.Proceed()) {}
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        primIndex = q.CommittedPrimitiveIndex();
        bary = q.CommittedTriangleBarycentrics();
        return true;
    }
    return false;
}

bool occluded(float3 origin, float3 dir, float tmax) {
    RayDesc ray;
    ray.Origin = origin; ray.Direction = dir; ray.TMin = 1e-3; ray.TMax = tmax;
    RayQuery<RAY_FLAG_NONE> q;
    q.TraceRayInline(tlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, ray);
    q.Proceed();
    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

struct Surface {
    float3 pos;
    float3 normal;
    float3 albedo;
    float  metallic;
    float  roughness;
    float3 emissive;
};

Surface getSurface(uint primIndex, float2 bary, float3 rayDir) {
    uint i0 = pc.indices[3 * primIndex + 0];
    uint i1 = pc.indices[3 * primIndex + 1];
    uint i2 = pc.indices[3 * primIndex + 2];
    Vertex v0 = pc.vertices[i0];
    Vertex v1 = pc.vertices[i1];
    Vertex v2 = pc.vertices[i2];

    float w0 = 1.0 - bary.x - bary.y, w1 = bary.x, w2 = bary.y;
    float3 p0 = float3(v0.px, v0.py, v0.pz), p1 = float3(v1.px, v1.py, v1.pz), p2 = float3(v2.px, v2.py, v2.pz);
    float3 n0 = float3(v0.nx, v0.ny, v0.nz), n1 = float3(v1.nx, v1.ny, v1.nz), n2 = float3(v2.nx, v2.ny, v2.nz);
    float2 t0 = float2(v0.u, v0.v), t1 = float2(v1.u, v1.v), t2 = float2(v2.u, v2.v);

    Surface s;
    s.pos = w0 * p0 + w1 * p1 + w2 * p2;
    float3 n = normalize(w0 * n0 + w1 * n1 + w2 * n2);
    if (dot(n, rayDir) > 0.0) n = -n; // 뷰 쪽을 향하도록
    s.normal = n;
    float2 uv = w0 * t0 + w1 * t1 + w2 * t2;

    GpuMaterial mat = pc.materials[pc.triMaterials[primIndex]];
    float4 bc = sampleTex(mat.baseColorTex, uv, float4(1,1,1,1)) * mat.baseColorFactor;
    s.albedo = bc.rgb;
    float4 mr = sampleTex(mat.metalRoughTex, uv, float4(1,1,1,1));
    s.metallic = mat.metalRoughTex >= 0 ? mr.b * mat.metallic : mat.metallic;
    s.roughness = clamp(mat.metalRoughTex >= 0 ? mr.g * mat.roughness : mat.roughness, 0.04, 1.0);
    s.emissive = sampleTex(mat.emissiveTex, uv, float4(1,1,1,1)).rgb * mat.emissiveFactor.rgb * mat.emissiveFactor.w;
    return s;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID) {
    if (tid.x >= pc.width || tid.y >= pc.height) return;

    uint seed = (tid.x * 1973u) ^ (tid.y * 9277u) ^ (pc.frameIndex * 26699u);
    seed |= 1u;

    // 카메라 레이 (픽셀 지터로 안티에일리어싱)
    float2 jitter = float2(rnd(seed), rnd(seed)) - 0.5;
    float2 pixel = float2(tid.xy) + 0.5 + jitter;
    float2 ndc = pixel / float2(pc.width, pc.height) * 2.0 - 1.0;
    float4 farP = mul(pc.invViewProj, float4(ndc, 1.0, 1.0));
    float3 origin = pc.cameraPos.xyz;
    float3 dir = normalize(farP.xyz / farP.w - origin);

    float3 Lsun = normalize(-pc.sunDirection.xyz);
    float cosThetaMax = cos(pc.sunAngularRadius);
    float solidAngle = 2.0 * PI * (1.0 - cosThetaMax);
    // sunColor*intensity는 표면에서의 irradiance(E)로 해석 → 원반 radiance = E / solidAngle.
    // 이러면 NEE 기여가 f*NoL*E가 되어 래스터라이저와 밝기가 일치한다.
    float3 sunRadiance = (pc.sunColor.rgb * pc.sunColor.w) / max(solidAngle, 1e-6);
    float3 skyColor = pc.ambient.rgb * pc.ambient.w;

    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);

    for (uint bounce = 0; bounce < pc.maxBounces; ++bounce) {
        uint primIndex; float2 bary;
        if (!traceClosest(origin, dir, primIndex, bary)) {
            radiance += throughput * skyColor; // 환경광 (miss)
            break;
        }

        Surface s = getSurface(primIndex, bary, dir);
        float3 N = s.normal;
        float3 V = -dir;

        radiance += throughput * s.emissive;

        // --- NEE: 태양(light) 표본추출 + MIS ---
        {
            float pdfL;
            float3 wi = sampleCone(Lsun, cosThetaMax, seed, pdfL);
            float NoL = dot(N, wi);
            if (NoL > 0.0) {
                float pdfB;
                float3 f = evalBRDF(N, V, wi, s.albedo, s.metallic, s.roughness, pdfB);
                if (pdfB > 0.0 && !occluded(s.pos + N * 1e-3, wi, 1e30)) {
                    float w = misWeight(pdfL, pdfB);
                    radiance += throughput * f * NoL * sunRadiance / pdfL * w;
                }
            }
        }

        // --- BRDF 표본추출 (연속 경로 + 태양 MIS) ---
        float3 wi; float3 f; float pdfB;
        sampleBRDF(N, V, s.albedo, s.metallic, s.roughness, seed, wi, f, pdfB);
        float NoL = dot(N, wi);
        if (pdfB <= 0.0 || NoL <= 0.0) break;

        // BRDF 표본이 태양 원반을 향하면 MIS로 태양 기여 추가
        if (dot(wi, Lsun) >= cosThetaMax) {
            if (!occluded(s.pos + N * 1e-3, wi, 1e30)) {
                float pdfL = 1.0 / solidAngle;
                float w = misWeight(pdfB, pdfL);
                radiance += throughput * f * NoL * sunRadiance / pdfB * w;
            }
        }

        throughput *= f * NoL / pdfB;
        origin = s.pos + N * 1e-3;
        dir = wi;

        // Russian roulette
        if (bounce > 3) {
            float p = clamp(max(throughput.x, max(throughput.y, throughput.z)), 0.05, 1.0);
            if (rnd(seed) > p) break;
            throughput /= p;
        }
    }

    // 진행형 누적 (러닝 평균)
    int2 coord = int2(tid.xy);
    float3 prev = pc.frameIndex == 0u ? float3(0.0) : accumImage[coord].rgb;
    float3 result = (prev * float(pc.frameIndex) + radiance) / float(pc.frameIndex + 1u);
    accumImage[coord] = float4(result, 1.0);
}
