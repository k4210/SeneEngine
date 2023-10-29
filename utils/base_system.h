#pragma once
#include "utils/small_container.h"
#include "utils/math/Transform.h"
#include "utils/message_queue.h"
#include <optional>

namespace CommonMsg
{
	enum State
	{
		Playing,
		Loading,
		InGameMenu,
		MainMenu,
		Background
	};
	struct Tick
	{
		uint64_t frame_id;
		double delta;
		double real_time;
		//time to sync ?
	};
	struct StateChange
	{
		State actual_state;
	};
	struct ConfigReload
	{
		
	};
	using Message = std::variant<Tick, StateChange>;
}

class IBaseSystem
{
public:
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void Destroy() = 0;
	virtual bool IsRunning() const = 0;

	virtual void HandleCommonMessage(CommonMsg::Message) = 0;

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
	//LockFreeQueue_SingleConsumer<TMsg, 32> msg_queue_;
	TMessageQueue<TMsg> msg_queue_;
	std::thread thread_;

protected:
	bool open_ = false;

	virtual ~BaseSystemImpl() = default;
	virtual void HandleSingleMessage(TMsg&) = 0;

	virtual void Tick() {}
	virtual void ThreadInitialize() {}
	virtual void ThreadCleanUp() {}
	//Executed on main thread
	virtual void CustomOpen() {}
	virtual void CustomClose() {}
	void Destroy() override {}

	virtual std::optional<Microsecond> GetMessageBudget() const { return {}; }

	void HandleMessages()
	{
		std::optional<Microsecond> budget = GetMessageBudget();
		if (!budget)
		{
			while (auto opt_msg = msg_queue_.Pop())
			{
				HandleSingleMessage(*opt_msg);
			}
			return;
		}

		const Microsecond time_budget = *budget;
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
		msg_queue_.Preallocate(64);
		ThreadInitialize();
		while (open_)
		{
			HandleMessages();
			Tick();
		}
		ThreadCleanUp();
	}
	
	void Open() override { CustomOpen(); open_ = true; thread_ = std::thread(&BaseSystemImpl::SystemLoop, this); }
	void Close() override { open_ = false; thread_.join(); CustomClose(); }
public:
	bool IsOpen() const override { return open_; }
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

	template<typename E> static void UpdateTransform(E& entity, Math::Transform transform)
	{
		auto func = [transform](auto& comp) { comp.UpdateTransform(transform); };
		entity.for_each_component(func);
	}
};