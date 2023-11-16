#pragma once

#include "base_system.h"
#include "stat/stat.h"
#include "small_container.h"
#include <variant>
#include <bitset>

#if DO_STAT
namespace Statistics
{
	struct StartDisplay
	{
		std::string group_name;
	};
	struct StopDisplay
	{
		std::string group_name;
	};
	using Message = std::variant<StartDisplay, StopDisplay, CommonMsg::Message>;

	class System : public BaseSystemImpl<Message>
	{
	public:
		void ThreadInitialize() override;

		void ThreadCleanUp() override;

		void ReceiveCommonMessage(CommonMsg::Message msg) override
		{
			EnqueueMsg(Message{ msg });
		}

		void HandleSingleMessage(Message& msg) override { std::visit([&](auto&& arg) { (*this)(std::move(arg)); }, msg); }

		void Destroy() override;

		std::string_view GetName() const override { return "Statistics"; }

		// Thread safe
		void ReceiveStat(uint32 index, Stat::EMode mode, double value);
	protected:
		void System::operator()(CommonMsg::Message msg);

		void operator()(StartDisplay msg);

		void operator()(StopDisplay msg);
		struct StatValue
		{
			std::atomic<double> value = 0.0;
			std::atomic<uint32> counter = 0;
		};
		using ValueArray = std::array<StatValue, Stat::kMaxSupportedStats>;
		Twins<ValueArray> buffers_;

		std::bitset<Stat::kMaxSupportedStats> enabled_;
	};

	IBaseSystem* CreateSystem();
}
#endif