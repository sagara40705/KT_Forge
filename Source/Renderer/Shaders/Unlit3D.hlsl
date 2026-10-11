// CPU Matrix4と同じrow-major/row-vector。転置なし、local*World*ViewProjection。
cbuffer ViewConstants : register(b0)
{
    row_major float4x4 viewProjection;
};
cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 world;
};
cbuffer MaterialConstants : register(b2)
{
    float4 color;
    float2 uvScale;
    float2 uvOffset;
    float alphaCutoff;
    float3 padding;
};
Texture2D<float4> baseColorTexture : register(t0);
SamplerState baseColorSampler : register(s0);

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
VertexOutput VSMain(float3 position : POSITION, float2 uv : TEXCOORD0)
{
    VertexOutput output;
    output.position = mul(mul(float4(position, 1), world), viewProjection);
    output.uv = uv;
    return output;
}

// Opaque/Maskは合成せず、残る画素を不透明として出力する。
float4 FinishUnlit(float4 value)
{
    if (alphaCutoff >= 0)
    {
        clip(value.a - alphaCutoff);
    }
    return float4(value.rgb, 1);
}
float4 PSMain(VertexOutput input) : SV_Target0
{
    return FinishUnlit(color);
}
float4 TexturedPSMain(VertexOutput input) : SV_Target0
{
    return FinishUnlit(baseColorTexture.Sample(baseColorSampler, input.uv * uvScale + uvOffset) * color);
}
