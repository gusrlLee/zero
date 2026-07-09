// shaders/pathtracing/fullscreen.slang
// 누적 HDR 이미지를 톤맵하여 스왑체인에 출력하는 풀스크린 삼각형 패스.

struct VSOut {
    float4 pos : SV_Position;
};

[shader("vertex")]
VSOut vertexMain(uint id : SV_VertexID) {
    VSOut o;
    float2 uv = float2((id << 1) & 2, id & 2); // (0,0),(2,0),(0,2)
    o.pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

[[vk::binding(0, 0)]] Texture2D<float4> accumTex;

float3 ACESFilm(float3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
float4 fragmentMain(float4 pos : SV_Position) : SV_Target {
    int2 coord = int2(pos.xy);
    float3 hdr = accumTex.Load(int3(coord, 0)).rgb; // 픽셀 1:1 대응이라 texelFetch
    return float4(ACESFilm(hdr), 1.0);
}
