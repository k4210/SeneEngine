#pragma once

#include "utils/graphics/gpu_containers.h"
#include "utils/math/VectorMath.h"

using namespace DirectX;
using namespace Math;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 texcoord;
};

struct MeshData 
{
	std::vector<Vertex> vertexes;
	std::vector<uint32_t> indices;
	// meterial info
};

struct Mesh : std::enable_shared_from_this<Mesh>
{
	//loaded by data_source
	//removed once render_data_manager filled the gpu data
	std::optional<MeshData> mesh_data;
	float radius = 0;

	//owned by render_data_manager
	VertexBuffer vertex_buffer;
	IndexBuffer32 index_buffer;

	uint32_t index = Const::kInvalid;
	uint32_t instances = 0;

	std::atomic_uint32_t added_in_batch = Const::kInvalid; // when mesh is loaded the value is smaller that RDM->batch_done
	bool ready_to_render() const
	{
		return vertex_buffer.is_ready() && index_buffer.is_ready()
			&& (index != Const::kInvalid);
	}

	~Mesh()
	{
		assert(index == Const::kInvalid);
		assert(instances == 0);
	}

	//Materials
	//debug str/path
};

struct MeshDataGPU
{
	D3D12_INDEX_BUFFER_VIEW index_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer;
	float radius;
	uint16_t texture_idx[2];
	//float material_value[2];
};

struct MeshInstanceGPU
{
	Transform transform;
	float radius = 0.0f;
	uint32_t mesh_index = Const::kInvalid;
};

struct SceneNodeGPU
{
	BoundingSphere bounding_sphere;
	uint32_t start:24;
	uint32_t size : 8;
};