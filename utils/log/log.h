#pragma once

#ifndef DO_LOG
	#define DO_LOG 0
#endif

#ifdef DO_LOG

#include <format>
#include "utils/common/base_types.h"

#define IF_DO_LOG(X) X

enum class ELogPriority : uint8
{
	Verbose,
	Log,
	Display,
	Warning,
	Error,
	Fatal
};

class LogCategory
{
public:
	const char* const name;
	std::atomic<ELogPriority> default_visibility;

private:
	static void Register(LogCategory&);
	static void Unregister(LogCategory&);

public:
	static const LogCategory& GetDefault();
	static bool ChangeVisibility(const char* name, const ELogPriority new_visibility);

	LogCategory(const char* in_name, const ELogPriority in_visibility)
		: name(in_name), default_visibility(in_visibility)
	{
		Register(*this);
	}
	~LogCategory()
	{
		Unregister(*this);
	}

	LogCategory(const LogCategory&) = delete;
	LogCategory(LogCategory&&) = delete;
	LogCategory& operator=(const LogCategory&) = delete;
	LogCategory& operator=(LogCategory&&) = delete;
};

namespace log_details
{
	void InnerLog(const char* category_name, ELogPriority priority, std::string_view msg);
}

template<typename... Args>
void Log(const LogCategory& category, ELogPriority priority, std::format_string<Args...> fmt, Args&&... args)
{
	if (category.default_visibility.load(std::memory_order_relaxed) <= priority)
	{
		log_details::InnerLog(category.name, priority, std::format(std::move(fmt), std::forward(args...)));
	}
}

template<typename... Args>
void Log(ELogPriority priority, std::format_string<Args...> fmt, Args&&... args)
{
	Log(log_details::LogCategory::GetDefault(), priority, std::move(fmt), std::forward(args...));
}

template<typename... Args>
void Log(std::format_string<Args...> fmt, Args&&... args)
{
	Log(ELogPriority::Log, std::move(fmt), std::forward(args...));
}

template<typename... Args>
void CondLog(bool bCondition, const LogCategory& category, ELogPriority priority, std::format_string<Args...> fmt, Args&&... args)
{
	if (bCondition)
	{
		Log(category, priority, std::move(fmt), std::forward(args...));
	}
}

template<typename... Args>
void CondLog(bool bCondition, ELogPriority priority, std::format_string<Args...> fmt, Args&&... args)
{
	if (bCondition)
	{
		Log(priority, std::move(fmt), std::forward(args...));
	}
}

template<typename... Args>
void CondLog(bool bCondition, std::format_string<Args...> fmt, Args&&... args)
{
	if (bCondition)
	{
		Log(std::move(fmt), std::forward(args...));
	}
}

#else
#define IF_DO_LOG(...)
#define Log(...)
#define CondLog(...)
#endif