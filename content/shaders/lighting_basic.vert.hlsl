struct VertexInput
{
    float3 Position : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float3 Color : TEXCOORD2;
    float2 UV : TEXCOORD3;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 Normal : TEXCOORD0;
    float3 Color : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 ViewProjectionMatrix : packoffset(c0);
    float4   ModelPosition        : packoffset(c4);
    float4x4 ModelMatrix          : packoffset(c5);
};
VertexOutput main(VertexInput input)
{
    VertexOutput output;

    // Use a float4 for the matrix multiplication, then grab the .xyz result
    float3 scaledRotatedPos = mul(ModelMatrix, float4(input.Position, 1.0)).xyz;

    // Use ModelPosition.xyz to ignore the padded 4th value
    float3 cameraRelativePos = ModelPosition.xyz + scaledRotatedPos;

    output.Position = mul(ViewProjectionMatrix, float4(cameraRelativePos, 1.0));
    output.Color = input.Color;

    // Cast the float4x4 down to a float3x3 inline just for the normals
    output.Normal = normalize(mul((float3x3)ModelMatrix, input.Normal));
    output.UV = input.UV;

    return output;
}
