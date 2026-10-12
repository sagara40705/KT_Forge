// CPUと同じrow-major/row-vector。CBVの先頭は既存Unlitと共通にする。
cbuffer ViewConstants : register(b0) { row_major float4x4 viewProjection; };
cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 world;
    row_major float4x4 normalWorld;
};
cbuffer MaterialConstants : register(b2)
{
    float4 color;
    float2 uvScale;
    float2 uvOffset;
    float alphaCutoff;
    float3 padding;
    float3 shadeColor;
    float threshold;
    float smoothness;
    float receiveShadows;
    float hasTexture;
    float toonPadding;
};
cbuffer SkinConstants : register(b3) { row_major float4x4 skinMatrices[256]; };
cbuffer LightConstants : register(b4)
{
    row_major float4x4 shadowViewProjection;
    float3 lightDirection;
    float lightIntensity;
    float3 lightColor;
    float shadowEnabled;
    float3 ambient;
    float depthBias;
    float normalBias;
    float shadowStrength;
    float2 lightPadding;
};
Texture2D<float4> baseColorTexture : register(t0);
Texture2D<float> shadowTexture : register(t1);
SamplerState baseColorSampler : register(s0);

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
    float3 normal : TEXCOORD2;
};

VertexOutput FinishVertex(float4 local, float3 normal, float2 uv)
{
    VertexOutput output;
    const float4 worldPosition = mul(local, world);
    output.position = mul(worldPosition, viewProjection);
    output.worldPosition = worldPosition.xyz;
    output.normal = mul(float4(normal, 0), normalWorld).xyz;
    output.uv = uv;
    return output;
}
VertexOutput VSMain(float3 position : POSITION, float2 uv : TEXCOORD0, float3 normal : NORMAL0)
{
    return FinishVertex(float4(position, 1), normal, uv);
}

// 合成した変形行列の逆転置を法線に使う。非一様scaleにも対応し、特異点では元法線を保つ。
float3 SkinNormal(float3 normal, float3x3 matrix)
{
    const float3 first = cross(matrix[1], matrix[2]);
    const float3 second = cross(matrix[2], matrix[0]);
    const float3 third = cross(matrix[0], matrix[1]);
    const float determinant = dot(matrix[0], first);
    if (abs(determinant) < 1e-8)
    {
        return normal;
    }
    return mul(normal, float3x3(first, second, third)) / determinant;
}
VertexOutput SkinnedVSMain(float3 position : POSITION, float2 uv : TEXCOORD0, float3 normal : NORMAL0,
    uint4 joints : BLENDINDICES0, float4 weights : BLENDWEIGHT0)
{
    const float4x4 skin = skinMatrices[joints.x] * weights.x + skinMatrices[joints.y] * weights.y +
        skinMatrices[joints.z] * weights.z + skinMatrices[joints.w] * weights.w;
    return FinishVertex(mul(float4(position, 1), skin), SkinNormal(normal, (float3x3)skin), uv);
}

// Reverse-Zは受け側の深度が格納深度以上なら照らされる。PCFを9点のLoadで行う。
float ShadowVisibility(float3 position, float3 normal)
{
    if (shadowEnabled < 0.5 || receiveShadows < 0.5)
    {
        return 1;
    }
    const float4 clipPosition = mul(float4(position + normal * normalBias, 1), shadowViewProjection);
    const float3 projected = clipPosition.xyz / clipPosition.w;
    const float2 uv = projected.xy * float2(0.5, -0.5) + 0.5;
    // 隣接texelの受け面深度へ補正し、斜めの床がPCFで自分自身を影にしないようにする。
    const float2 uvDx = ddx(uv);
    const float2 uvDy = ddy(uv);
    const float depthDx = ddx(projected.z);
    const float depthDy = ddy(projected.z);
    const float determinant = uvDx.x * uvDy.y - uvDx.y * uvDy.x;
    float2 depthGradient = float2(0, 0);
    if (abs(determinant) > 1e-12)
    {
        depthGradient = float2(depthDx * uvDy.y - depthDy * uvDx.y,
            depthDy * uvDx.x - depthDx * uvDy.x) / determinant;
    }
    if (any(abs(projected.xy) > 1) || projected.z < 0 || projected.z > 1)
    {
        return 1;
    }
    uint width, height;
    shadowTexture.GetDimensions(width, height);
    const int2 pixel = int2(floor(uv * float2(width, height)));
    float visibility = 0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            const int2 samplePixel = pixel + int2(x, y);
            if (any(samplePixel < 0) || samplePixel.x >= int(width) || samplePixel.y >= int(height))
            {
                visibility += 1;
            }
            else
            {
                const float stored = shadowTexture.Load(int3(samplePixel, 0));
                const float2 sampleUv = (float2(samplePixel) + 0.5) / float2(width, height);
                const float receiverDepth = projected.z + dot(depthGradient, sampleUv - uv);
                visibility += receiverDepth + depthBias >= stored ? 1 : 0;
            }
        }
    }
    return lerp(1, visibility / 9, shadowStrength);
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    float4 albedo = color;
    if (hasTexture > 0.5)
    {
        albedo *= baseColorTexture.Sample(baseColorSampler, input.uv * uvScale + uvOffset);
    }
    if (alphaCutoff >= 0)
    {
        clip(albedo.a - alphaCutoff);
    }
    // 補間で法線が消える特異点でも有限の色を保つ。
    const float lengthSquared = dot(input.normal, input.normal);
    const float3 normal = lengthSquared > 1e-12 ? input.normal * rsqrt(lengthSquared) : float3(0, 1, 0);
    const float diffuse = saturate(dot(normal, -lightDirection));
    const float band = smoothness > 0 ? smoothstep(threshold - smoothness, threshold + smoothness, diffuse) : step(threshold, diffuse);
    const float visibility = ShadowVisibility(input.worldPosition, normal);
    const float3 direct = lerp(shadeColor, float3(1, 1, 1), band) * lightColor * lightIntensity * visibility;
    return float4(saturate(albedo.rgb * (ambient + direct)), 1);
}
