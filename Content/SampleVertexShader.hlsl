// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	min16float4 Pos : POSITION;
	min16float2 Tex : TEXCOORD;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	min16float4 Pos : SV_POSITION;
	min16float2 Tex : TEXCOORD;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
	return input;
}
