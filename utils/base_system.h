#pragma once

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
	void Close() override { CustomClose(); open_ = false; thread_.join(); }
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
