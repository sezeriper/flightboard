struct PixelInput
{
    float4 Position : SV_Position;
    float3 Color : TEXCOORD0;
    float3 Normal : TEXCOORD1;
};

float4 main(PixelInput input) : SV_Target0
{
    float3 N = normalize(input.Normal);
    // Hard-coded light direction (pointing towards the light, i.e., top-right-front)
    float3 L = normalize(float3(1.0, 1.0, 1.0));
    float3 lightColor = float3(1.0, 1.0, 1.0);

    // Ambient component
    float ambientStrength = 0.2;
    float3 ambient = ambientStrength * lightColor;

    // Diffuse component
    float diff = max(dot(N, L), 0.0f);
    float3 diffuse = diff * lightColor;

    // Combine
    float3 result = (ambient + diffuse) * input.Color;

    return float4(result, 1.0f);
}
