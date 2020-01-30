#include "common.hlsli"
#include "common_scene.hlsli"

RWStructuredBuffer<uint> out_nodes_idx	: register(u0);

[numthreads(64, 1, 1)]
void main(uint3 index3d : SV_DispatchThreadID)
{
	const uint index = index3d.x;
	if (index >= k_instances_num)
		return;

	if (nodes[index].radius > 0.0f)
	{
		out_nodes_idx[out_nodes_idx.IncrementCounter()] = index;
	}
}