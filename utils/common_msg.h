#pragma once

#include <variant>
#include "common/base_types.h"

namespace CommonMsg
{
	enum class State
	{
		Playing,
		Loading,
		InGameMenu,
		MainMenu,
		Background
	};
	struct Frame
	{
		uint64 frame_id;
		double delta;
	};
	struct StateChange
	{
		State actual_state;
	};
	enum class EReload
	{
		Config,
		Code,
		Asset
	};
	struct Reload
	{
		EReload mode;
		bool before_or_after;
	};
	using Message = std::variant<Frame, StateChange, Reload>;
}
