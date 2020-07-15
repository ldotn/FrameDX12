#pragma once
#include "../Core/stdafx.h"
#include "CommandListPool.h"

namespace FrameDX12
{
	enum class QueueType { Graphics, Compute, Copy };

	class Device
	{
	public:
		// Constructs the device
		// WindowPtr - The pointer to the window for which to create the swapchain. If null no swapchain will be created.
		// AdapterIndex - The index of the adapter to use. If -1 all adapters will be listed and the user will be asked to choose one.
		Device(class Window* WindowPtr, int AdapterIndex = 0);

		// Gets a weak (raw) pointer to the device
		ID3D12Device* GetDevice() const { return mD3DDevice.Get(); }
		// Same as GetDevice but allows to specify the version. You should check first that the required version is supported
		template<int Version>
		auto GetDevice() const
		{
			if constexpr (Version == 0)
			{
				return (ID3D12Device*)mD3DDevice.Get();
			}
			else if constexpr (Version == 1)
			{
				return (ID3D12Device1*)mD3DDevice.Get();
			}
			else if constexpr (Version == 2)
			{
				return (ID3D12Device2*)mD3DDevice.Get();
			}
			else if constexpr (Version == 3)
			{
				return (ID3D12Device3*)mD3DDevice.Get();
			}
			else if constexpr (Version == 4)
			{
				return (ID3D12Device4*)mD3DDevice.Get();
			}
			else if constexpr (Version == 5)
			{
				return (ID3D12Device5*)mD3DDevice.Get();
			}
		}

		// Returns the highest device version supported
		int GetDeviceVersion() const { return mDeviceVersion; }

		// Returns a raw (weak) pointer to the required queue
		ID3D12CommandQueue* GetQueue(QueueType type) const
		{
			if (type == QueueType::Graphics)
				return mGraphicsCQ.Get();
			else if (type == QueueType::Compute)
				return mComputeCQ.Get();
			else if (type == QueueType::Copy)
				return mCopyCQ.Get();
		}

		// Returns a raw (weak) pointer to the required CL pool
		CommandListPool* GetCLPool(QueueType type)
		{
			if (type == QueueType::Graphics)
				return &mGraphicsCLPool;
			else if (type == QueueType::Compute)
				return &mComputeCLPool;
			else if (type == QueueType::Copy)
				return &mCopyCLPool;
		}
	private:
		Microsoft::WRL::ComPtr<ID3D12Device> mD3DDevice;
		int mDeviceVersion;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mGraphicsCQ;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mComputeCQ;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCopyCQ;
		CommandListPool mGraphicsCLPool;
		CommandListPool mComputeCLPool;
		CommandListPool mCopyCLPool;
	};
}