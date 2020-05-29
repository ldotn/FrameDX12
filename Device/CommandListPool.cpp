#include "CommandListPool.h"
#include "../Core/Log.h"
#include "Device.h"

using namespace FrameDX12;

CommandListPool::CLWrapper CommandListPool::FetchCommandList(Device* DevicePtr, uint32_t TargetSize, ID3D12PipelineState* InitialState)
{
	using namespace std;

	int target = (int)GetClosestTarget(TargetSize);

	// TODO : Check if this can be made more concurrent
	// For now, if we are working with the same target we need to lock
	// Different targets can be worked on in parallel
	scoped_lock lock(mMutex[target]);

	// Look for a closed CL
	CommandList* clptr = nullptr;
	for (auto& cl : mCLPool[target])
	{
		if (!cl->IsOpen)
		{
			clptr = cl.get();
			break;
		}
	}

	if (!clptr)
	{
		// We couldn't find an open CL so creating a new one and a new allocator for this target size
		// Not reusing allocators between CLs so you can safely use multiple CLs concurrently

		// Create the allocator
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> newAlloc;
		LogCheck(DevicePtr->GetDevice()->CreateCommandAllocator(mPoolType, IID_PPV_ARGS(newAlloc.GetAddressOf())), LogCategory::Error);

		// Create the DX command list
		auto newCl = make_unique<CommandList>();
		newCl->AllocatorPtr = newAlloc.Get();
		LogCheck(DevicePtr->GetDevice()->CreateCommandList(
			0,
			mPoolType,
			newAlloc.Get(), // Associated command allocator
			InitialState,
			IID_PPV_ARGS(newCl->List.GetAddressOf())), LogCategory::Error);

		// Push the allocator to the allocator pool and register it on the map
		mAllocatorMap[mCurrentAllocatorSet][target].push_back(mAllocatorPool[mCurrentAllocatorSet].size());
		mAllocatorPool[mCurrentAllocatorSet].emplace_back(std::move(newAlloc));

		// Store the CL struct
		clptr = newCl.get();
		mCLPool[target].emplace_back(std::move(newCl));
	}

	return CLWrapper(clptr, InitialState);
}

CommandListPool::AllocatorTargetSize CommandListPool::GetClosestTarget(int size)
{
	int bestDiff = std::numeric_limits<int>::max();
	AllocatorTargetSize bestTarget;
	for (int sizeIdx = 0; sizeIdx < (int)AllocatorTargetSize::COUNT - 1; sizeIdx++)
	{
		int diff = std::abs(cAllocatorTargetSizes[sizeIdx] - size);
		if (diff < bestDiff)
		{
			bestTarget = (AllocatorTargetSize)sizeIdx;
			bestDiff = diff;
		}
	}

	return bestTarget;
}

void CommandListPool::AdvanceFrame()
{
	using namespace std;

	// Make sure we finished cleaning up from the prev frame
	// Technically if you were using more than two sets of allocators you could continue before waiting
	//  but lets keep things *simple*
	if(mResetTask.valid()) mResetTask.get();

	// TODO : On debug verify that there aren't open CLs

	// Move to the next set
	int setToCleanup = mCurrentAllocatorSet;
	mCurrentAllocatorSet = (mCurrentAllocatorSet + 1) % cAllocatorSets;

	// Fire async reset task
	mResetTask = async(std::launch::async, [&, setToCleanup]()
	{
		// Reset all allocators
		for (auto& alloc : mAllocatorPool[setToCleanup])
		{
			LogCheck(alloc->Reset(), LogCategory::Error);
		}
	});
}
