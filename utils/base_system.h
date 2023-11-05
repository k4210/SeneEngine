#pragma once
#include "small_container.h"
#include "message_queue.h"
#include <optional>
#include "mathfu/mathfu.h"
#include "common_msg.h"
#include "common/utils.h"

class IBaseSystem
{
public:
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void Destroy() = 0;
	virtual bool IsRunning() const = 0;

	virtual void HandleCommonMessage(CommonMsg::Message) = 0;

	virtual std::string_view GetName() const = 0;
	virtual ~IBaseSystem() = default;
};

template<class TMsg>
class BaseSystemImpl : public IBaseSystem
{
	LockFreeQueue_SingleConsumer<TMsg, 32> msg_queue_;
	//TMessageQueue<TMsg> msg_queue_;
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

	virtual std::optional<Utils::TimeSpan> GetMessageBudget() const { return {}; }

	virtual void HandleCommonMessage(CommonMsg::Message) {}

	void HandleMessages()
	{
		std::optional<Utils::TimeSpan> budget = GetMessageBudget();
		if (!budget)
		{
			while (auto opt_msg = msg_queue_.Pop())
			{
				HandleSingleMessage(*opt_msg);
			}
			return;
		}

		const Utils::TimeSpan time_budget = *budget;
		const auto start_time = Utils::GetTime();
		while (auto opt_msg = msg_queue_.Pop())
		{
			HandleSingleMessage(*opt_msg);
			if ((Utils::GetTime() - start_time) > time_budget)
				break;
		}
	}

	void SystemLoop()
	{
		//msg_queue_.Preallocate(64);
		ThreadInitialize();
		while (open_)
		{
			HandleMessages();
			Tick();
		}
		ThreadCleanUp();
	}
	
	void Start() override { CustomOpen(); open_ = true; thread_ = std::thread(&BaseSystemImpl::SystemLoop, this); }
	void Stop() override { open_ = false; thread_.join(); CustomClose(); }
public:
	bool IsRunning() const override { return open_; }
	void EnqueueMsg(TMsg&& msg) { msg_queue_.Enqueue(std::forward<TMsg>(msg)); }
};
