#include "stdafx.h"
#include "gpu_containers.h"

void DescriptorHeap::create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, uint32_t capacity, uint32_t initially_allocated)
{
	assert(device);
	assert(!heap_);
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc{ type, capacity, flags, 0 };
	ThrowIfFailed(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap_)));
	type_ = type;
	descriptor_size_ = device->GetDescriptorHandleIncrementSize(type);

	taken_slots_.resize(capacity, false);
	assert(initially_allocated <= capacity);
	for (uint32_t idx = 0; idx < initially_allocated; idx++)
	{
		taken_slots_[idx] = true;
	}
}

uint32_t DescriptorHeap::allocate()
{
	auto found = std::find(taken_slots_.begin(), taken_slots_.end(), false);
	if (found != taken_slots_.end())
	{
		*found = true;
		return static_cast<uint32_t>(std::distance(taken_slots_.begin(), found));
	}
	return Const::kInvalid;
}

void DescriptorHeap::free(uint32_t idx)
{
	assert(is_set(idx));
	taken_slots_[idx] = false;
}

void DescriptorHeap::copy(ID3D12Device* device, uint32_t num, uint32_t dst_start, uint32_t src_start, const DescriptorHeap& src_heap)
{
	assert(device);
	assert(type_ == src_heap.type_);
	assert(dst_start + num <= get_capacity());
	assert(src_start + num <= src_heap.get_capacity());
	assert([&]() -> bool
	{
		for (uint32_t idx = 0; idx < num; idx++)
		{
			if (!src_heap.is_set(src_start + idx))
				return false;
		}
		return true;
	}());

	device->CopyDescriptorsSimple(num, get_cpu_handle(dst_start), src_heap.get_cpu_handle(src_start), type_);
	for (uint32_t idx = 0; idx < num; idx++) 
	{ 
		taken_slots_[dst_start + idx] = true;
	}
}

void CommitedBuffer::create_resource(ID3D12Device* device)
{
	assert(device);
	assert(element_size_);
	assert(elements_num_);

	const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size());
	const CD3DX12_HEAP_PROPERTIES heap_prop(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(&heap_prop
		, D3D12_HEAP_FLAG_NONE, &desc, state_, nullptr, IID_PPV_ARGS(&resource_)));
}

void ConstantBuffer::create_resource(ID3D12Device* device)
{
	assert(device);
	assert(element_size_);
	assert(elements_num_);

	const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(whole_buffer_size());
	const CD3DX12_HEAP_PROPERTIES heap_prop(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(&heap_prop
		, D3D12_HEAP_FLAG_NONE, &desc, state_, nullptr, IID_PPV_ARGS(&resource_)));
}

void ConstantBuffer::create_views(ID3D12Device* device, DescriptorHeap* descriptor_heap)
{
	assert(descriptor_heap);
	assert(resource_);
	assert(device);
	const bool ok = cbv_.initialize(*descriptor_heap);
	assert(ok);
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.BufferLocation = resource_->GetGPUVirtualAddress();
	desc.SizeInBytes = whole_buffer_size();
	device->CreateConstantBufferView(&desc, cbv_.get_cpu_handle());
}

void StructBuffer::create_views(ID3D12Device* device, DescriptorHeap* descriptor_heap)
{
	assert(descriptor_heap);
	assert(resource_);
	const bool ok = srv_.initialize(*descriptor_heap);
	assert(ok);
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Buffer.NumElements = elements_num_;
	desc.Buffer.StructureByteStride = element_size_;
	desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	desc.Buffer.FirstElement = 0;
	device->CreateShaderResourceView(resource_.Get(), &desc, srv_.get_cpu_handle());
}

void UavCountedBuffer::create_views(ID3D12Device* device, DescriptorHeap* descriptor_heap)
{
	assert(descriptor_heap);
	assert(resource_);
	const bool ok = uav_.initialize(*descriptor_heap);
	assert(ok);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = elements_num_;
	uavDesc.Buffer.StructureByteStride = element_size_;
	uavDesc.Buffer.CounterOffsetInBytes = counter_offset_bytes_;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	device->CreateUnorderedAccessView(resource_.Get(), resource_.Get(), &uavDesc, uav_.get_cpu_handle());
}

uint64_t UploadBuffer::align(uint64_t uLocation, uint64_t uAlign)
{
	if ((0 == uAlign) || (uAlign & (uAlign - 1)))
	{
		throw std::runtime_error("non-pow2 alignment");
	}
	return ((uLocation + (uAlign - 1)) & ~(uAlign - 1));
}

void UploadBuffer::initialize(ID3D12Device* device, uint32_t size)
{
	assert(device);
	const CD3DX12_HEAP_PROPERTIES heap_prop(D3D12_HEAP_TYPE_UPLOAD);
	const CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
	ThrowIfFailed(device->CreateCommittedResource( &heap_prop, D3D12_HEAP_FLAG_NONE,
		&resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_buffer_)));
	NAME_D3D12_OBJECT(upload_buffer_);
	void* data = nullptr;
	CD3DX12_RANGE read_range(0, 0);
	upload_buffer_->Map(0, &read_range, &data);
	current_pos_ = data_begin_ = reinterpret_cast<UINT8*>(data);
	data_end_ = data_begin_ + size;
}

std::optional<std::tuple<uint64_t, UINT8*>> UploadBuffer::reserve_space(std::size_t size, std::size_t alignment)
{
	static_assert(sizeof(uint64_t) == sizeof(UINT8*), "only x64 is supported");
	UINT8* const local_begin = reinterpret_cast<UINT8*>(align(reinterpret_cast<uint64_t>(current_pos_), alignment));
	//assert(std::distance(data_begin_, data_end_) > size);
	if ((local_begin + size) > data_end_)
		return {};
	current_pos_ = local_begin + size;
	return { {std::distance(data_begin_, local_begin), local_begin} };
}

std::optional< uint64_t> UploadBuffer::data_to_upload(
	const void* src_data, std::size_t size, std::size_t alignment)
{
	auto reserved_mem = reserve_space(size, alignment);
	assert(reserved_mem);
	if (!reserved_mem)
		return {};
	auto [offset, dst_ptr] = *reserved_mem;
	memcpy(dst_ptr, src_data, size);
	return { offset };
}

void UploadBuffer::reset()
{
	current_pos_ = data_begin_;
}

void UploadBuffer::destroy()
{
	if (upload_buffer_)
		upload_buffer_->Unmap(0, nullptr);
	upload_buffer_.Reset();
	data_begin_ = nullptr;
	current_pos_ = nullptr;
	data_end_ = nullptr;
}