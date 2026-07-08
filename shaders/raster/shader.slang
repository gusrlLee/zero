// shaders/raster/shader.slang

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

// ★ CameraData 구조체 삭제 후, PushConstants에 매트릭스 직접 삽입
struct PushConstants {
    float4x4 viewProj;
    Vertex *vertices;
    uint textureIndex;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConsts;

[[vk::binding(0, 0)]] Texture2D globalTextures[];
[[vk::binding(1, 0)]] SamplerState globalSamplers[];

struct VertexOutput {
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID) {
    VertexOutput output;
    
    Vertex v = pushConsts.vertices[vertexID];
    
    // float들을 조합
    float3 pos = float3(v.px, v.py, v.pz);
    float3 norm = float3(v.nx, v.ny, v.nz);
    float2 uv = float2(v.u, v.v);

    // ★ pushConsts 안에 있는 뷰프로젝션 직접 사용
    output.position = mul(pushConsts.viewProj, float4(pos, 1.0));
    output.normal = norm;
    output.uv = uv;
    
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float4 texColor = globalTextures[pushConsts.textureIndex].Sample(globalSamplers[pushConsts.textureIndex], input.uv);
    
    // 뚫린 나뭇잎 등을 표현하기 위한 알파 테스팅
    if (texColor.a < 0.1) discard;

    float3 N = normalize(input.normal);
    float3 L = normalize(float3(1.0, 1.0, 1.0));
    float NdL = max(dot(N, L), 0.2);

    return float4(texColor.rgb * NdL, texColor.a);
}