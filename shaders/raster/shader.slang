struct PushConstants {
    uint64_t bdaAddress;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pushConsts;

struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexID: SV_VertexID) {
    VertexOutput output;

    // 화면 중앙에 꽉 차는 거대한 하드코딩 삼각형
    float2 positions[3] = {
        float2(0.0, -0.5),
        float2(0.5, 0.5),
        float2(-0.5, 0.5)
    };

    float3 colors[3] = {
        float3(1.0, 0.0, 0.0), // 빨강
        float3(0.0, 1.0, 0.0), // 초록
        float3(0.0, 0.0, 1.0)  // 파랑
    };

    output.position = float4(positions[vertexID], 0.0, 1.0);
    output.color = colors[vertexID];

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    return float4(input.color, 1.0);
}