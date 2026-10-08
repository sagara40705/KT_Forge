// CPU Matrix4と同じrow-major/row-vector。転置なし、local*World*ViewProjection。
cbuffer ViewConstants : register(b0) { row_major float4x4 viewProjection; };
cbuffer ObjectConstants : register(b1) { row_major float4x4 world; };
cbuffer MaterialConstants : register(b2) { float4 color; };
struct VertexOutput { float4 position : SV_Position; };
VertexOutput VSMain(float3 position : POSITION)
{
    VertexOutput output;
    output.position = mul(mul(float4(position,1),world),viewProjection);
    return output;
}
float4 PSMain(VertexOutput input) : SV_Target0 { return color; }
