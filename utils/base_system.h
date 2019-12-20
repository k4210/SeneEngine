#pragma once
#include "third_party/bitset2/bitset2.hpp" 

class IBaseSystem
{
public:
	virtual void Open() = 0;
	virtual void Close() = 0;
	virtual void Destroy() = 0;
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
	void Destroy() override {}
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

	void Free(T* component)
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

struct Transform
{

};

/*
struct Archive
{
	// data offset
	// shared_ptr to DefaultData asset  
};



struct ComponentHandle
{
	// component ptr <-- How to fill: callback msg or allocate during construction? Or map id, each time? future/promise?

	bool IsValid() const;
	void Initialize(EntityId, Archive);
	//void CustomInitialize(EntityId, ...)
	void CleanUp();
//optional
	void UpdateTransform(Transform);
	void CreateComponent(EntityId);
};

struct SceneEntity
{
	virtual ~SceneEntity() = default;

	void Serialize();

	virtual void Initialize() = 0; //
	virtual void Cleanup() = 0;
	virtual void UpdateTransform(Transform) = 0;

	EntityId id;
	Transform transform;
	// flags
	// components
};
*/