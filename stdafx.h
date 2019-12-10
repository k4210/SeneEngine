//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include "third_party/bitset2/bitset2.hpp"
#include "utils/d3dx12.h"
#include "utils/mpsc_queue.h"

#include <windows.h>
#include <wrl.h>
#include <shellapi.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>

#include <string>
#include <deque>
#include <vector>
#include <thread>
#include <variant>
#include <atomic>
#include <memory>
#include <utility>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <algorithm>

#include <assert.h>