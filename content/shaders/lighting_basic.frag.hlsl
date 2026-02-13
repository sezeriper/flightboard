Texture2D AlbedoTexture : register(t0, space2);
SamplerState AlbedoSampler : register(s0, space2);

struct PixelInput
{
    float4 Position : SV_Position;
    float3 Normal : TEXCOORD0;
    float3 Color : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

float4 main(PixelInput input) : SV_Target0
{
    float4 albedo = AlbedoTexture.Sample(AlbedoSampler, input.UV);

    float3 normal = normalize(input.Normal);
    // Hard-coded light direction (pointing towards the light, i.e., top-right-front)
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float3 lightColor = float3(1.0, 1.0, 1.0);

    // Ambient component
    float ambientStrength = 0.2;
    float3 ambient = ambientStrength * lightColor;

    // Diffuse component
    float3 diffuse = max(dot(normal, lightDir), 0.0f) * lightColor;

    // Combine
    float3 result = (ambient + diffuse) * albedo.rgb;

    return float4(result, albedo.a);
}
