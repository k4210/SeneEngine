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
#include "common/base_types.h"
#include "common/utils.h"

namespace Config
{
	constexpr uint8 kInvalidOverrideLevel{ std::numeric_limits<uint8>::max() };

	bool ReadFile(std::string_view file_path, uint8 override_level = 0);

	void ResetAll(); // all string_views will be invalidated

	std::optional<std::string_view> GetValue(std::string_view section, std::string_view name);

	std::vector<std::string_view> GetList(std::string_view section, std::string_view name);

	template<Utils::NumericType T>
	std::optional<T> GetNumber(std::string_view section, std::string_view name)
	{
		std::optional<std::string_view> str = GetValue(section, name);
		return str ? Utils::ParseNumber<T>(*str) : std::optional<T>{};
	}
}