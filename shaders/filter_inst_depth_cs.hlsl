#include "common.hlsli"
#include "common_scene.hlsli"

RWStructuredBuffer<uint> out_inst_idx	: register(u0);

StructuredBuffer<uint> counter			: register(t4);
StructuredBuffer<uint> in_inst_idx		: register(t5);

[numthreads(64, 1, 1)]
void main(uint3 index3d : SV_DispatchThreadID)
{
	const uint index = index3d.x;
	if (index >= counter[0])
		return;

	const uint inst_idx = in_inst_idx[index];
	if (instances[inst_idx].radius > 0.0f)
	{
		out_inst_idx[out_inst_idx.IncrementCounter()] = inst_idx;
	}
}