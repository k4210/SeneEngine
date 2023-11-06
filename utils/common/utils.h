#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <charconv>
#include <map>
#include <concepts>
#include <functional>
#include <limits>
#include <chrono>
#include "common/base_types.h"
#include "mathfu/mathfu.h"

namespace Utils
{
	template <typename T>
	concept NumericType = std::integral<T> || std::floating_point<T>;

	template<typename K, typename V>
	const V* FindPtrInMap(const std::map<K, V>& map, const K& key)
	{
		auto iter = map.find(key);
		return (iter == map.end()) ? nullptr : &(iter->second);
	}

	std::string_view TrimWhiteSpace(std::string_view str);

	template<NumericType T>
	std::optional<T> ParseNumber(std::string_view trimmed_str)
	{
		T result{};
		const char* end = str.data() + str.size();
		auto [ptr, ec] = std::from_chars(str.data(), end, result);
		const bool bOk = (ec == std::errc()) && (ptr == end);
		return bOk ? result : std::optional<T>{};
	}

	// if func returns false, then break execution and return false
	bool ForEachLine(const std::string& string, std::function<bool(std::string_view)> func);

	using Microsecond = std::chrono::microseconds;
	using TimeSpan = std::chrono::nanoseconds;
	inline auto GetTime() { return std::chrono::high_resolution_clock::now(); }
	using TimeType = decltype(GetTime());
	inline double ToMiliseconds(TimeSpan duration)
	{
		return duration.count() / (1000.0 * 1000.0);
	}
}

struct EntityUtils
{
	template<typename E> static bool AllComponentsValid(const E& entity)
	{
		bool valid = true;
		auto func = [&valid](const auto& comp) { valid = valid && comp.IsValid(); };
		const_cast<E&>(entity).for_each_component(func);
		return valid;
	}

	template<typename E> static void Cleanup(E& entity)
	{
		auto func = [](auto& comp) { comp.Cleanup(); };
		entity.for_each_component(func);
	}

	template<typename E> static void UpdateTransform(E& entity, mathfu::Transform transform)
	{
		auto func = [transform](auto& comp) { comp.UpdateTransform(transform); };
		entity.for_each_component(func);
	}
};