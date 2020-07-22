#pragma once
#include "../Core/stdafx.h"

namespace FrameDX12
{
	// Keeps track of the different descriptor heaps and resizes them when there are inserts
	// Restrictions
	//	- Can't store a pointer to the heap as it may be reallocated
	//	- If you somehow ask for a new descriptor during command list recording you need to reset the heaps
	//	- GetNextDescriptor IS NOT thread safe
	// It returns a wrapper of CD3DX12_CPU_DESCRIPTOR_HANDLE which dereferences the correct heap on accesing
	//	 for that it keeps a pointer to the the heap ComPtr (not a ComPtr, it's a pointer to member)
	class Descriptor
	{
	public:
		CD3DX12_CPU_DESCRIPTOR_HANDLE operator*()
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE((*mHeapPtr)->GetCPUDescriptorHandleForHeapStart(), mIndex, mEntrySize);
		}
	private:
		friend class DescriptorVector;

		ID3D12DescriptorHeap** mHeapPtr;
		INT mIndex;
		UINT mEntrySize;
	};

	class DescriptorVector
	{
	public:
		~DescriptorVector();
		void Initialize(class Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool is_shader_visible, UINT initial_capacity = 16);

		Descriptor GetNextDescriptor();
	private:
		bool mIsInitialized = false;

		// Not using a ComPtr because it has an overload for operator& so I can't grab a pointer-to-member
		ID3D12DescriptorHeap* mHeap = nullptr;
		UINT mEntrySize;
		INT mSize;
		UINT mCapacity;
		
		D3D12_DESCRIPTOR_HEAP_DESC mDesc;
		class Device* mDevicePtr;
	};
}