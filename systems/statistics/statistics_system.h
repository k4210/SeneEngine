#pragma once

#include "base_system.h"
#include "stat/stat.h"
#include "small_container.h"
#include <variant>

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
	using Message = std::variant<StartDisplay, StopDisplay>;

	class System : public BaseSystemImpl<Message>
	{
	public:
		void ThreadInitialize() override;

		void ThreadCleanUp() override;

		void HandleCommonMessage(CommonMsg::Message) override;

		void HandleSingleMessage(Message& msg) override { std::visit([&](auto&& arg) { (*this)(std::move(arg)); }, msg); }

		void Destroy() override;

		std::string_view GetName() const override { return "Statistics"; }

		// Thread safe
		void ReceiveStat(uint32 index, Stat::EMode mode, double value);
	protected:
		void operator()(StartDisplay msg);

		void operator()(StopDisplay msg);

		using ValueArray = std::array<std::atomic<double>, 1024>;
		Twins<ValueArray> buffers_;
	};

	IBaseSystem* CreateSystem();
}