#include "stdafx.h"
#include "stat.h"

#if DO_STAT
#include <atomic>
namespace Stat
{
	struct AllStatsInfo
	{
		std::array<Details, 1024> details;
		std::atomic<uint32> filled = 0;
		std::mutex mutex;

		static AllStatsInfo& get()
		{
			static AllStatsInfo instance;
			return instance;
		}
	};

	uint32 Id::Register(const char* in_gruop, const char* in_name, EMode in_mode)
	{
		const Details details{ in_gruop, in_name, in_mode };
		AllStatsInfo& all_info = AllStatsInfo::get();
		{
			const std::lock_guard<std::mutex> lock(all_info.mutex);
			const uint32 num = all_info.filled.load();
			all_info.details[num] = details;
			return all_info.filled++;
		}
	}

	std::span<const Details> Id::AllStatsDetails()
	{
		const AllStatsInfo& all_info = AllStatsInfo::get();
		const uint32 num = all_info.filled.load(std::memory_order_relaxed);
		const Details* begin = all_info.details.data();
		return std::span<const Details>(begin, num);
	}

	void Id::Unregister(uint32) {}

	 std::atomic<Id::HandleStatsFunctionPtr> g_handle_stats = nullptr; 

	void Id::SetHandleStatsFunction(Id::HandleStatsFunctionPtr func)
	{
		g_handle_stats = func;
	}

	void Id::PassValue(double delta_ms) const
	{
		HandleStatsFunctionPtr func = g_handle_stats.load(std::memory_order_relaxed);
		if (func)
		{
			func(index, mode, delta_ms);
		}
	}

	const auto g_app_start = std::chrono::steady_clock::now();

	double TimeScope::GetMicrosecondsSinceAppStart()
	{
		using namespace std;
		const auto nano = (std::chrono::steady_clock::now() - g_app_start) / 1ns;
		return nano / 1000.0;
	}
}

#endif