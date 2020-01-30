cbuffer SceneManagerParams : register(b0)
{
	uint k_instances_num;
};

StructuredBuffer<BoundingSphere>	nodes				: register(t0);
StructuredBuffer<MeshInstance>		instances			: register(t1);
StructuredBuffer<uint>				instances_in_node	: register(t2);
StructuredBuffer<MeshData>			meshes				: register(t3);