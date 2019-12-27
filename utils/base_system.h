#pragma once
#include "third_party/bitset2/bitset2.hpp" 
#include "utils/small_container.h"

class IBaseSystem
{
public:
	virtual void Open() = 0;
	virtual void Close() = 0;
	virtual void destroy() = 0;
	virtual bool IsOpen() const = 0;
	//get id..
	virtual ~IBaseSystem() = default;
};

using Microsecond = std::chrono::microseconds;
inline Microsecond GetTime()
{
	return std::chrono::duration_cast<Microsecond>(std::chrono::system_clock::now().time_since_epoch());
}

template<class TMsg>
class BaseSystemImpl : public IBaseSystem
{
protected:
	LockFreeQueue_SingleConsumer<TMsg, 32> msg_queue_;
	std::thread thread_;
	bool open_ = false;

	virtual ~BaseSystemImpl() = default;
	virtual void HandleSingleMessage(TMsg&) = 0;

	virtual void Tick() {}
	virtual void ThreadInitialize() {}
	virtual void ThreadCleanUp() {}
	virtual void CustomOpen() {}
	virtual void CustomClose() {}
	void destroy() override {}
	virtual Microsecond GetMessageBudget() const { return Microsecond{ 7000 }; }

	void HandleMessages()
	{
		const Microsecond time_budget = GetMessageBudget();
		const Microsecond start_time = GetTime();
		while (auto opt_msg = msg_queue_.Pop())
		{
			HandleSingleMessage(*opt_msg);
			if ((GetTime() - start_time) > time_budget)
				break;
		}
	}
	void SystemLoop()
	{
		ThreadInitialize();
		while (open_)
		{
			HandleMessages();
			Tick();
		}
		ThreadCleanUp();
	}
	
	void Open() override { CustomOpen(); open_ = true; thread_ = std::thread(&BaseSystemImpl::SystemLoop, this); }
	bool IsOpen() const override { assert(thread_.joinable() == open_); return open_; }
	void Close() override { open_ = false; thread_.join(); CustomClose(); }
public:
	void EnqueueMsg(TMsg&& msg) { msg_queue_.Enqueue(std::forward<TMsg>(msg)); }
};

template<class... Ts> struct visitor : Ts... { using Ts::operator()...; };
template<class... Ts> visitor(Ts...)->visitor<Ts...>;

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

template<class T, int N>
struct PreallocatedContainer
{
private:
	Bitset2::bitset2<N> free_;
	std::array<T, N> components_;
	std::mutex mutex_;
public:
	PreallocatedContainer()
	{
		free_.set();
	}

	T* UnsafeAllocate()
	{
		const auto idx = free_.find_first();
		if (idx == Bitset2::bitset2<N>::npos)
			return nullptr;
		free_.set(idx, false);
		return &components_[idx];
	}

	void UnsafeFree(T* component)
	{
		assert(component);
		const auto idx = std::distance(&components_[0], component);
		assert(idx < N && idx >= 0);
		assert(!free_.test(idx));
		free_.set(idx, true);
	}

	T* Allocate()
	{
		std::lock_guard guard(mutex_);
		return UnsafeAllocate();
	}

	void free(T* component)
	{
		std::lock_guard guard(mutex_);
		UnsafeFree(component);
	}
};

struct EntityId
{
	uint32_t index = 0xffffffff;
	uint32_t generation = 0xffffffff;
};

/*
struct Transform
{
	XMFLOAT3 translate = XMFLOAT3(0, 0, 0);
	XMFLOAT4 rotation = XMFLOAT4(0, 0, 0, 1); // quaternion
	float scale = 1.0f;
};

struct Archive
{
	// data offset
	// shared_ptr to DefaultData asset  
};
*/

template<typename C> struct ComponentHandleBase
{
protected:
	C* component_ = nullptr;

public:
	bool IsValid() const { return !!component_; };

	ComponentHandleBase() = default;

	ComponentHandleBase(ComponentHandleBase&& other) : ComponentHandleBase() { std::swap(component_, other.component_); }
	ComponentHandleBase& operator=(ComponentHandleBase&& other) { std::swap(component_, other.component_); }

	ComponentHandleBase(const ComponentHandleBase&) = delete;
	ComponentHandleBase& operator=(const ComponentHandleBase&) = delete;

	//EntityId GetEntityId() const;
	//void Initialize();

	void Cleanup() {}
	//void UpdateTransform(Transform);
};

struct EntitiUtils
{
	template<typename E> static bool AllComponentsValid(const E& entity)
	{
		bool valid = true;
		auto func = [&valid](const auto& comp) { valid = valid && comp.IsValid(); };
		const_cast<E&>(entity).ForEachComponent(func);
		return valid;
	}

	template<typename E> static void Cleanup(E& entity)
	{
		auto func = [](auto& comp) { comp.Cleanup(); };
		entity.ForEachComponent(func);
	}
};

class SampleEntity
{
	ComponentHandleBase<uint64_t> field;
	std::optional<ComponentHandleBase<int32_t>> optional_field;
	SmallContainer<ComponentHandleBase<float>> vector_field;
public:

	template<typename F> 
	void ForEachComponent(F& func)
	{
		func(field);
		if (optional_field)
		{
			func(*optional_field);
		}
		for (auto& handle : vector_field)
		{
			func(handle);
		}
	}

	bool Valid() const { return EntitiUtils::AllComponentsValid(*this); }
	void Cleanup() { EntitiUtils::Cleanup(*this); }
};
