struct PSInput
{
    float4 position : SV_POSITION;
    float4 norm : NORMAL;
	float2 tex : TEXCOORD;
};

PSInput main(float4 position : POSITION, float4 norm : NORMAL, float2 tex : TEXCOORD)
{
    PSInput result;
    result.position = position;
    result.norm = norm;
	result.tex = tex;
    return result;
}
