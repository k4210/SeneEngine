#include "stdafx.h"
#include "statistics_system.h"
#if DO_STAT
namespace Statistics
{
	IF_DO_LOG(LogCategory stat_log("stat");)
	System* g_instance = nullptr;

	IBaseSystem* CreateSystem()
	{
		g_instance = new System();
		return g_instance;
	}

	void System::Destroy()
	{
		g_instance = nullptr;
	}

	void PassStat(uint32 index, Stat::EMode mode, double value)
	{
		g_instance->ReceiveStat(index, mode, value);
	}

	void System::ThreadInitialize()
	{
		Stat::Id::SetHandleStatsFunction(&PassStat);
	}

	void System::ThreadCleanUp()
	{
		Stat::Id::SetHandleStatsFunction(nullptr);
	}

	void System::ReceiveStat(uint32 index, Stat::EMode mode, double value)
	{
		auto& var = buffers_.GetActive()[index];
		var.counter++;
		switch (mode)
		{
		case Stat::EMode::Accumulate:
		case Stat::EMode::PerFrame:
			var.value += value;
			break;
		case Stat::EMode::Override:
			var.value = value;
			break;
		}
	}

	void System::operator()(CommonMsg::Message msg)
	{
		if (const CommonMsg::Frame* frame = std::get_if<CommonMsg::Frame>(&msg))
		{
			LOG(stat_log, ELog::Display, "frame {}", Utils::ToMiliseconds(frame->delta));

			buffers_.FlipActive();

			auto& full_bufff = buffers_.GetInactive();
			const std::span<const Stat::Details> details = Stat::Id::AllStatsDetails();
			for (uint32 idx = 0; idx < details.size(); idx++)
			{
				const Stat::Details& detail = details[idx];
				const double value = full_bufff[idx].value.load(std::memory_order_relaxed);
				const uint32 counter = full_bufff[idx].counter.load(std::memory_order_relaxed);
				LOG(stat_log, ELog::Display, "{}:{} {} / {}", detail.group, detail.name, value, counter);

				if (detail.mode == Stat::EMode::PerFrame)
				{
					full_bufff[idx].value.store(0.0f, std::memory_order_relaxed);
					full_bufff[idx].counter.store(0, std::memory_order_relaxed);
				}
			}
		}
	}

	void System::operator()(StartDisplay msg)
	{}

	void System::operator()(StopDisplay msg)
	{}

}
#endif