#include "common.hlsli"

cbuffer CB0 : register(b0)
{
    uint tex_idx_0;
    uint tex_idx_1;
    float material_val;
}

float4 main(PSInput input) : SV_TARGET
{
    return float4(0.8, 0.8, material_val, 1.0);
}
