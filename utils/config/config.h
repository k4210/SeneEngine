#pragma once

#include <optional>
#include <string_view>
#include <vector>
#include "common/base_types.h"

namespace Config
{
	std::optional<std::string_view> GetSingle(std::string_view file, std::string_view section, std::string_view name);

	struct ListElement
	{
		std::string_view str;
		char prefix_symbol;
		uint8 override_level;
	};
	std::vector<ListElement> GetList(std::string_view file, std::string_view section, std::string_view name);

}