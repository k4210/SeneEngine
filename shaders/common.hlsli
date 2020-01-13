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

struct MeshData
{
	uint4 index_buffer;
	uint4 vertex_buffer;
	uint texture_index; //2 x 16
	float mat_val;
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
	uint mesh_index;
};

struct BoundingSphere
{
	float3 Center;
	float Radius;
};

struct SceneNodeGPU
{
	BoundingSphere bounding_sphere;
	uint instances; //start:24; size : 8;
};