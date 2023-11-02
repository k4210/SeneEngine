#include "stdafx.h"
#include "statistics_system.h"

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
		switch (mode)
		{
		case Stat::EMode::Accumulate:
		case Stat::EMode::PerFrame:
			var += value;
			break;
		case Stat::EMode::Override:
			var = value;
			break;
		}
	}

	void System::HandleCommonMessage(const CommonMsg::Message msg)
	{
		if (const CommonMsg::Frame* frame = std::get_if<CommonMsg::Frame>(&msg))
		{
			LOG(stat_log, ELogPriority::Display, "frame {}", frame->delta);

			buffers_.FlipActive();

			auto& full_bufff = buffers_.GetInactive();
			const std::span<const Stat::Details> details = Stat::Id::AllStatsDetails();
			for (uint32 idx = 0; idx < details.size(); idx++)
			{
				const Stat::Details& detail = details[idx];
				const double value = full_bufff[idx].load(std::memory_order_relaxed);
				LOG(stat_log, ELogPriority::Display, "{}:{} {}", detail.group, detail.name, value);

				if (detail.mode == Stat::EMode::PerFrame)
				{
					full_bufff[idx].store(0.0f, std::memory_order_relaxed);
				}
			}
		}
	}

	void System::operator()(StartDisplay msg)
	{}

	void System::operator()(StopDisplay msg)
	{}

}