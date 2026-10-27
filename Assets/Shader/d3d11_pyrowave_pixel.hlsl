// Three-plane YUV -> RGB pixel shader for the PyroWave path.
// Handles both 4:4:4 (full-res chroma planes) and 4:2:0 (quarter-res chroma
// planes): planes are sampled with normalized coordinates, so the sampler's
// bilinear filter upscales 4:2:0 chroma; chromaOffset applies cositing and
// is zero for 4:4:4 (and for center-sited 4:2:0).
// Same CSC constant buffer layout as d3d11_yuv420_pixel.hlsl so
// VideoRenderer::bindColorConversion is shared. PyroWave planes are exactly
// frame-sized (no alignment padding), so chromaTexMax is 1.0.

Texture2D<min16float> luminancePlane : register(t0);
Texture2D<min16float> cbPlane : register(t1);
Texture2D<min16float> crPlane : register(t2);
SamplerState theSampler : register(s0);

struct ShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

cbuffer CSC_CONST_BUF : register(b0)
{
    min16float3x3 cscMatrix;
    min16float3 offsets;
    min16float2 chromaOffset;
    min16float2 chromaTexMax;
};

min16float4 main(ShaderInput input) : SV_TARGET
{
    min16float2 chromaTex = min(input.tex + chromaOffset, chromaTexMax.rg);
    min16float3 yuv = min16float3(luminancePlane.Sample(theSampler, input.tex),
                                  cbPlane.Sample(theSampler, chromaTex),
                                  crPlane.Sample(theSampler, chromaTex));

    // Subtract the YUV offset for limited vs full range
    yuv -= offsets;

    // Multiply by the conversion matrix for this colorspace
    yuv = mul(yuv, cscMatrix);

    return min16float4(yuv, 1.0);
}
