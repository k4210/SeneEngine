#pragma once

#include <vector>
#include "utils/gpu_containers.h"
#include "utils/math.h"

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
};

struct Mesh : std::enable_shared_from_this<Mesh>
{
	//loaded by data_source
	//removed once render_data_manager filled the gpu data
	std::optional<MeshData> mesh_data;

	//owned by render_data_manager
	VertexBuffer vertex_buffer;
	IndexBuffer32 index_buffer;
	uint32_t index = Const::kInvalid;
	uint32_t instances = 0;

	//Materials
	//debug str/path
};

struct MeshDataGPU
{
	D3D12_INDEX_BUFFER_VIEW index_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer;
	float radius;
	uint16_t texture_idx[2];
	float material_value[2];
};