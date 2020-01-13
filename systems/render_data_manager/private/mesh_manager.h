#pragma once

#include "rdm_base.h"

class MeshManager
{
	Bitset2::bitset2<Const::kMeshCapacity> free_mesh_slots_;
	std::vector<std::shared_ptr<Mesh>> pending_remove_;
	std::vector<std::shared_ptr<Mesh>> pending_add_;

	bool AddMeshGPU(StructBuffer& mesh_buffer, const Mesh& mesh, ID3D12GraphicsCommandList* command_list, UploadBuffer& upload_buffer)
	{
		const auto reserved_mem = upload_buffer.reserve_space(sizeof(MeshDataGPU), alignof(MeshDataGPU));
		if (reserved_mem.has_value())
		{
			auto [offset, dst_ptr] = *reserved_mem;
			{
				MeshDataGPU& data = *reinterpret_cast<MeshDataGPU*>(dst_ptr);
				data.index_buffer = mesh.index_buffer.get_index_view();
				data.vertex_buffer = mesh.vertex_buffer.get_vertex_view();
				data.texture_idx[0] = data.texture_idx[1] = 0;
				data.material_value = 1.0f;
			}
			command_list->CopyBufferRegion(mesh_buffer.get_resource(), mesh.index * sizeof(MeshDataGPU)
				, upload_buffer.get_resource(), offset, sizeof(MeshDataGPU));
		}
		return reserved_mem.has_value();
	}

public:
	MeshManager() { free_mesh_slots_.set(); }

	void Add(std::shared_ptr<Mesh> mesh)
	{
		assert(mesh);
		const auto free_slot = free_mesh_slots_.find_first();
		assert(free_slot != Bitset2::bitset2<Const::kMeshCapacity>::npos);
		free_mesh_slots_.set(free_slot, false);
		assert(mesh->index == Const::kInvalid);
		mesh->index = static_cast<uint32_t>(free_slot);
		pending_add_.push_back(mesh);
	}

	void Remove(std::shared_ptr<Mesh> mesh)
	{
		assert(mesh->index != Const::kInvalid);
		assert(mesh->ready_to_render());
		pending_remove_.push_back(mesh);
	}

	void FlushPendingRemove()
	{
		for (auto mesh : pending_remove_)
		{
			assert(mesh->index != Const::kInvalid);
			assert(!free_mesh_slots_[mesh->index]);
			free_mesh_slots_.set(mesh->index, true);
			mesh->index = Const::kInvalid;
			mesh->added_in_batch = Const::kInvalid;
		}
		pending_remove_.clear();
	}

	void Clear()
	{
		FlushPendingRemove();
		pending_add_.clear();
		free_mesh_slots_.set();
	}

	EUpdateResult Update(StructBuffer& mesh_buffer, ID3D12GraphicsCommandList* command_list, UploadBuffer& upload_buffer)
	{
		if (!pending_add_.size())
			return EUpdateResult::NoUpdateRequired;
		const IRenderer::RendererCommon& render_common = IRenderer::GetRendererCommon();
		for (uint32_t idx = 0; idx < pending_add_.size();)
		{
			auto mesh = pending_add_[idx];
			assert(mesh);
			bool added = true;
			if (!mesh->vertex_buffer.is_ready())
			{
				assert(mesh->mesh_data);
				std::vector<Vertex>& vertexes = mesh->mesh_data->vertexes;
				added = Construct<Vertex, VertexBuffer>(mesh->vertex_buffer, &(vertexes[0]), 
					vertexes.size(), render_common.device.Get(), command_list, upload_buffer,
					nullptr, { D3D12_RESOURCE_STATE_COPY_DEST });
			}
			if (added && !mesh->index_buffer.is_ready())
			{
				assert(mesh->mesh_data);
				std::vector<uint32_t>& indices = mesh->mesh_data->indices;
				added = Construct<uint32_t, IndexBuffer32>(mesh->index_buffer, &(indices[0]), 
					indices.size(), render_common.device.Get(), command_list, upload_buffer,
					nullptr, { D3D12_RESOURCE_STATE_COPY_DEST });
			}
			if (added)
			{
				added = AddMeshGPU(mesh_buffer, *mesh, command_list, upload_buffer);
			}
			if (added)
			{
				pending_add_.erase(pending_add_.begin() + idx);
				mesh->mesh_data.reset();
			}
			else
			{
				return EUpdateResult::UpdateStillNeeded;
			}
		}
		return EUpdateResult::Updated;
	}
};