struct CameraData {
    float4x4 viewProj;
};

// C++의 Vertex 구조체와 완벽하게 동일해야 함
struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

// C++에서 매 프레임 쏴줄 16바이트 (주소 2개)
struct PushConstants {
    CameraData* camera;
    Vertex* vertices; // ★ BDA: 정점 버퍼의 메모리 주소!
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConsts;

struct VertexOutput {
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID) {
    VertexOutput output;
    
    Vertex v = pushConsts.vertices[vertexID];

    output.position = mul(pushConsts.camera.viewProj, float4(v.position, 1.0));
    output.normal = v.normal;
    output.uv = v.uv;
    
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float3 N = normalize(input.normal);
    float3 L = normalize(float3(1.0, 1.0, 1.0)); // 우측 상단에서 빛이 온다고 가정
    float NdL = max(dot(N, L), 0.2);             // 최소한의 밝기(Ambient) 0.2 유지

    float3 baseColor = float3(0.2, 0.6, 1.0);    // 예쁜 파란색 모델
    float3 finalColor = baseColor * NdL;
    
    return float4(finalColor, 1.0);
}