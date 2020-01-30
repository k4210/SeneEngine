#include "common.hlsli"
#include "common_scene.hlsli"

RWStructuredBuffer<uint> out_inst_idx		: register(u0);

StructuredBuffer<uint> counter				: register(t4);
StructuredBuffer<uint> in_node_idx			: register(t5);

[numthreads(64, 1, 1)]
void main(uint3 index3d : SV_DispatchThreadID)
{
	const uint index = index3d.x;
	if (index >= counter[0])
		return;

	const uint node_idx = in_node_idx[index];
	const uint base_inst_idx = node_idx * 16;
	for (uint it = 0; it < 16; it++)
	{
		const uint instance = instances_in_node[base_inst_idx + it];
		if (instance != 0xFFFFFFFF)
		{
			out_inst_idx[out_inst_idx.IncrementCounter()] = instance;
		}
	}
}