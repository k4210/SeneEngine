#pragma once

#ifndef DO_LOG
	#define DO_LOG 1
#endif

#if DO_LOG

#include <format>
#include <functional>
#include "common/base_types.h"

#define IF_DO_LOG(X) X

enum class ELog : uint8
{
	Verbose,
	Log,
	Display,
	Warning,
	Error,	// Breakpoint
	Assert,	// Callstack
	Fatal	// Exit
};

struct LogCategory
{
	static const LogCategory& GetDefault();
	static bool ChangeVisibility(const char* name, const ELog new_visibility);
	static void ForEach(std::function<void(LogCategory&)>& func);

	LogCategory(const char* in_name, const ELog in_visibility = ELog::Log)
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

	void Log(ELog priority, std::string_view msg) const;

private:
	static void Register(LogCategory&);
	static void Unregister(LogCategory&);

public:
	const char* const name;
	std::atomic<ELog> default_visibility;
};

template<typename... Args>
void LOG(const LogCategory& category, ELog priority, std::format_string<Args...> fmt, Args&&... args)
{
	category.Log(priority, std::format(std::move(fmt), std::forward<Args>(args)...));
}

template<typename... Args>
void LOG(const LogCategory& category, std::format_string<Args...> fmt, Args&&... args)
{
	LOG(category, ELog::Log, std::move(fmt), std::forward<Args>(args)...);
}

template<typename... Args>
void LOG(ELog priority, std::format_string<Args...> fmt, Args&&... args)
{
	LOG(LogCategory::GetDefault(), priority, std::move(fmt), std::forward<Args>(args)...);
}

template<typename... Args>
void LOG(std::format_string<Args...> fmt, Args&&... args)
{
	LOG(ELog::Log, std::move(fmt), std::forward<Args>(args)...);
}

template<typename... Args>
void CLOG(bool bCondition, const LogCategory& category, ELog priority, std::format_string<Args...> fmt, Args&&... args)
{
	if (bCondition)
	{
		LOG(category, priority, std::move(fmt), std::forward<Args>(args)...);
	}
}

template<typename... Args>
void CLOG(bool bCondition, const LogCategory& category, std::format_string<Args...> fmt, Args&&... args)
{
	if (bCondition)
	{
		LOG(category, std::move(fmt), std::forward<Args>(args)...);
	}
}

template<typename... Args>
void CLOG(bool bCondition, ELog priority, std::format_string<Args...> fmt, Args&&... args)
{
	if (bCondition)
	{
		LOG(priority, std::move(fmt), std::forward<Args>(args)...);
	}
}

template<typename... Args>
void CLOG(bool bCondition, std::format_string<Args...> fmt, Args&&... args)
{
	if (bCondition)
	{
		LOG(std::move(fmt), std::forward<Args>(args)...);
	}
}

#else
#define IF_DO_LOG(...)
#define LOG(...)
#define CLOG(...)
#endif