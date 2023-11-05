#pragma once

#include <atomic>
#include <array>
#include <algorithm>
#include "third_party/bitset2/bitset2.hpp" 
#include "common/base_types.h"

template<typename T>
struct Twins
{
private:
	std::array<T, 2> buffers_;
	std::atomic<bool> second_active_ = false;

	inline bool IsSecondActive() const
	{
		return second_active_.load(std::memory_order_relaxed);
	}

	inline uint32 GetActiveIndex() const
	{
		return IsSecondActive() ? 1 : 0;
	}

public:
			void	FlipActive()			{ second_active_ = !IsSecondActive(); }

			T&		GetActive()				{ return buffers_[GetActiveIndex()]; }
	const	T&		GetActive()		const	{ return buffers_[GetActiveIndex()]; }
			T&		GetInactive()			{ return buffers_[1 - GetActiveIndex()]; }
	const	T&		GetInactive()	const	{ return buffers_[1 - GetActiveIndex()]; }

	const	auto	begin()			const	{ return buffers_.begin(); }
			auto	begin()					{ return buffers_.begin(); }
	const	auto	end()			const	{ return buffers_.end(); }
			auto	end()					{ return buffers_.end(); }
};

template<typename T> void RemoveSwap(std::vector<T>& vec, std::size_t idx)
{
	assert(vec.size() > idx);
	const std::size_t last_valid_idx = vec.size() - 1;
	if (idx != last_valid_idx)
	{
		vec[idx] = std::move(vec[last_valid_idx]);
	}
	vec.pop_back();
}

template<typename T, uint32_t kMinInlineCapacity = 1>
class small_container
{
	using RawData = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

public:
	static constexpr uint32_t kInlineCapacity = std::max<uint32_t>(sizeof(RawData*) / sizeof(T), kMinInlineCapacity);
	static_assert(kInlineCapacity >= 1, "wrong size");

private:
	union
	{
		RawData inline_[kInlineCapacity];
		RawData* allocation_;
	} data_;
	uint32_t num_ = 0;
	uint32_t allocation_size_ = 0;

	const	RawData*	get_mem()					const	{ return allocation_size_ ? data_.allocation_ : data_.inline_; }
			RawData*	get_mem()							{ return allocation_size_ ? data_.allocation_ : data_.inline_; }
	const	T*			get_ptr(std::size_t pos)	const	{ return reinterpret_cast<const T*>(get_mem() + pos); }
			T*			get_ptr(std::size_t pos)			{ return reinterpret_cast<		T*>(get_mem() + pos); }
	std::size_t			max_size()					const	{ return allocation_size_ ? allocation_size_ : kInlineCapacity; }

	void resize_allocation(std::size_t new_capacity)
	{
		assert(new_capacity >= num_);
		const bool inline_alloc = new_capacity <= kInlineCapacity;
		if (inline_alloc && !allocation_size_)
			return;
		RawData* const old_mem = get_mem();
		RawData* const new_allocation = inline_alloc ? data_.inline_ : new RawData[new_capacity];
		auto get_old_ptr = [old_mem](std::size_t idx) -> T* { return reinterpret_cast<T*>(old_mem + idx); };
		for (std::size_t idx = 0; idx < num_; idx++)
		{
			T* const ptr = get_old_ptr(idx);
			new (new_allocation + idx) T(std::move(*ptr));
			ptr->~T();
		}
		if (allocation_size_)
		{
			delete old_mem;
		}
		if (!inline_alloc)
		{
			data_.allocation_ = new_allocation;
		}
		allocation_size_ = inline_alloc ? 0 : static_cast<uint32_t>(new_capacity);
	}

	RawData* next_place()
	{
		const auto current_capacity = max_size();
		assert(current_capacity >= num_);
		if (current_capacity == num_)
		{
			resize_allocation(std::max<std::size_t>(2 * current_capacity, 16));
		}
		return get_mem() + num_++;
	}

public:
	const	T&			operator[](std::size_t pos)	const	{ assert(num_ > pos); return *get_ptr(pos); }
			T&			operator[](std::size_t pos)			{ assert(num_ > pos); return *get_ptr(pos); }
	const	T*			begin()						const	{ return get_ptr(0); }
			T*			begin()								{ return get_ptr(0); }
	const	T*			end()						const	{ return get_ptr(num_); }
			T*			end()								{ return get_ptr(num_); }
			std::size_t size()						const	{ return num_; }
			std::size_t capacity()					const	{ return max_size(); }

	template<typename ...Args> 
	void emplace_back	(Args&&... args)	{ new(next_place()) T(std::forward<Args>(args)...); }
	void add			(T&& element)		{ new(next_place()) T(std::forward<T>(element)); }

	void shrink(uint32_t slack = 0)
	{
		const uint32_t new_num = num_ + slack;
		if (new_num < max_size())
		{
			resize_allocation(new_num);
		}
	}

	void reset()
	{
		for (T& it : *this) 
		{ 
			it.~T(); 
		}
		num_ = 0;
	}

	void reserve(uint32_t new_capacity)
	{
		if (new_capacity > max_size())
		{
			resize_allocation(new_capacity);
		}
	}

	std::size_t iterator_to_index(T* item) const
	{
		return std::distance(get_mem(), reinterpret_cast<const RawData*>(item));
	}

	void remove(std::size_t idx, bool swap)
	{
		assert(idx < num_);
		const std::size_t last_idx = num_ - 1;
		T* const last_element = get_ptr(last_idx);
		if (idx < last_idx)
		{
			if (swap)	{ *get_ptr(idx) = std::move(*last_element); }
			else		{ std::move(get_ptr(idx + 1), get_ptr(num_), get_ptr(idx)); }
		}
		last_element->~T();
		num_--;
	}

	std::optional<T> pop()
	{
		if (num_)
		{
			T* const last_element = get_ptr(num_ - 1);
			std::optional<T> result(std::move(*last_element));
			last_element->~T();
			num_--;
			return result;
		}
		return {};
	}

	~small_container() { reset(); resize_allocation(0); }
};

template<class T, int N>
struct preallocated_container
{
	using RawData = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

private:
	Bitset2::bitset2<N> free_;
	std::array<RawData, N> data_;
	std::mutex mutex_;

public:
	T* safe_get_data() { return reinterpret_cast<T*>(&data_[0]); }
	const T* safe_get_data() const { return reinterpret_cast<const T*>(&data_[0]); }

	template<typename ...Args> T* allocate(Args&&... args)
	{
		const auto idx = free_.find_first();
		if (idx == Bitset2::bitset2<N>::npos)
			return nullptr;
		free_.set(idx, false);
		return new (&data_[idx]) T(std::forward<Args>(args)...);
	}

	void free(T* item)
	{
		assert(item);
		const auto idx = safe_get_index(item);
		assert(!free_.test(idx));
		free_.set(idx, true);
		item->~T();
	}

	uint32_t safe_get_index(const T* item) const
	{
		assert(item);
		const auto idx = static_cast<uint32_t>(std::distance(&data_[0], reinterpret_cast<const RawData*>(item)));
		assert((idx < N) && (idx >= 0));
		return idx;
	}

	bool is_set(const T* item) const
	{
		const auto idx = safe_get_index(item);
		return (idx < N) && (idx >= 0) && !free_.test(idx);
	}

	bool is_set(const std::size_t idx) const
	{
		return (idx < N) && (idx >= 0) && !free_.test(idx);
	}

	template<typename ...Args> T* safe_allocate(Args&&... args)
	{ 
		std::lock_guard guard(mutex_); 
		return allocate(std::forward<Args>(args)...);
	}

	void safe_free(T* component) 
	{ 
		std::lock_guard guard(mutex_); 
		free(component);
	}

	template<typename F> void for_each(F& func)
	{
		const auto occupied = ~free_;
		for (auto idx = occupied.find_first(); idx != Bitset2::bitset2<N>::npos; idx = occupied.find_next(idx))
		{
			func(safe_get_data()[idx]);
		}
	}

	void reset()
	{
		auto reset_single = [](T& component) { component.~T(); };
		for_each(reset_single);
		free_.set();
	}

	void safe_reset()
	{
		std::lock_guard guard(mutex_);
		reset();
	}

	preallocated_container() 
	{ 
		free_.set(); 
	}

	~preallocated_container()
	{
		reset();
	}

	const	T& operator[](std::size_t idx)	const	{	assert(N > idx); assert(!free_.test(idx)); return safe_get_data()[idx]; }
			T& operator[](std::size_t idx)			{	assert(N > idx); assert(!free_.test(idx)); return safe_get_data()[idx]; }

	std::size_t find_first_free() const
	{
		return free_.find_first();
	}

	std::size_t find_next_taken(std::size_t start) const
	{
		return (~free_).find_next(start);
	}
};