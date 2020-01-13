#include "common.hlsli"

float4x4 world_mtx : register(b0);

PSInput main(float4 position : POSITION, float4 norm : NORMAL, float2 tex : TEXCOORD)
{
    PSInput result;
    result.position = position;
    result.norm = norm;
	result.tex = tex;
    return result;
}
