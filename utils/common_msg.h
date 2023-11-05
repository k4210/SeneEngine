#pragma once

#include <variant>
#include "common/base_types.h"
#include "common/utils.h"

namespace CommonMsg
{
	enum class EMsgType : uint8
	{
		Frame = 1,
	};

	inline UINT_PTR ToWParam(EMsgType msg)
	{
		return static_cast<UINT_PTR>(msg);
	}

	inline EMsgType ToMsgType(UINT_PTR msg)
	{
		return static_cast<EMsgType>(msg);
	}

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
		Utils::TimeSpan delta;
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
