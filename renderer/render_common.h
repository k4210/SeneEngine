#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <wrl.h>
#include <cstddef>
#include <unordered_map>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct ID
{
private:
	uint64_t data = 0;
public:
	static ID New()
	{
		static uint64_t id = 0;
		id++;
		ID result;
		result.data = id;
		return result;
	}
	bool operator==(const ID& other) const { return data == other.data; }
	bool IsValid() const { return !!data; }

	static std::size_t Hash(const ID& id) { return static_cast<std::size_t>(id.data); }
};

namespace std
{
	template<> struct hash<ID>
	{
		typedef ID argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
			return ID::Hash(s);
		}
	};
}

using FenceValue = uint64_t;

struct RawTextureData
{

};

struct RawMeshData
{

};

struct InstanceData
{
	uint32_t mesh_index; 
	XMFLOAT4X4 model_to_world;
	BoundingSphere bounding_sphere_world_space_;
};

struct RendererCommon
{
	static constexpr uint32_t kInvalid = 0xFFFFFFFF;
	static constexpr uint32_t kFrameCount = 2;
	static constexpr uint32_t kLodNum = 3;
	static constexpr uint32_t kMeshCapacity = 4096;
	static constexpr uint32_t kStaticNodesCapacity = 4096;
	static constexpr uint32_t kStaticInstancesCapacity = 12 * 4096;
	static constexpr uint32_t kMoveableInstancesCapacity = 4 * 4096;
	static constexpr uint32_t kMaxInstancesPerNode = 32;

	ComPtr<IDXGISwapChain3> swap_chain_;
	ComPtr<ID3D12Device> device_;
	ComPtr<ID3D12CommandQueue> direct_command_queue_;
	ComPtr<ID3D12Fence> direct_queue_fence_;
	ComPtr<ID3D12Fence> update_queue_fence_;
	ComPtr<ID3D12Fence> loading_queue_fence_;
	const class RenderThread*	render_thread_		= nullptr;
	const class SceneThread*	scene_thread_		= nullptr;
	const class MoveableThread*	moveable_thread_	= nullptr;

	BOOL tearing_supported_ = false;
	uint32_t width = 1280;
	uint32_t height = 720;

	FenceValue GetLastUnfinishedFrameRT() const;
	FenceValue GetLastUnfinishedFrameMT() const;
	FenceValue GetLastUnfinishedBatch() const;
	FenceValue GetLastLoadedBatch() const;
};
	

template<typename MSG> class BaseOngoingThread
{
protected:
	moodycamel::ConcurrentQueue< MSG > message_queue_;
	std::thread thread_;
	RendererCommon& renderer_common_;
	std::atomic_uint64_t tick_index_ = 0;
	bool exit_requested_ = false;

	template<typename Func, typename Obj> void InnerStart(Func func, Obj obj)
	{
		exit_requested_ = false;
		thread_ = std::thread(func, obj);
	}

public:
	BaseOngoingThread(RendererCommon& renderer_common)
		: renderer_common_(renderer_common) {}
	void ReceiveMsg(MSG&& msg) { message_queue_.enqueue(msg); }
	
	void RequestStop() { exit_requested_ = true; }
	void WaitForStop()
	{
		assert(exit_requested_);
		if (thread_.joinable()) { thread_.join(); }
	}

	FenceValue LastUnfinishedTick() const { return tick_index_; }
};