#include "stdafx.h"
#include "config.h"
#include "log/log.h"
#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>
#include <assert.h>
#include <algorithm>

namespace Config
{
	IF_DO_LOG(LogCategory config_log("config");)

	struct File
	{
		std::string path;
		std::string content;
	};

	struct SingleValue
	{
		std::string_view data;
		uint8 override_level = kInvalidOverrideLevel;
		bool add = 0;
	};

	struct List
	{
		std::vector<SingleValue> elements;
		std::vector<std::string_view> processed;
		void process()
		{
			std::vector<SingleValue> added_elements;
			std::copy_if(elements.begin(), elements.end(), std::back_inserter(added_elements),
				[](const SingleValue& value) { return value.add; });
			for (const SingleValue& to_remove : elements)
			{
				if (to_remove.add)
					continue;
				
				auto iter = std::find_if(added_elements.begin(), added_elements.end(), [&](const SingleValue& it) 
					{ return (it.data == to_remove.data) && (it.override_level < to_remove.override_level); });
				if (iter != added_elements.end())
				{
					added_elements.erase(iter);
				}
			}

			processed.clear();
			std::transform(added_elements.begin(), added_elements.end(), std::back_inserter(processed),
				[](const SingleValue& value) { return value.data; });
		}
	};

	struct SectionData
	{
		std::map<std::string_view, SingleValue> values;
		std::map<std::string_view, List> lists;
	};

	struct GlobalData
	{
		std::vector<File> files;
		std::map<std::string_view, SectionData> sections;
		std::shared_mutex mutex;
	};

	GlobalData g_config;

	void ResetAll() 
	{
		std::unique_lock lock(g_config.mutex);
		g_config.sections.clear();
		g_config.files.clear();
	}

	bool ReadFile(std::string_view file_path, uint8 override_level)
	{
		File file;
		{
			std::ifstream ifs(std::filesystem::path(file_path), std::ios::in);
			if (!ifs)
			{
				LOG(config_log, ELog::Error, "file '{}' cannot open", file_path);
				return false;
			}
			std::ostringstream sstr;
			sstr << ifs.rdbuf();

			file.content = sstr.str();
			file.path = file_path;
		}

		std::unique_lock lock(g_config.mutex);
		g_config.files.push_back(std::move(file));
		const std::string& file_string = ((g_config.files.end() - 1)->content);

		SectionData* section = nullptr;
		std::set<std::string_view> lists_to_process;
		auto process_lists_from_section = [&lists_to_process, &section]()
		{
			assert(section || lists_to_process.empty());
			if (section)
			{
				for (std::string_view list_key : lists_to_process)
				{
					List& list = section->lists[list_key];
					list.process();
				}
			}
			lists_to_process.clear();
		};

		auto process = [&](std::string_view in_line)
		{
			std::string_view line = Utils::TrimWhiteSpace(in_line);
			if (!line.size()) 
				return true; // empty line

			if (line.starts_with(';')) 
				return true; // comment

			if (line.starts_with('[') && line.ends_with(']')) // section
			{
				process_lists_from_section();

				line = line.substr(1, line.size() - 2);
				section = &g_config.sections[line];
				return true;
			}

			const char first_symbol = line[0];
			const bool is_list = (first_symbol == '+') || (first_symbol == '-');
			if (is_list)
			{
				line = Utils::TrimWhiteSpace(line.substr(1));
				if (!line.size())
				{
					LOG(config_log, ELog::Error, "file '{}' line '{}' empty list element", file_path, in_line);
					return false;
				}
			}

			if (!std::isalnum(line[0]))
			{
				LOG(config_log, ELog::Error, "file '{}' line '{}' unrecognized initial symbol", file_path, in_line);
				return false;
			}
			if (!section)
			{
				LOG(config_log, ELog::Error, "file '{}' missing section", file_path);
				return false;
			}
			const size_t assign = line.find_first_of('=');
			if (assign == std::string_view::npos)
			{
				LOG(config_log, ELog::Error, "file '{}' line '{}' missing '='", file_path, in_line);
				return false;
			}

			const std::string_view key_str = Utils::TrimWhiteSpace(line.substr(0, assign));
			const std::string_view value_str = Utils::TrimWhiteSpace(line.substr(assign+1));
			if (is_list)
			{
				SingleValue value;
				value.override_level = override_level;
				value.data = value_str;
				value.add = first_symbol == '+';

				List& list = section->lists[key_str];
				list.elements.push_back(value);

				lists_to_process.insert(key_str);
			}
			else
			{
				SingleValue& value = section->values[key_str];
				if ((value.override_level == kInvalidOverrideLevel) || (value.override_level < override_level))
				{
					value.override_level = override_level;
					value.data = value_str;
				}
			}
			return true;
		};
		const bool result = Utils::ForEachLine(file_string, process);
		process_lists_from_section();
		return result;
	}

	std::optional<std::string_view> GetValue(std::string_view section_name, std::string_view name)
	{
		std::shared_lock lock(g_config.mutex);
		const SectionData* section = Utils::FindPtrInMap(g_config.sections, section_name);
		const SingleValue* value = section ? Utils::FindPtrInMap(section->values, name) : nullptr;
		return value ? value->data : std::optional<std::string_view>{};
	}

	std::vector<std::string_view> GetList(std::string_view section_name, std::string_view name)
	{
		std::shared_lock lock(g_config.mutex);
		const SectionData* section = Utils::FindPtrInMap(g_config.sections, section_name);
		const List* list = section ? Utils::FindPtrInMap(section->lists, name) : nullptr;
		return list ? list->processed : std::vector<std::string_view>{};
	}
}