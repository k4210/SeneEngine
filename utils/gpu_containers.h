#pragma once

#include "d3dx12.h"
#include <wrl.h>
#include "base_app_helper.h"

using Microsoft::WRL::ComPtr;

namespace Const
{
	constexpr uint32_t kInvalid = 0xFFFFFFFF;
	constexpr uint64_t kInvalid64 = 0xFFFFFFFFFFFFFFFF;
}

struct DescriptorHeap
{
protected:
	ComPtr<ID3D12DescriptorHeap> heap_;
	uint32_t descriptor_size_ = 0;
	std::vector<bool> taken_slots_;

public:
	void create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, uint32_t capacity, uint32_t initially_allocated = 0);

	void destroy()
	{
		taken_slots_.clear();
		heap_.Reset();
		descriptor_size_ = 0;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(uint32_t idx) const
	{
		assert(heap_);
		assert(idx < taken_slots_.size());
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(heap_->GetCPUDescriptorHandleForHeapStart(), idx, descriptor_size_);
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(uint32_t idx) const
	{
		assert(heap_);
		assert(idx < taken_slots_.size());
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(heap_->GetGPUDescriptorHandleForHeapStart(), idx, descriptor_size_);
	}

	ID3D12DescriptorHeap* get_heap() const { return heap_.Get(); }

	uint32_t allocate();
	void free(uint32_t idx);
};

struct DescriptorHeapElement
{
private:
	DescriptorHeap* descriptor_heap_ = nullptr;
	uint32_t idx_ = Const::kInvalid;

public:
	CD3DX12_CPU_DESCRIPTOR_HANDLE get_cpu_handle() const
	{
		assert(descriptor_heap_ && (idx_ != Const::kInvalid));
		return descriptor_heap_->get_cpu_handle(idx_);
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE get_gpu_handle() const
	{
		assert(descriptor_heap_ && (idx_ != Const::kInvalid));
		return descriptor_heap_->get_gpu_handle(idx_);
	}

	void release()
	{
		if (descriptor_heap_ && idx_ != Const::kInvalid)
		{
			descriptor_heap_->free(idx_);
		}
		descriptor_heap_ = nullptr;
		idx_ = Const::kInvalid;
	}

	bool initialize(DescriptorHeap& desc_heap)
	{
		release();
		descriptor_heap_ = &desc_heap;
		idx_ = descriptor_heap_->allocate();
		return idx_ != Const::kInvalid;
	}

	DescriptorHeapElement() = default;
	DescriptorHeapElement(const DescriptorHeapElement&) = delete;
	DescriptorHeapElement& operator=(const DescriptorHeapElement&) = delete;
	DescriptorHeapElement(DescriptorHeapElement&& other)
	{
		descriptor_heap_ = other.descriptor_heap_;
		idx_ = other.idx_;
		other.descriptor_heap_ = nullptr;
		other.idx_ = Const::kInvalid;
	}
	DescriptorHeapElement& operator=(DescriptorHeapElement&& other)
	{
		std::swap(descriptor_heap_, other.descriptor_heap_);
		std::swap(idx_, other.idx_);
	}
	~DescriptorHeapElement() { release(); }

	operator bool() const { return descriptor_heap_ && (idx_ != Const::kInvalid); }
};

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
protected:
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

	void							destroy()						{ srv_.release(); CommitedBuffer::destroy(); }
	bool							is_ready()				const	{ return CommitedBuffer::is_ready() && srv_; }
	D3D12_CPU_DESCRIPTOR_HANDLE		get_srv_handle_cpu()	const	{ return srv_.get_cpu_handle(); }
	D3D12_GPU_DESCRIPTOR_HANDLE		get_srv_handle_gpu()	const	{ return srv_.get_gpu_handle(); }

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
	uint64_t counter_offset_ = Const::kInvalid;
	DescriptorHeapElement uav_;

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

	void							destroy()
	{
		counter_offset_ = Const::kInvalid64;
		uav_.release();
		StructBuffer::destroy();
	}
	void							create_resource(ID3D12Device* device)
	{
		counter_offset_ = align_for_uav_counter(size());
		const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(counter_offset_ + sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		const CD3DX12_HEAP_PROPERTIES heap_prop(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &desc, state_, nullptr, IID_PPV_ARGS(&resource_)));
	}
	void							create_views(ID3D12Device* device, DescriptorHeap* descriptor_heap);

	D3D12_CPU_DESCRIPTOR_HANDLE		get_uav_handle()		const { return uav_.get_cpu_handle(); }
	uint64_t						get_counter_offset()	const { return counter_offset_; }
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
		const uint32_t counter_value = elements ? static_cast<uint32_t>(elements_num) : 0;
		const auto offset = upload_buffer.data_to_upload(&counter_value, sizeof(uint32_t), alignof(uint32_t));
		if (!offset.has_value())
		{
			out_buffer.destroy();
			return false;
		}
		command_list->CopyBufferRegion(out_buffer.get_resource(), out_buffer.get_counter_offset(), upload_buffer.get_resource(), offset.value(), sizeof(uint32_t));
	}

	const D3D12_RESOURCE_STATES new_state = final_state.value_or(Buffer::kDefaultState);
	if (new_state != out_buffer.get_state())
	{
		D3D12_RESOURCE_BARRIER barriers[] = { out_buffer.transition_barrier(new_state) };
		command_list->ResourceBarrier(_countof(barriers), barriers);
	}
	return true;
}

