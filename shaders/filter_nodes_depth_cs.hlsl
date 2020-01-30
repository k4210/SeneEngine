#include "common.hlsli"
#include "common_scene.hlsli"

RWStructuredBuffer<uint> out_nodes_idx	: register(u0);

StructuredBuffer<uint> counter			: register(t4);
StructuredBuffer<uint> in_node_idx		: register(t5);

[numthreads(64, 1, 1)]
void main(uint3 index3d : SV_DispatchThreadID)
{
	const uint index = index3d.x;
	if (index >= counter[0])
		return;

	const uint node_idx = in_node_idx[index];
	if (nodes[node_idx].radius > 0.0f)
	{
		out_nodes_idx[out_nodes_idx.IncrementCounter()] = node_idx;
	}
}