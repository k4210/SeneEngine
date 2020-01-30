#include "common.hlsli"

RWStructuredBuffer<IndirectCommand> out_commands		: register(u0);
RWStructuredBuffer<uint>			out_temp_1			: register(u1);	//16 bits!
RWStructuredBuffer<uint>			out_temp_2			: register(u2);	//16 bits!

StructuredBuffer<MeshData>			meshes				: register(t0);
StructuredBuffer<BoundingSphere>	nodes				: register(t1);
StructuredBuffer<uint>				instances_in_node	: register(t2); //16 bits!
StructuredBuffer<MeshInstance>		instances			: register(t3);

StructuredBuffer<uint>				temp_1				: register(t4); //16 bits!
StructuredBuffer<uint>				temp_1_counter		: register(t5);
StructuredBuffer<uint>				temp_2				: register(t6);	//16 bits!
StructuredBuffer<uint>				temp_2_counter		: register(t7);

cbuffer SceneManagerParams : register(b0)
{
	uint instances_num;
};

[numthreads(64, 1, 1)]
void main(uint3 index3d : SV_DispatchThreadID)
{
	const uint index = index3d.x;
	if (index >= instances_num)
		return;

	const MeshInstance instance = instances[index];
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