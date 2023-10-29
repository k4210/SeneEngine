#include "log.h"

#ifdef DO_LOG
#include <iostream>
#include <map>
#include <assert.h>

namespace log_details
{
	std::map<const char*, LogCategory*> g_categories;
	std::mutex g_categories_mutex;

	void InnerLog(const char* category_name, ELogPriority priority, std::string_view msg)
	{
		const char* prio_str = [priority]()
			{
				switch (priority)
				{
				case ELogPriority::Warning: return " Warning";
				case ELogPriority::Error: return " Error";
				case ELogPriority::Fatal: return " Fatal";
				}
				return "";
			}();
		std::cout << category_name << prio_str << ": " << msg << std::endl;
		assert(priority != ELogPriority::Error);
		assert(priority != ELogPriority::Fatal);
	}
}

bool LogCategory::ChangeVisibility(const char* name, const ELogPriority new_visibility)
{
	const std::lock_guard<std::mutex> lock(log_details::g_categories_mutex);
	auto iter = log_details::g_categories.find(name);
	if (iter == log_details::g_categories.end())
	{
		return false;
	}

	LogCategory* const category = iter->second;
	assert(category);
	category->default_visibility = new_visibility;
	return true;
}

void LogCategory::Register(LogCategory& category)
{
	const std::lock_guard<std::mutex> lock(log_details::g_categories_mutex);
	const auto iter_result_pair = log_details::g_categories.try_emplace(category.name, &category);
	assert(iter_result_pair.second);
}

void LogCategory::Unregister(LogCategory& category)
{
	const std::lock_guard<std::mutex> lock(log_details::g_categories_mutex);
	const auto num_removed = log_details::g_categories.erase(category.name);
	assert(num_removed == 1);
}

const LogCategory& LogCategory::GetDefault()
{
	static LogCategory Default("Default", ELogPriority::Log);
	return Default;
}
#endif