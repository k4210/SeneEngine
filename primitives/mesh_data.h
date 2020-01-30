#pragma once

#include "utils/graphics/gpu_containers.h"
#include "utils/math/VectorMath.h"

using namespace DirectX;
using namespace Math;

namespace Const
{
	constexpr uint32_t kLodNum = 3;
	constexpr uint32_t kMaxInstancesPerNode = 16;
};

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 texcoord;
};

struct MeshLODGPU
{
	D3D12_INDEX_BUFFER_VIEW index_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer;
};

struct MeshDataGPU
{
	MeshLODGPU lod[Const::kLodNum];
	float max_lod_distance[Const::kLodNum - 1];
	float material_value;
	uint16_t texture_idx[2];
};

struct MeshDataCPU
{
	struct LOD 
	{
		std::vector<Vertex> vertexes;
		std::vector<uint32_t> indices;//16?
	};
	LOD lod[Const::kLodNum];
};

struct Mesh : std::enable_shared_from_this<Mesh>
{
public: //SERIALIZED
	//loaded by data_source
	//removed once render_data_manager filled the gpu data
	std::unique_ptr<MeshDataCPU> mesh_data;
	uint32_t additional_lod_num = 0;
	float max_distance[Const::kLodNum];
	float radius = 0;
	
	//Material Data

public:
	//debug str/path

public: //TRANSIENT data
	struct LOD
	{
		VertexBuffer vertex_buffer;
		IndexBuffer32 index_buffer;

		bool is_ready() const 
		{
			return vertex_buffer.is_ready() && index_buffer.is_ready();
		}
	};
	LOD lod[Const::kLodNum];

	uint32_t index = Const::kInvalid32;
	uint32_t instances = 0;
	std::atomic_uint32_t added_in_batch = Const::kInvalid32; // when mesh is loaded the value is smaller that RDM->batch_done

public:
	uint32_t get_lod_num() const
	{
		return 1 + additional_lod_num;
	}

	bool ready_to_render() const
	{
		for (uint32_t idx = 0; idx < get_lod_num(); idx++)
		{
			if (!lod[idx].is_ready())
				return false;
		}
		return index != Const::kInvalid32;
	}

	float get_max_draw_distance()
	{
		return max_distance[additional_lod_num];
	}

	void fill_gpu(MeshDataGPU& out_data) const
	{
		for (uint32_t idx = 0; idx < get_lod_num(); idx++)
		{
			out_data.lod[idx].index_buffer = lod[idx].index_buffer.get_index_view();
			out_data.lod[idx].vertex_buffer = lod[idx].vertex_buffer.get_vertex_view();
			if (idx < (Const::kLodNum - 1))
			{
				out_data.max_lod_distance[idx] = max_distance[idx];
			}
		}
		for (int idx = get_lod_num(); idx < (Const::kLodNum - 1); idx++)
		{
			out_data.max_lod_distance[idx] = -1.0f;
		}

		out_data.texture_idx[0] = 0;
		out_data.texture_idx[1] = 0;
		out_data.material_value = 1.0f;
	}

	~Mesh()
	{
		assert(index == Const::kInvalid32);
		assert(instances == 0);
	}
};

struct MeshInstanceGPU
{
	Transform transform;
	float radius;
	uint16_t mesh_index;
	uint16_t max_distance;
};

struct InstancesInNodeGPU
{
	using TIndex = uint32_t;
	static constexpr TIndex kInvalid = Const::kInvalid32;
	std::array<TIndex, Const::kMaxInstancesPerNode> instances = { kInvalid };
};