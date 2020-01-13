#include "common.hlsli"

uint instances_num : register(b0);
RWStructuredBuffer<IndirectCommand> out_commands : register(u0);
StructuredBuffer<MeshData> meshes : register(t0);
StructuredBuffer<MeshInstance> instances : register(t1);

[numthreads(64, 1, 1)]
void main(uint3 index3d : SV_DispatchThreadID)
{
	const uint index = index3d.x;
	if (index >= instances_num)
		return;

	const MeshInstance instance = instances[index];
	const MeshData mesh = meshes[instance.mesh_index];
	IndirectCommand command;
	command.index_buffer						= mesh.index_buffer;
	command.vertex_buffer						= mesh.vertex_buffer;
	command.mat_val								= mesh.mat_val;
	command.texture_index[0]					= mesh.texture_index & 0xFFFF;
	command.texture_index[1]					= mesh.texture_index >> 16;
	command.draw_arg.IndexCountPerInstance		= mesh.index_buffer[2] / 4;
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