// shaders/raster/pbr.slang
// Cook-Torrance GGX 물리 기반 라이팅 (방향광 sun + 앰비언트).
// 정점은 device address로 풀링, 머티리얼은 device address 버퍼에서 인덱싱,
// 텍스처는 bindless 배열(set 0)에서 이미지 인덱스로 접근.

static const float PI = 3.14159265358979323846;

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct GpuMaterial {
    float4 baseColorFactor;
    float4 emissiveFactor;   // rgb = emissive, w = strength (미사용시 1)
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
    float4x4 viewProj;
    Vertex      *vertices;
    GpuMaterial *materials;
    float4 cameraPos;      // xyz
    float4 sunDirection;   // xyz = 빛이 진행하는 방향
    float4 sunColor;       // rgb, w = intensity
    float4 ambient;        // rgb, w = intensity
    uint   materialIndex;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConsts;

[[vk::binding(0, 0)]] Texture2D    globalTextures[];
[[vk::binding(1, 0)]] SamplerState globalSamplers[];

struct VertexOutput {
    float4 position : SV_Position;
    float3 worldPos : POSITION0;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID) {
    Vertex v = pushConsts.vertices[vertexID];

    VertexOutput o;
    float3 pos = float3(v.px, v.py, v.pz);
    o.worldPos = pos; // 정점은 로드 시 이미 월드 좌표로 베이크됨
    o.position = mul(pushConsts.viewProj, float4(pos, 1.0));
    o.normal   = float3(v.nx, v.ny, v.nz);
    o.uv       = float2(v.u, v.v);
    return o;
}

// --- bindless 텍스처 샘플 헬퍼 (인덱스 < 0 이면 기본값) ---
float4 sampleTex(int idx, float2 uv, float4 fallback) {
    if (idx < 0) return fallback;
    uint i = NonUniformResourceIndex(uint(idx));
    return globalTextures[i].Sample(globalSamplers[i], uv);
}

// --- 미세면 BRDF 항 ---
float D_GGX(float NoH, float rough) {
    float a = rough * rough;
    float a2 = a * a;
    float d = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-7);
}
float G_SchlickGGX(float NoX, float k) {
    return NoX / (NoX * (1.0 - k) + k);
}
float G_Smith(float NoV, float NoL, float rough) {
    float r = rough + 1.0;
    float k = (r * r) / 8.0; // 직접광용
    return G_SchlickGGX(NoV, k) * G_SchlickGGX(NoL, k);
}
float3 F_Schlick(float VoH, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - VoH), 5.0);
}

// 탄젠트 없이 화면공간 미분으로 노멀맵 적용
float3 perturbNormal(float3 N, float3 worldPos, float2 uv, float3 tangentNormal) {
    float3 dp1 = ddx(worldPos);
    float3 dp2 = ddy(worldPos);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    float3x3 TBN = float3x3(T * invmax, B * invmax, N);
    return normalize(mul(tangentNormal, TBN));
}

float3 ACESFilm(float3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    GpuMaterial mat = pushConsts.materials[pushConsts.materialIndex];

    // Base color (+ 알파 테스트)
    float4 baseColor = sampleTex(mat.baseColorTex, input.uv, float4(1,1,1,1)) * mat.baseColorFactor;
    if (baseColor.a < 0.5) discard;
    float3 albedo = baseColor.rgb;

    // Metallic-Roughness (glTF: B=metallic, G=roughness)
    float4 mr = sampleTex(mat.metalRoughTex, input.uv, float4(1,1,1,1));
    float metallic  = mat.metalRoughTex >= 0 ? mr.b * mat.metallic : mat.metallic;
    float roughness = mat.metalRoughTex >= 0 ? mr.g * mat.roughness : mat.roughness;
    roughness = clamp(roughness, 0.04, 1.0);

    // 노멀
    float3 N = normalize(input.normal);
    if (mat.normalTex >= 0) {
        float3 tn = sampleTex(mat.normalTex, input.uv, float4(0.5,0.5,1,1)).rgb * 2.0 - 1.0;
        N = perturbNormal(N, input.worldPos, input.uv, tn);
    }

    float3 V = normalize(pushConsts.cameraPos.xyz - input.worldPos);
    float NoV = max(dot(N, V), 1e-4);

    float3 F0 = lerp(float3(0.04), albedo, metallic);

    // --- 방향광 (sun) 직접 조명 ---
    float3 L = normalize(-pushConsts.sunDirection.xyz);
    float3 H = normalize(V + L);
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    float  D = D_GGX(NoH, roughness);
    float  G = G_Smith(NoV, NoL, roughness);
    float3 F = F_Schlick(VoH, F0);

    float3 spec = (D * G * F) / max(4.0 * NoV * NoL, 1e-4);
    float3 kd = (1.0 - F) * (1.0 - metallic);
    float3 diffuse = kd * albedo / PI;

    float3 radiance = pushConsts.sunColor.rgb * pushConsts.sunColor.w;
    float3 Lo = (diffuse + spec) * radiance * NoL;

    // --- 앰비언트 (IBL 대용) + AO ---
    float ao = sampleTex(mat.occlusionTex, input.uv, float4(1,1,1,1)).r;
    float3 ambient = pushConsts.ambient.rgb * pushConsts.ambient.w * albedo * ao;

    // --- 이미시브 ---
    float3 emissive = sampleTex(mat.emissiveTex, input.uv, float4(1,1,1,1)).rgb
                    * mat.emissiveFactor.rgb * mat.emissiveFactor.w;

    float3 color = Lo + ambient + emissive;
    color = ACESFilm(color);
    return float4(color, 1.0);
}
