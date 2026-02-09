struct VertexInput
{
    float3 Position : TEXCOORD0;
    float3 Color : TEXCOORD1;
    float3 Normal : TEXCOORD2;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 Color : TEXCOORD0;
    float3 Normal : TEXCOORD1;
};

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 ViewProjectionMatrix : packoffset(c0);
    float4x4 ModelMatrix : packoffset(c4);
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;

    float4 worldPosition = mul(ModelMatrix, float4(input.Position, 1.0));
    output.Position = mul(ViewProjectionMatrix, worldPosition);

    output.Color = input.Color;
    output.Normal = normalize(mul((float3x3)ModelMatrix, input.Normal));

    return output;
}
