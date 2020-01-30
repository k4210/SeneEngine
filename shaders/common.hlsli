struct PairUInt16
{
	uint val;
};

uint GetFirst(const PairUInt16 pair)
{
	return pair.val & 0x0000FFFF;
}

uint GetSecond(const PairUInt16 pair)
{
	return (pair.val >> 16) & 0x0000FFFF;
}

void SetFirst(inout PairUInt16 pair, uint val)
{
	pair.val = (pair.val & 0xFFFF0000) | (val & 0x0000FFFF);
}

void SetSecond(inout PairUInt16 pair, uint val)
{
	pair.val = (pair.val & 0x0000FFFF) | ((val << 16) & 0xFFFF0000);
}

struct PSInput
{
    float4 position : SV_POSITION;
    float4 norm : NORMAL;
    float2 tex : TEXCOORD;
};

struct DrawIndexedArgs
{
	uint IndexCountPerInstance;
	uint InstanceCount;
	uint StartIndexLocation;
	int BaseVertexLocation;
	uint StartInstanceLocation;
};

struct IndirectCommand
{
	float4x4 wvp_mtx;
	uint4 index_buffer;
	uint4 vertex_buffer;
	uint texture_index[2];
	float mat_val;
	DrawIndexedArgs draw_arg;
};

struct MeshLOD
{
	uint4 index_buffer;
	uint4 vertex_buffer;
};

struct MeshData
{
	MeshLOD lod[3];
	float max_lod_distance[2];
	float mat_val;
	PairUInt16 texture_indexes;
};

struct Transform
{
	float3 translate;
	float scale;
	float4 rotation;
};

struct MeshInstance
{
	Transform transform;
	float radius;
	PairUInt16 mesh_index_and_max_distance;
};

struct BoundingSphere
{
	float3 center;
	float radius;
};
