// TriangleVertexのPOSITION(float3)/COLOR(float3)と対応する専用シェーダー。
struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct PixelInput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

PixelInput VSMain(VertexInput input)
{
    PixelInput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSMain(PixelInput input) : SV_Target
{
    // 色をそのまま出力。SRGB RTVを選んだ場合のみRTVがencodeする。
    return float4(input.color, 1.0f);
}
