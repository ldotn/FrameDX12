#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // Exclude rarely used stuff from Windows headers
#define NOMINMAX
#include <Windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include "d3dx12.h"
#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>
#include <atomic>
#include <ppl.h>
#include <concurrent_vector.h>
#include <future>
#include <unordered_map>
#include <algorithm>
#include <execution>
#include "enumerate.h"
#include "zip.h"
#include <mutex>
#include "pix3.h"

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;