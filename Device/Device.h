#pragma once
#include "../Core/stdafx.h"
#include "../Resource/DescriptorVector.h"
#include "../Resource/PipelineStateObjectPool.h"

namespace FrameDX12
{
	enum class QueueType 
	{ 
		Graphics = D3D12_COMMAND_LIST_TYPE_DIRECT, 
		Compute = D3D12_COMMAND_LIST_TYPE_COMPUTE, 
		Copy = D3D12_COMMAND_LIST_TYPE_COPY
	};

	class Device
	{
	public:
		// Constructs the device
		// window_ptr - The pointer to the window for which to create the swapchain. If null no swapchain will be created.
		// adapter_index - The index of the adapter to use. If -1 all adapters will be listed and the user will be asked to choose one.
		Device(class Window* window_ptr, int adapter_index = 0);

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

		// Gets a weak (raw) pointer to the swap chain
		IDXGISwapChain* GetSwapChain() const { return mSwapChain.Get(); }

		// Same as GetSwapChain but allows to specify the version. You should check first that the required version is supported
		template<int Version>
		auto GetSwapChain() const
		{
			if constexpr (Version == 0)
			{
				return (ID3D12Device*)mSwapChain.Get();
			}
			else if constexpr (Version == 1)
			{
				return (IDXGISwapChain1*)mSwapChain.Get();
			}
			else if constexpr (Version == 2)
			{
				return (IDXGISwapChain2*)mSwapChain.Get();
			}
			else if constexpr (Version == 3)
			{
				return (IDXGISwapChain3*)mSwapChain.Get();
			}
		}


		// Returns the highest version supported
		int GetDeviceVersion() const { return mDeviceVersion; }
		int GetSwapChainVersion() const { return mSwapChainVersion; }

		// Returns a raw (weak) pointer to the required queue
		ID3D12CommandQueue* GetQueue(QueueType type) const
		{
			switch (type)
			{
			case QueueType::Graphics:
				return mGraphicsQueue.Get();
			case QueueType::Compute:
				return mComputeQueue.Get();
			case QueueType::Copy:
				return mCopyQueue.Get();
			default:
					return nullptr;
			}
		}

		// Signals the fence of the queue and increases the value
		void SignalQueueWork(QueueType queue);

		// Waits for the queue to finish
		void WaitForQueue(QueueType queue);

		// Returns a reference to the descriptor vector
		DescriptorVector& GetDescriptorVector(D3D12_DESCRIPTOR_HEAP_TYPE type)
		{
			return mDescriptorVectors[type];
		}

		ID3D12PipelineState* GetPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) { return mPSOPool.GetPSO(desc); }
	private:
		int QueueTypeToIndex(QueueType type) const
		{
			if (type == QueueType::Graphics)
				return 0;
			else if (type == QueueType::Compute)
				return 1;
			else if (type == QueueType::Copy)
				return 2;
			else
				return -1;
		}

		ComPtr<ID3D12Device> mD3DDevice;
		int mDeviceVersion;

		ComPtr<ID3D12CommandQueue> mGraphicsQueue;
		ComPtr<ID3D12CommandQueue> mComputeQueue;
		ComPtr<ID3D12CommandQueue> mCopyQueue;

		struct
		{
			ComPtr<ID3D12Fence> fence;
			UINT64 value;
			HANDLE sync_event;
		} mFences[3];

		ComPtr<IDXGISwapChain> mSwapChain;
		int mSwapChainVersion;

		DescriptorVector mDescriptorVectors[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

		PipelineStateObjectPool mPSOPool;
	};
}