// SpinningCube.vert.hlsl
//
// Takes in a full 4-component vertex position and transforms it
// using a matrix provided in a Uniform Block.
//
// Compile this for Direct3D 12 with:
// dxc -T vs_6_0 SpinningCube.vert.hlsl -Fo SpinningCube.vert.dxil
//
// or for Vulkan with:
// dxc -spirv -T vs_6_0 SpinningCube.vert.hlsl -Fo SpinningCube.vert.spv

struct InputVertex
{
    float3 Position : TEXCOORD0;
    float3 Color : TEXCOORD1;
};

struct OutputVertex
{
    float4 Position : SV_Position;
    float4 Color : TEXCOORD0;
};

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 ModelViewProjectionMatrix : packoffset(c0);
};

OutputVertex main(InputVertex vertex_in)
{
    OutputVertex vertex_out;
    vertex_out.Position = mul(ModelViewProjectionMatrix, float4(vertex_in.Position, 1.0));
    vertex_out.Color = float4(vertex_in.Color, 1.0);
    return vertex_out;
}
