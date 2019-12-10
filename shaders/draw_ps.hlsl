struct PSInput
{
    float4 position : SV_POSITION;
    float4 norm : NORMAL;
	float2 tex : TEXCOORD;
};

float4 Main(PSInput input) : SV_TARGET
{
    return float4(0.8, 0.8, 0.8, 1.0);
}
