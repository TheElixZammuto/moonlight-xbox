// YUV->RGB shader that samples the decoder's output surface directly. FFmpeg's
// D3D11VA decoder hands frames out as slices of a single Texture2D *array*, so
// the planes are declared as Texture2DArray here. The SRVs bound to t0/t1 are
// created with FirstArraySlice pointing at the decoded frame's slice and
// ArraySize == 1, so within the shader we always sample array index 0. This
// avoids a per-frame CopySubresourceRegion1 into an intermediate texture.
Texture2DArray<min16float> luminancePlane : register(t0);
Texture2DArray<min16float2> chrominancePlane : register(t1);
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
    // Clamp the chrominance texcoords to avoid sampling the row of texels adjacent to the alignment padding
    min16float3 yuv = min16float3(luminancePlane.Sample(theSampler, float3(input.tex, 0)),
                                  chrominancePlane.Sample(theSampler, float3(min(input.tex + chromaOffset, chromaTexMax.rg), 0)));

    // Subtract the YUV offset for limited vs full range
    yuv -= offsets;

    // Multiply by the conversion matrix for this colorspace
    yuv = mul(yuv, cscMatrix);

    return min16float4(yuv, 1.0);
}
