#include "stdafx.h"
#include "log.h"

#if DO_LOG
#include <iostream>
#include <map>
#include <assert.h>
#include <cstdlib>

struct CategoriesMapScope
{
	std::map<const char*, LogCategory*>& get()
	{
		static std::map<const char*, LogCategory*> categories;
		return categories;
	}

	CategoriesMapScope() : lock(Mutex()) {}
	~CategoriesMapScope() = default;

private:
	static std::mutex& Mutex()
	{
		static std::mutex mutex;
		return mutex;
	}
public:
	const std::lock_guard<std::mutex> lock;
};

void LogCategory::Log(const ELog priority, std::string_view msg) const
{
	if (default_visibility.load(std::memory_order_relaxed) > priority)
	{
		return;
	}

	const char* const prio_str = [priority]()
	{
		switch (priority)
		{
			case ELog::Warning: return " Warning";
			case ELog::Error:	return " Error";
			case ELog::Assert:	return " Assert";
			case ELog::Fatal:	return " Fatal";
		}
		return "";
	}();
	{
		static std::mutex cout_mutex;
		const std::lock_guard<std::mutex> lock(cout_mutex);
		std::cout << name << prio_str << ": " << msg << std::endl;

		if (priority >= ELog::Assert)
		{
			//TODO callstack
		}
	}

	assert(priority <= ELog::Error);

	if (priority >= ELog::Fatal)
	{
		std::exit(EXIT_FAILURE);
	}
}

bool LogCategory::ChangeVisibility(const char* name, const ELog new_visibility)
{
	CategoriesMapScope map_scope;
	auto iter = map_scope.get().find(name);
	if (iter == map_scope.get().end())
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
	CategoriesMapScope map_scope;
	const auto iter_result_pair = map_scope.get().try_emplace(category.name, &category);
	assert(iter_result_pair.second);
}

void LogCategory::Unregister(LogCategory& category)
{
	CategoriesMapScope map_scope;
	const auto num_removed = map_scope.get().erase(category.name);
	assert(num_removed == 1);
}

const LogCategory& LogCategory::GetDefault()
{
	static LogCategory Default("Default", ELog::Log);
	return Default;
}

void LogCategory::ForEach(std::function<void(LogCategory&)>& func)
{
	CategoriesMapScope map_scope;
	for (auto& pair : map_scope.get())
	{
		func(*pair.second);
	}
}
#endif