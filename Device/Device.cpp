#include "Device.h"
#include "../Core/Log.h"
#include <iostream>

using namespace FrameDX12;

Device::Device(Window* window_ptr, int adapter_index) :
	mGraphicsCLPool(D3D12_COMMAND_LIST_TYPE_DIRECT),
	mComputeCLPool(D3D12_COMMAND_LIST_TYPE_COMPUTE),
	mCopyCLPool(D3D12_COMMAND_LIST_TYPE_COPY)
{
	using namespace std;
	using namespace Microsoft::WRL;

#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debug_controller;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)));
		debug_controller->EnableDebugLayer();
	}
#endif

	// Find the graphics adapter
	IDXGIAdapter* adapter = nullptr;
	IDXGIFactory* factory = nullptr;
	bool using_factory2 = true;

	UINT dxgi2_flags = 0;
#ifdef _DEBUG
	dxgi2_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	if (CreateDXGIFactory2(dxgi2_flags, IID_PPV_ARGS(&factory)) != S_OK)
	{
		using_factory2 = false;
		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
	}

	if (adapter_index == -1)
	{
		for (UINT i = 0; factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
		{
			DXGI_ADAPTER_DESC desc;
			adapter->GetDesc(&desc);

			wcout << L"GPU : " << i << L" , " << desc.Description << endl;
		}

		wcout << L"Select GPU : ";
		int adapterIDX = 0;
		cin >> adapter_index;
	}

	factory->EnumAdapters(adapter_index, &adapter);

	// Create the device
	// Try first with feature level 12.1
	D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_12_1;
	ThrowIfFailed(D3D12CreateDevice(adapter, level, IID_PPV_ARGS(&mD3DDevice)));
	if (!mD3DDevice)
	{
		// If the device failed to be created, try with feature level 12.0
		LogMsg(L"Failed to create device with feature level 12.1, trying with feature level 12.0", LogCategory::Info);

		level = D3D_FEATURE_LEVEL_12_0;
		ThrowIfFailed(D3D12CreateDevice(adapter, level, IID_PPV_ARGS(&mD3DDevice)));
	}

	// Check if the device can be casted to anything higher than 11.0
	ID3D12Device* new_device;
	mDeviceVersion = 0;
	if (mD3DDevice->QueryInterface(__uuidof(ID3D12Device5), (void**)&new_device) == S_OK)
	{
		mD3DDevice = new_device;
		mDeviceVersion = 5;
	}
	else if (mD3DDevice->QueryInterface(__uuidof(ID3D12Device4), (void**)&new_device) == S_OK)
	{
		mD3DDevice = new_device;
		mDeviceVersion = 4;
	}
	else if (mD3DDevice->QueryInterface(__uuidof(ID3D12Device3), (void**)&new_device) == S_OK)
	{
		mD3DDevice = new_device;
		mDeviceVersion = 3;
	}
	else if (mD3DDevice->QueryInterface(__uuidof(ID3D12Device2), (void**)&new_device) == S_OK)
	{
		mD3DDevice = new_device;
		mDeviceVersion = 2;
	}
	else if (mD3DDevice->QueryInterface(__uuidof(ID3D12Device1), (void**)&new_device) == S_OK)
	{
		mD3DDevice = new_device;
		mDeviceVersion = 1;
	}

	LogMsg(wstring(L"Created a device of version ") + to_wstring(mDeviceVersion), LogCategory::Info);

	// Create the command queues
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(mD3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mGraphicsCQ)));

	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	ThrowIfFailed(mD3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mComputeCQ)));

	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	ThrowIfFailed(mD3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCopyCQ)));
}
