#include "DescriptorPool.h"
#include "../Device/Device.h"
#include "../Core/Log.h"

using namespace FrameDX12;

void DescriptorPool::Initialize(class Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool is_shader_visible, UINT size)
{
	mDevicePtr = device;

	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.NodeMask = 0;
	desc.NumDescriptors = size;
	desc.Type = type;
	desc.Flags = is_shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(mDevicePtr->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap)));

	mEntrySize = device->GetDevice()->GetDescriptorHandleIncrementSize(type);
	mCurrentTop = 0;
	mSize = size;
	mCPUHeapStart = mHeap->GetCPUDescriptorHandleForHeapStart();
	mGPUHeapStart = mHeap->GetGPUDescriptorHandleForHeapStart();
}

DescriptorPool::~DescriptorPool()
{
	mHeap->Release();
}

Descriptor::Descriptor(Descriptor&& other) :
	  mRefCount(std::exchange(other.mRefCount, nullptr))
	, mPool(std::exchange(other.mPool, nullptr))
	, mIndex(std::exchange(other.mIndex, -1))
{
}

Descriptor& Descriptor::operator=(Descriptor&& rhs)
{
	mRefCount = std::exchange(rhs.mRefCount, nullptr);
	mPool = std::exchange(rhs.mPool, nullptr);
	mIndex = std::exchange(rhs.mIndex, -1);

	return *this;
}

Descriptor::Descriptor() : 
	  mRefCount(new std::atomic_int(1))
	, mPool(nullptr)
	, mIndex(-1)
{
}

Descriptor::Descriptor(const Descriptor& other)
{
	*this = other;
}

Descriptor& Descriptor::operator=(const Descriptor& rhs)
{
	mRefCount = rhs.mRefCount;
	mPool = rhs.mPool;
	mIndex = rhs.mIndex;

	(*mRefCount)++;

	return *this;
}

Descriptor::~Descriptor()
{
	if (mPool != nullptr && mRefCount != nullptr && mRefCount->fetch_sub(1) == 1)
	{
		std::scoped_lock lock(mPool->mLock);

		mPool->mFreeIndexes.push(mIndex);
		delete mRefCount;
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Descriptor::operator*()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mPool->mCPUHeapStart, mIndex, mPool->mEntrySize);
}
CD3DX12_GPU_DESCRIPTOR_HANDLE Descriptor::GetGPUDescriptor()
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mPool->mGPUHeapStart, mIndex, mPool->mEntrySize);
}

bool Descriptor::IsValid() const
{
	return (mPool != nullptr) && (mPool->GetHeap() != nullptr);
}

Descriptor DescriptorPool::GetNextDescriptor()
{
	Descriptor handle;

	if (!LogAssertAndContinue(mHeap != nullptr, LogCategory::Error))
		return handle;

	if (mFreeIndexes.empty())
	{
		if (!LogAssertAndContinue(mCurrentTop < mSize, LogCategory::Error))
			return handle;

		handle.mPool = this;
		handle.mIndex = mCurrentTop.fetch_add(1);
	}
	else
	{
		std::scoped_lock lock(mLock);

		handle.mPool = this;
		handle.mIndex = mFreeIndexes.front();
		mFreeIndexes.pop();
	}

	return handle;
}

/*Descriptor Descriptor::Duplicate() const
{
	Descriptor copy = mPool->GetNextDescriptor();
	mPool->mDevicePtr->GetDevice()->CopyDescriptorsSimple(1, *copy, CD3DX12_CPU_DESCRIPTOR_HANDLE(mPool->mCPUHeapStart, mIndex, mPool->mEntrySize), mPool->
}*/