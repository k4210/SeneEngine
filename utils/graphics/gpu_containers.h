#pragma once

#include "d3dx12.h"
#include <wrl.h>
#include <unordered_map>
#include "../base_app_helper.h"

using Microsoft::WRL::ComPtr;

namespace Const
{
	constexpr uint16_t kInvalid16 = 0xFFFF;
	constexpr uint32_t kInvalid32 = 0xFFFFFFFF;
	constexpr uint64_t kInvalid64 = 0xFFFFFFFFFFFFFFFF;
}

struct DescriptorHeap
{
protected:
	ComPtr<ID3D12DescriptorHeap> heap_;
	D3D12_DESCRIPTOR_HEAP_TYPE type_ = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	uint32_t descriptor_size_ = 0;
	std::vector<bool> taken_slots_;

public:
	ID3D12DescriptorHeap* get_heap() const { return heap_.Get(); }
	ID3D12DescriptorHeap* Get() const { return heap_.Get(); } // to work with macro
	uint32_t get_capacity() const { return static_cast<uint32_t>(taken_slots_.size()); }
	bool is_set(uint32_t idx) const { return taken_slots_[idx]; }

	CD3DX12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(uint32_t idx) const
	{
		assert(heap_);
		assert(idx < get_capacity());
		assert(is_set(idx));
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(heap_->GetCPUDescriptorHandleForHeapStart(), idx, descriptor_size_);
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(uint32_t idx) const
	{
		assert(heap_);
		assert(idx < get_capacity());
		assert(is_set(idx));
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(heap_->GetGPUDescriptorHandleForHeapStart(), idx, descriptor_size_);
	}

	void create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, uint32_t capacity, uint32_t initially_allocated = 0);
	uint32_t allocate();
	void copy(ID3D12Device* device, uint32_t num, uint32_t dst_start, uint32_t src_start, const DescriptorHeap& src_heap);
	void free(uint32_t idx);
	void clear(uint32_t initially_allocated = 0)
	{
		for (uint32_t idx = 0; idx < taken_slots_.size(); idx++)
		{
			taken_slots_[idx] = initially_allocated > idx;
		}
	}
	void destroy()
	{
		taken_slots_.clear();
		heap_.Reset();
		descriptor_size_ = 0;
		type_ = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	}
};

struct DescriptorHeapElementRef
{
protected:
	DescriptorHeap* descriptor_heap_ = nullptr;
	uint32_t idx_ = Const::kInvalid32;
public:
	CD3DX12_CPU_DESCRIPTOR_HANDLE get_cpu_handle() const
	{
		assert(descriptor_heap_ && (idx_ != Const::kInvalid32));
		return descriptor_heap_->get_cpu_handle(idx_);
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE get_gpu_handle() const
	{
		assert(descriptor_heap_ && (idx_ != Const::kInvalid32));
		return descriptor_heap_->get_gpu_handle(idx_);
	}

	void release()
	{
		if (descriptor_heap_ && idx_ != Const::kInvalid32)
		{
			descriptor_heap_->free(idx_);
		}
		descriptor_heap_ = nullptr;
		idx_ = Const::kInvalid32;
	}

	void clear_no_release()
	{
		descriptor_heap_ = nullptr;
		idx_ = Const::kInvalid32;
	}

	bool initialize(DescriptorHeap& desc_heap)
	{
		assert(!descriptor_heap_ && (idx_ == Const::kInvalid32));
		const auto new_idx = desc_heap.allocate();
		if (new_idx == Const::kInvalid32)
			return false;
		descriptor_heap_ = &desc_heap;
		idx_ = new_idx;
		return true;
	}

	void initialize(DescriptorHeap& desc_heap, uint32_t already_allocaded_idx)
	{
		assert(already_allocaded_idx != Const::kInvalid32);
		assert(!descriptor_heap_ && (idx_ == Const::kInvalid32));
		descriptor_heap_ = &desc_heap;
		idx_ = already_allocaded_idx;
		assert(desc_heap.is_set(idx_));
	}

	operator bool() const { return descriptor_heap_ && (idx_ != Const::kInvalid32); }

	uint32_t get_index() const { return idx_; }

	DescriptorHeap* get_heap() const { return descriptor_heap_; }
};

struct DescriptorHeapElement : public DescriptorHeapElementRef
{
	DescriptorHeapElement() = default;
	DescriptorHeapElement(DescriptorHeapElementRef&& ref)
		: DescriptorHeapElementRef(std::forward<DescriptorHeapElementRef>(ref)) {}
	DescriptorHeapElement(const DescriptorHeapElement&) = delete;
	DescriptorHeapElement& operator=(const DescriptorHeapElement&) = delete;
	DescriptorHeapElement(DescriptorHeapElement&& other) = default;

	~DescriptorHeapElement() { release(); }

	DescriptorHeapElementRef get_ref() const { return *this; }
};

inline DescriptorHeapElementRef CopyDescriptor(ID3D12Device* device, DescriptorHeap& dst, const DescriptorHeapElementRef& src, uint32_t dst_idx = Const::kInvalid32)
{
	assert(src);
	assert(device);
	DescriptorHeapElementRef result;
	if (dst_idx == Const::kInvalid32)
	{
		const bool allocated = result.initialize(dst);
		assert(allocated);
	}
	else
	{
		result.initialize(dst, dst_idx);
	}
	dst.copy(device, 1, result.get_index(), src.get_index(), *src.get_heap());
	return result;
}

class UploadBuffer
{
	ComPtr<ID3D12Resource> upload_buffer_;
	UINT8* data_begin_ = nullptr;    // starting position of upload buffer
	UINT8* current_pos_ = nullptr;      // current position of upload buffer
	UINT8* data_end_ = nullptr;      // ending position of upload buffer

public:
	static uint64_t align(uint64_t uLocation, uint64_t uAlign);
	void initialize(ID3D12Device* device, uint32_t size);
	std::optional< uint64_t > data_to_upload(const void* src_data, std::size_t size_bytes, std::size_t alignment);
	std::optional<std::tuple<uint64_t, UINT8*>> reserve_space(std::size_t size_bytes, std::size_t alignment);
	void reset();
	void destroy();
	ID3D12Resource* get_resource() { return upload_buffer_.Get(); }
	~UploadBuffer() { destroy(); }
};

struct CommitedBuffer
{
	static constexpr D3D12_RESOURCE_STATES kDefaultState = D3D12_RESOURCE_STATE_COMMON;

	ComPtr<ID3D12Resource> resource_;
	uint32_t elements_num_ = 0;
	uint32_t element_size_ = 0;
	D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COPY_DEST;
	
	void destroy()
	{
		resource_.Reset();
		elements_num_ = 0;
		element_size_ = 0;
		state_ = D3D12_RESOURCE_STATE_COPY_DEST;
	}

	void initialize(std::size_t capacity, std::size_t element_size)
	{
		assert(!element_size_);
		assert(!elements_num_);
		element_size_ = static_cast<uint32_t>(element_size);
		elements_num_ = static_cast<uint32_t>(capacity);
	}

	void create_resource(ID3D12Device* device);

	void create_views(ID3D12Device*, DescriptorHeap*) {}

	D3D12_RESOURCE_BARRIER transition_barrier(D3D12_RESOURCE_STATES new_state)
	{
		assert(new_state != state_);
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource_.Get(), state_, new_state);
		state_ = new_state;
		return barrier;
	}

	uint32_t				elements_num()	const { return elements_num_; }
	D3D12_RESOURCE_STATES	get_state()		const { return state_; }
	bool					is_ready()		const { return resource_ && elements_num_ && element_size_; }
	uint32_t				size()			const { assert(element_size_ && elements_num_); return element_size_ * elements_num_; }
	ID3D12Resource*			get_resource()	const { return resource_.Get(); }
};

struct ConstantBuffer : protected CommitedBuffer
{
protected:
	DescriptorHeapElement cbv_;

public:
	static constexpr D3D12_RESOURCE_STATES kDefaultState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	using CommitedBuffer::initialize;
	using CommitedBuffer::transition_barrier;
	using CommitedBuffer::elements_num;
	using CommitedBuffer::get_resource;
	using CommitedBuffer::get_state;
	using CommitedBuffer::size;

	uint32_t						whole_buffer_size()	const	{ return (size() + 255) & ~255; }
	void							destroy()					{ cbv_.release(); CommitedBuffer::destroy(); }
	bool							is_ready()			const	{ return CommitedBuffer::is_ready() && cbv_; }
	DescriptorHeapElementRef		get_cbv_handle()	const	{ return cbv_.get_ref(); }

	void create_resource(ID3D12Device * device);
	void create_views(ID3D12Device* device, DescriptorHeap* descriptor_heap);
};

struct StructBuffer : protected CommitedBuffer
{
protected:
	DescriptorHeapElement srv_;

public:
	static constexpr D3D12_RESOURCE_STATES kDefaultState = D3D12_RESOURCE_STATE_GENERIC_READ;

	using CommitedBuffer::initialize;
	using CommitedBuffer::create_resource;
	using CommitedBuffer::transition_barrier;
	using CommitedBuffer::elements_num;
	using CommitedBuffer::get_resource;
	using CommitedBuffer::size;
	using CommitedBuffer::get_state;

	void							destroy()					{ srv_.release(); CommitedBuffer::destroy(); }
	bool							is_ready()			const	{ return CommitedBuffer::is_ready() && srv_; }
	DescriptorHeapElementRef		get_srv_handle()	const	{ return srv_.get_ref(); }

	void create_views(ID3D12Device* device, DescriptorHeap* descriptor_heap);
};

struct VertexBuffer : protected CommitedBuffer
{
	static constexpr D3D12_RESOURCE_STATES kDefaultState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	using CommitedBuffer::initialize;
	using CommitedBuffer::create_resource;
	using CommitedBuffer::transition_barrier;
	using CommitedBuffer::elements_num;
	using CommitedBuffer::get_resource;
	using CommitedBuffer::size;
	using CommitedBuffer::get_state;
	using CommitedBuffer::destroy;
	using CommitedBuffer::is_ready;
	using CommitedBuffer::create_views;

	D3D12_VERTEX_BUFFER_VIEW get_vertex_view() const
	{
		assert(resource_);
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
		vertex_buffer_view.BufferLocation = resource_->GetGPUVirtualAddress();
		vertex_buffer_view.StrideInBytes = element_size_;
		vertex_buffer_view.SizeInBytes = size();
		return vertex_buffer_view;
	}
};

struct IndexBuffer32 : protected CommitedBuffer
{
	static constexpr D3D12_RESOURCE_STATES kDefaultState = D3D12_RESOURCE_STATE_INDEX_BUFFER;

	using CommitedBuffer::create_resource;
	using CommitedBuffer::transition_barrier;
	using CommitedBuffer::elements_num;
	using CommitedBuffer::get_resource;
	using CommitedBuffer::size;
	using CommitedBuffer::get_state;
	using CommitedBuffer::destroy;
	using CommitedBuffer::is_ready;
	using CommitedBuffer::create_views;

	void initialize(std::size_t capacity, std::size_t element_size = sizeof(uint32_t))
	{ 
		assert(element_size == sizeof(uint32_t)); 
		CommitedBuffer::initialize(capacity, element_size); 
	}

	D3D12_INDEX_BUFFER_VIEW get_index_view()	const
	{
		assert(resource_);
		D3D12_INDEX_BUFFER_VIEW index_buffer_view;
		index_buffer_view.BufferLocation = resource_->GetGPUVirtualAddress();
		index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
		index_buffer_view.SizeInBytes = size();
		return index_buffer_view;
	}
};

struct UavCountedBuffer : protected StructBuffer
{
protected:
	uint64_t counter_offset_bytes_ = Const::kInvalid32;
	DescriptorHeapElement uav_;
	DescriptorHeapElement counter_;

	static inline uint64_t align_for_uav_counter(uint64_t buffer_size)
	{
		const uint64_t alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
		return (buffer_size + (alignment - 1)) & ~(alignment - 1);
	}

public:
	static constexpr D3D12_RESOURCE_STATES kDefaultState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	using StructBuffer::initialize;
	using StructBuffer::transition_barrier;
	using StructBuffer::elements_num;
	using StructBuffer::get_resource;
	using StructBuffer::size;
	using StructBuffer::get_state;
	using StructBuffer::is_ready;
	using StructBuffer::get_srv_handle;

	void							destroy()
	{
		counter_offset_bytes_ = Const::kInvalid64;
		uav_.release();
		counter_.release();
		StructBuffer::destroy();
	}
	void							create_resource(ID3D12Device* device)
	{
		assert(device);
		assert(element_size_);
		assert(elements_num_);

		counter_offset_bytes_ = align_for_uav_counter(size());
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(counter_offset_bytes_ + sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		const CD3DX12_HEAP_PROPERTIES heap_prop(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &desc, state_, nullptr, IID_PPV_ARGS(&resource_)));
	}
	void							create_views(ID3D12Device* device, DescriptorHeap* descriptor_heap);

	DescriptorHeapElementRef		get_uav_handle()		const { return uav_.get_ref(); }
	uint64_t						get_counter_offset()	const { return counter_offset_bytes_; }
	DescriptorHeapElementRef		get_counter_srv_handle()const { return counter_; }

	void reset_counter(ID3D12GraphicsCommandList* command_list, ID3D12Resource* src_resource, uint64_t src_offset)
	{
		assert((get_state() == D3D12_RESOURCE_STATE_COMMON) || !!(get_state() & D3D12_RESOURCE_STATE_COPY_DEST));
		assert(command_list && src_resource);
		command_list->CopyBufferRegion(resource_.Get(), counter_offset_bytes_, src_resource, src_offset, sizeof(uint32_t));
	}
};

template<typename Element, typename Buffer> static bool Construct(
	Buffer& out_buffer,
	const Element* elements,
	std::size_t elements_num,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* command_list,
	UploadBuffer& upload_buffer,
	DescriptorHeap* descriptor_heap = nullptr,
	std::optional<D3D12_RESOURCE_STATES> final_state = {})
{
	assert(elements_num && device && command_list);
	out_buffer.initialize(elements_num, sizeof(Element));
	if(elements)
	{
		const auto offset = upload_buffer.data_to_upload(elements, out_buffer.size(), alignof(Element));
		if (!offset.has_value())
		{
			out_buffer.destroy();
			return false;
		}
		out_buffer.create_resource(device);
		out_buffer.create_views(device, descriptor_heap);
		command_list->CopyBufferRegion(out_buffer.get_resource(), 0, upload_buffer.get_resource(), offset.value(), out_buffer.size());
	}
	else
	{
		out_buffer.create_resource(device);
		out_buffer.create_views(device, descriptor_heap);
	}

	if constexpr (std::is_same_v<Buffer, UavCountedBuffer>)
	{
		const uint32_t counter_value = elements ? out_buffer.elements_num() : 0;
		const auto offset = upload_buffer.data_to_upload(&counter_value, sizeof(uint32_t), alignof(uint32_t));
		if (!offset.has_value())
		{
			out_buffer.destroy();
			return false;
		}
		out_buffer.reset_counter(command_list, upload_buffer.get_resource(), offset.value());
	}

	const D3D12_RESOURCE_STATES new_state = final_state.value_or(Buffer::kDefaultState);
	if (new_state != out_buffer.get_state())
	{
		D3D12_RESOURCE_BARRIER barriers[] = { out_buffer.transition_barrier(new_state) };
		command_list->ResourceBarrier(_countof(barriers), barriers);
	}
	return true;
}

template<typename Element, typename Buffer> 
static bool FillGpuContainer(Buffer& buffer,
	const Element* elements, std::size_t elements_num, ID3D12GraphicsCommandList* command_list,
	UploadBuffer& upload_buffer, uint32_t dst_offset = 0
	//, std::optional<D3D12_RESOURCE_STATES> final_state = {}
	)
{
	assert(elements && elements_num);
	assert(command_list);
	const auto local_size = sizeof(Element) * elements_num;
	assert((local_size + dst_offset) <= buffer.size());

	const auto upload_offset = upload_buffer.data_to_upload(elements, local_size, alignof(Element));
	if (!upload_offset.has_value())
		return false;

	assert((buffer.get_state() == D3D12_RESOURCE_STATE_COMMON) || !!(buffer.get_state() & D3D12_RESOURCE_STATE_COPY_DEST));
	command_list->CopyBufferRegion(buffer.get_resource(), dst_offset, upload_buffer.get_resource()
		, upload_offset.value(), local_size);
	return true;
}

struct TransitionManager
{
private:
	ComPtr<ID3D12GraphicsCommandList> command_list_;
	std::vector<D3D12_RESOURCE_BARRIER> pending_;
	std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_BARRIER> started_;

	static ID3D12Resource* get_resource(const D3D12_RESOURCE_BARRIER& b) { return b.Transition.pResource; }

	template<typename... TTail>
	void move_to_pending(ID3D12Resource* resource, TTail... tail)
	{
		auto found_it = started_.find(resource);
		if (found_it != started_.end())
		{
			pending_.push_back(std::move(found_it->second));
			started_.erase(found_it);
		}
		if constexpr (0 != sizeof...(TTail))
		{
			move_to_pending(tail...);
		}
	}

	template<typename... TTail>
	void move_to_pending(const D3D12_RESOURCE_BARRIER& barrier, TTail... tail)
	{
		assert(started_.find(get_resource(barrier)) == started_.end());
		assert(barrier.Flags == D3D12_RESOURCE_BARRIER_FLAG_NONE);
		pending_.push_back(barrier);
		if constexpr (0 != sizeof...(TTail))
		{
			move_to_pending(tail...);
		}
	}

	void execute_pending(uint64_t begin_only_num)
	{
		if (!pending_.size())
			return;

		assert(command_list_);
		command_list_->ResourceBarrier(static_cast<uint32_t>(pending_.size()), &pending_[0]);

		assert(begin_only_num <= pending_.size());
		for (uint32_t it = 0; it < begin_only_num; it++)
		{
			auto& b = pending_[it];
			ID3D12Resource* res = get_resource(b);
			assert(b.Flags == D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);
			b.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
			started_.insert_or_assign(std::move(res), std::move(b));
		}
		pending_.clear();
	}

public:
	~TransitionManager()
	{
		assert(!pending_.size());
		assert(!started_.size());
	}

	void set_command_list(ComPtr<ID3D12GraphicsCommandList> command_list)
	{
		command_list_ = command_list;
	}

	void clear()
	{
		pending_.clear();
		started_.clear();
	}

	void insert(D3D12_RESOURCE_BARRIER&& in_barrier)
	{
		auto same_resource = [resource = get_resource(in_barrier)](const D3D12_RESOURCE_BARRIER& b)
			{ return resource == get_resource(b); };
		assert(std::find_if(pending_.begin(), pending_.end(), same_resource) == pending_.end());
		assert(started_.find(get_resource(in_barrier)) == started_.end());
		pending_.push_back(in_barrier);
		auto& barrier = pending_.back();
		assert(barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
		assert(barrier.Flags == D3D12_RESOURCE_BARRIER_FLAG_NONE || barrier.Flags == D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;	
	}

	void start()
	{
		const auto begin_only_num = pending_.size();
		execute_pending(begin_only_num);
	}

	template<typename... TTail>
	void wait_for(TTail... tail)
	{
		const auto begin_only_num = pending_.size();
		pending_.reserve(begin_only_num + sizeof...(TTail));
		move_to_pending(tail...);
		execute_pending(begin_only_num);
	}

	void wait_for_all_started()
	{
		const auto begin_only_num = pending_.size();

		pending_.reserve(begin_only_num + started_.size());
		for (auto iter = started_.begin(); iter != started_.end(); ++iter)
		{
			pending_.push_back(std::move(iter->second));
		}
		started_.clear();

		execute_pending(begin_only_num);
	}
};