#include "common.hlsli"
#include "common_scene.hlsli"

RWStructuredBuffer<IndirectCommand> out_commands		: register(u0);

StructuredBuffer<uint>				counter				: register(t4);
StructuredBuffer<uint>				in_inst_idx			: register(t5);

[numthreads(64, 1, 1)]
void main(uint3 index3d : SV_DispatchThreadID)
{
	const uint index = index3d.x;
	if (index >= counter[0])
		return;

	const uint inst_idx = in_inst_idx[index];
	const MeshInstance instance = instances[inst_idx];
	const uint mesh_idx = GetFirst(instance.mesh_index_and_max_distance);
	const MeshData mesh = meshes[mesh_idx];
	IndirectCommand command;
	command.index_buffer						= mesh.lod[0].index_buffer;
	command.vertex_buffer						= mesh.lod[0].vertex_buffer;
	command.mat_val								= mesh.mat_val;
	command.texture_index[0]					= GetFirst(mesh.texture_indexes);
	command.texture_index[1]					= GetSecond(mesh.texture_indexes);
	command.draw_arg.IndexCountPerInstance		= command.index_buffer[2] / 4;
	command.draw_arg.InstanceCount				= 1;
	command.draw_arg.StartIndexLocation			= 0;
	command.draw_arg.BaseVertexLocation			= 0;
	command.draw_arg.StartInstanceLocation		= 0;
	command.wvp_mtx								= float4x4(0.0f, 0.0f, 0.0f, 0.0f
		, 0.0f, 0.0f, 0.0f, 0.0f
		, 0.0f, 0.0f, 0.0f, 0.0f
		, 0.0f, 0.0f, 0.0f, 0.0f);
	//TODO: world_mtx. Here final WVP mtx may be calculated.
	out_commands[out_commands.IncrementCounter()] = command;
}