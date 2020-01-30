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
			mesh.fill_gpu(*reinterpret_cast<MeshDataGPU*>(dst_ptr));
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
		assert(mesh->index == Const::kInvalid32);
		mesh->index = static_cast<uint32_t>(free_slot);
		pending_add_.push_back(mesh);
	}

	void Remove(std::shared_ptr<Mesh> mesh)
	{
		assert(mesh->index != Const::kInvalid32);
		assert(mesh->ready_to_render());
		pending_remove_.push_back(mesh);
	}

	void FlushPendingRemove()
	{
		for (auto mesh : pending_remove_)
		{
			assert(mesh->index != Const::kInvalid32);
			assert(!free_mesh_slots_[mesh->index]);
			free_mesh_slots_.set(mesh->index, true);
			mesh->index = Const::kInvalid32;
			mesh->added_in_batch = Const::kInvalid32;
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
			for (uint32_t lod_idx = 0; added && (lod_idx < mesh->get_lod_num()); lod_idx++)
			{
				assert(mesh->mesh_data);
				const MeshDataCPU::LOD& lod_cpu = mesh->mesh_data->lod[lod_idx];
				Mesh::LOD& lod_buffs = mesh->lod[lod_idx];
				if (!lod_buffs.vertex_buffer.is_ready())
				{
					added = Construct<Vertex, VertexBuffer>(lod_buffs.vertex_buffer, &(lod_cpu.vertexes[0]),
						lod_cpu.vertexes.size(), render_common.device.Get(), command_list, upload_buffer,
						nullptr, { D3D12_RESOURCE_STATE_COPY_DEST });
				}
				if (added && !lod_buffs.index_buffer.is_ready())
				{
					added = Construct<uint32_t, IndexBuffer32>(lod_buffs.index_buffer, &(lod_cpu.indices[0]),
						lod_cpu.indices.size(), render_common.device.Get(), command_list, upload_buffer,
						nullptr, { D3D12_RESOURCE_STATE_COPY_DEST });
				}
			}
			if (added)
			{
				added = AddMeshGPU(mesh_buffer, *mesh, command_list, upload_buffer);
			}

			if (!added)
				return EUpdateResult::UpdateStillNeeded;

			pending_add_.erase(pending_add_.begin() + idx);
			mesh->mesh_data.reset();
		}
		return EUpdateResult::Updated;
	}
};