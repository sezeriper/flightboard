struct PixelInput
{
    float4 Position : SV_Position;
    float3 Normal : TEXCOORD0;
    float3 Color : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

cbuffer ColorBlock : register(b0, space3)
{
    float4 IndicatorColor : packoffset(c0);
};

float4 main(PixelInput input) : SV_Target0
{
    return IndicatorColor;
}
