#pragma once
#include "utils/small_container.h"
#include "utils/math.h"

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

struct EntityUtils
{
	template<typename E> static bool AllComponentsValid(const E& entity)
	{
		bool valid = true;
		auto func = [&valid](const auto& comp) { valid = valid && comp.IsValid(); };
		const_cast<E&>(entity).for_each_component(func);
		return valid;
	}

	template<typename E> static void Cleanup(E& entity)
	{
		auto func = [](auto& comp) { comp.Cleanup(); };
		entity.for_each_component(func);
	}

	template<typename E> static void UpdateTransform(E& entity, Transform transform)
	{
		auto func = [transform](auto& comp) { comp.UpdateTransform(transform); };
		entity.for_each_component(func);
	}
};