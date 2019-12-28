#pragma once

#include <DirectXMath.h>

using namespace DirectX;

struct Transform
{
	XMFLOAT3 translate = XMFLOAT3(0, 0, 0);
	XMFLOAT4 rotation = XMFLOAT4(0, 0, 0, 1); // quaternion
	float scale = 1.0f;
};