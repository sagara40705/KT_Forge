// RmlUiのpremultiplied sRGB色を線形へ変換し、既存SRGB RTVへ合成する。
cbuffer UiConstants : register(b0)
{
    row_major float4x4 transform;
    float2 viewportSize;
    float2 translation;
};
Texture2D image : register(t0);
SamplerState imageSampler : register(s0);
struct Input
{
    float2 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};
struct Output
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};
float3 ToLinear(float3 value)
{
    return float3(value.r <= 0.04045 ? value.r / 12.92 : pow((value.r + 0.055) / 1.055, 2.4),
        value.g <= 0.04045 ? value.g / 12.92 : pow((value.g + 0.055) / 1.055, 2.4),
        value.b <= 0.04045 ? value.b / 12.92 : pow((value.b + 0.055) / 1.055, 2.4));
}
Output VSMain(Input input)
{
    Output output;
    float4 transformedPosition = mul(float4(input.position + translation, 0, 1), transform);
    output.position = float4(2 * transformedPosition.x / viewportSize.x - transformedPosition.w,
        transformedPosition.w - 2 * transformedPosition.y / viewportSize.y, 0, transformedPosition.w);
    output.color = float4(ToLinear(input.color.rgb / max(input.color.a, 0.000001)) * input.color.a, input.color.a);
    output.uv = input.uv;
    return output;
}
float4 PSMain(Output input) : SV_Target
{
    return input.color * image.Sample(imageSampler, input.uv);
}
