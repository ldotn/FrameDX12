#pragma once
#include "../Core/stdafx.h"
#include "../Core/Log.h"

namespace FrameDX12
{
	class CommandListPool
	{
	public:
		struct CommandList
		{
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> List;
			ID3D12CommandAllocator* AllocatorPtr = nullptr;
			bool IsOpen = true;
		};

		// There can't be two CLWrapper for the same CL pointer on different threads
		class CLWrapper
		{
		public:
			CLWrapper(CommandList* CL, ID3D12PipelineState* InitialState) :
				mCommandListPtr(CL)
			{
				if (!CL->IsOpen)
				{
					CL->IsOpen = true;
					ThrowIfFailed(mCommandListPtr->List->Reset(mCommandListPtr->AllocatorPtr, InitialState));
				}
			}

			~CLWrapper()
			{
				LogCheck(mCommandListPtr->List->Close(), LogCategory::Error);
				mCommandListPtr->IsOpen = false;
			}

			ID3D12GraphicsCommandList* operator->() 
			{ 
				return mCommandListPtr->List.Get(); 
			}
		private:
			CommandList* mCommandListPtr = nullptr;
		};

		CommandListPool(D3D12_COMMAND_LIST_TYPE type) :
			mPoolType(type)
		{
		}

		// Fetches a command list from the pool in a thread safe fashion
		// TargetSize - The approximate expected number of elements. Used to select the allocator
		// InitialState - The initial pipeline state
		CLWrapper FetchCommandList(class Device* DevicePtr, uint32_t TargetSize, ID3D12PipelineState* InitialState);

		// Alternates to the set of allocators for the next frame and resets the current set of allocators
		// On debug it checks that there are no live CLWrappers
		// Reset and bookkeeping for the prev frame are async
		// IMPORTANT! NO CL CAN BE RECORDING BEFORE THIS FUNCTION RETURNS! YOU CAN'T CALL FetchCommandList BEFORE THIS RETURNS!
		void AdvanceFrame();
		
		enum class AllocatorTargetSize { Small, Medium, Large, Huge, COUNT};
		static constexpr int cAllocatorSizes = (int)AllocatorTargetSize::COUNT;
	private:
		// Finds the closest target size for a provided expected size
		AllocatorTargetSize GetClosestTarget(int size);

		std::mutex mMutex[cAllocatorSizes];
		const D3D12_COMMAND_LIST_TYPE mPoolType;

		std::vector<std::unique_ptr<CommandList>> mCLPool[cAllocatorSizes];
		std::future<void> mResetTask;

		static constexpr int cAllocatorTargetSizes[] = { 4, 32, 128, 2048 };
		static constexpr int cAllocatorSets = 2;
		std::vector<int> mAllocatorMap[cAllocatorSets][cAllocatorSizes];
		std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> mAllocatorPool[cAllocatorSets];
		int mCurrentAllocatorSet = 0;
	};
}