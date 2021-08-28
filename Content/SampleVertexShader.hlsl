// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	float4 Pos : POSITION;
	float2 Tex : TEXCOORD;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
	return input;
}
