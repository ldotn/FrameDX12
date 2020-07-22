#include "DescriptorVector.h"
#include "../Device/Device.h"
#include "../Core/Log.h"

using namespace FrameDX12;

void DescriptorVector::Initialize(class Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool is_shader_visible, UINT initial_capacity)
{
	mIsInitialized = true;

	mDevicePtr = device;

	mDesc.NodeMask = 0;
	mDesc.NumDescriptors = initial_capacity;
	mDesc.Type = type;
	mDesc.Flags = is_shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(mDevicePtr->GetDevice()->CreateDescriptorHeap(&mDesc, IID_PPV_ARGS(&mHeap)));

	mSize = 0;
	mCapacity = initial_capacity;
	mEntrySize = device->GetDevice()->GetDescriptorHandleIncrementSize(type);
}

DescriptorVector::~DescriptorVector()
{
	if (mHeap)
	{
		mHeap->Release();
		mHeap = nullptr;
	}
}

Descriptor DescriptorVector::GetNextDescriptor()
{
	if (!LogAssertAndContinue(mIsInitialized, LogCategory::Error))
		return Descriptor{};

	++mSize;

	if (mSize == mCapacity)
	{
		mCapacity += mCapacity / 2;
		mHeap->Release();
		mDesc.NumDescriptors = mCapacity;
		ThrowIfFailed(mDevicePtr->GetDevice()->CreateDescriptorHeap(&mDesc, IID_PPV_ARGS(&mHeap)));
	}

	Descriptor handle;
	handle.mHeapPtr = &mHeap;
	handle.mEntrySize = mEntrySize;
	handle.mIndex = mSize - 1;

	return handle;
}
