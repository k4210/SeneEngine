#include "stdafx.h"
#include "common/utils.h"

namespace Utils
{
	std::string_view TrimWhiteSpace(std::string_view str)
	{
		const char* white_space = " \t\v\r\n";

		const size_t offset = str.find_first_not_of(white_space);
		str = str.substr(offset);

		const size_t last = str.find_last_not_of(white_space);
		if (std::string_view::npos != last)
		{
			str = str.substr(0, last + 1);
		}
		return str;
	}

	bool ForEachLine(const std::string& string, std::function<bool(std::string_view)> func)
	{
		for (size_t start = 0; start < string.size(); )
		{
			const size_t found = string.find_first_of('\n', start);
			const size_t end = (found == std::string::npos) ? string.size() : (found + 1);
			std::string_view line(string.data() + start, string.data() + end);
			if (!func(line))
			{
				return false;
			}
			start = end;
		}
		return true;
	}
} 