#pragma once

#include "render_common.h"
#include "render_thread.h"
#include "scene_thread.h"
#include "moveable_thread.h"

using namespace DirectX;

class Renderer
{
	RendererCommon common_;
	RenderThread render_thread_;
	SceneThread scene_thread_;
	MoveableThread moveable_thread_;

public:
	Renderer() : render_thread_(common_)
		, scene_thread_(common_), moveable_thread_(common_)
	{
		common_.render_thread_ = &render_thread_;
		common_.scene_thread_ = &scene_thread_;
		common_.moveable_thread_ = &moveable_thread_;
	}

	void Init(HWND window_handle, UINT width, UINT height);
	void Destroy();
	
	void Start();
	void Stop();

	const RendererCommon& GetRendererCommon() const { return common_; }

	void SetCamera(XMFLOAT3 position, XMFLOAT3 direction);

	//returns the index in mesh table
	struct AddedMesh { uint32_t index; FenceValue batch; };
	AddedMesh AddMesh(std::shared_ptr<RawMeshData> mesh);
	void RemoveMesh(uint32_t index);

	ID AddStaticSceneNode(BoundingSphere bounding_sphere_world_space
		, std::vector<InstanceData>&& instances);
	void RemoveStaticSceneNode(ID id);

	ID AddMoveableInstance(InstanceData instance, uint32_t needed_batch);
	void UpdateMoveableInstance(ID instance_id
		, XMFLOAT4X4 model_to_world
		, BoundingSphere bounding_sphere_world_space);
	void RemoveMoveableInstance(ID instance_id);
};

