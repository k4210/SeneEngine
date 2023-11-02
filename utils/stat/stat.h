#pragma once

#ifndef DO_STAT
	#define DO_STAT 1
#endif

#if DO_STAT
#include <vector>
#include <functional>
#include <span>
#include "common/base_types.h"

#define IF_DO_STAT(X) X
namespace Stat
{
	enum class EMode : uint8
	{
		PerFrame,
		Accumulate,
		Override
	};

	struct Details
	{
		const char* group = nullptr;
		const char* name = nullptr;
		EMode mode = EMode::PerFrame;
	};

	struct Id
	{
		static std::span<const Details> AllStatsDetails();
		typedef void (*HandleStatsFunctionPtr)(uint32, EMode, double);
		static void SetHandleStatsFunction(HandleStatsFunctionPtr func);

		Id(const char* in_gruop, const char* in_name, EMode in_mode)
			: index(Register(in_gruop, in_name, in_mode))
			, mode(in_mode)
		{}

		~Id()
		{
			Unregister(index);
		}

		void PassValue(double delta) const;

		const uint32 index;
		const EMode mode;
	private:
		static uint32 Register(const char* in_gruop, const char* in_name, EMode in_mode);
		static void Unregister(uint32);
	};

	struct TimeScope
	{
		static double GetMicrosecondsSinceAppStart();

		TimeScope(const Id& in_id)
			: id(in_id)
			, start(GetMicrosecondsSinceAppStart())
		{}

		~TimeScope()
		{
			const double delta_ms = GetMicrosecondsSinceAppStart() - start;
			id.PassValue(delta_ms);
		}

	private:
		const Id& id;
		const double start;
	};
}
// Do not use directly
#define STAT_CONCAT_INNER(a, b) a ## b
#define STAT_CONCAT(a, b) STAT_CONCAT_INNER(a, b)
#define STAT_TIME_SCOPE_INNER(GROUP, NAME, INSTANCE) static Stat::Id INSTANCE(#GROUP, #NAME, Stat::EMode::PerFrame); \
	Stat::TimeScope STAT_CONCAT(INSTANCE, _scope)(INSTANCE);

// Use this
#define STAT_TIME_SCOPE(GROUP, NAME) STAT_TIME_SCOPE_INNER(GROUP, NAME, STAT_CONCAT(stat_id_, __COUNTER__))

#else
#define IF_DO_STAT(...)
#define STAT_TIME_SCOPE(...)
#endif