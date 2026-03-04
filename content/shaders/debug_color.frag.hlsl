struct PixelInput
{
    float4 Position : SV_Position;
    float3 Normal : TEXCOORD0;
    float3 Color : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

float4 main(PixelInput input) : SV_Target0
{
    float3 normal = normalize(input.Normal);
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float3 lightColor = float3(1.0, 1.0, 1.0);

    float ambientStrength = 0.2;
    float3 ambient = ambientStrength * lightColor;

    float3 diffuse = max(dot(normal, lightDir), 0.0) * lightColor;

    float3 result = (ambient + diffuse) * input.Color;

    return float4(1.0, 0.0, 0.0, 0.2);
}
