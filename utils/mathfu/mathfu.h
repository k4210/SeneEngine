#pragma once

#include "mathfu/hlsl_mappings.h"
#include "mathfu/constants.h"
#include "mathfu/utilities.h"
#include "mathfu/rect.h"

namespace mathfu
{
	struct Transform
	{
		float3 translate = float3(0.0f);
		float scale = 1.0f;
		Quaternion<float> rotation = Quaternion<float>::identity;
	};

    template <typename T> __forceinline T AlignUpWithMask(T value, size_t mask)
    {
        return (T)(((size_t)value + mask) & ~mask);
    }

    template <typename T> __forceinline T AlignDownWithMask(T value, size_t mask)
    {
        return (T)((size_t)value & ~mask);
    }

    template <typename T> __forceinline T AlignUp(T value, size_t alignment)
    {
        return AlignUpWithMask(value, alignment - 1);
    }

    template <typename T> __forceinline T AlignDown(T value, size_t alignment)
    {
        return AlignDownWithMask(value, alignment - 1);
    }

    template <typename T> __forceinline bool IsAligned(T value, size_t alignment)
    {
        return 0 == ((size_t)value & (alignment - 1));
    }
}