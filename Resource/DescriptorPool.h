#pragma once
#include "../Core/stdafx.h"

namespace FrameDX12
{
	class DescriptorPool;

	class Descriptor
	{
		friend class DescriptorPool;
	public:
		Descriptor();
		~Descriptor();
		Descriptor(const Descriptor&);
		Descriptor& operator=(const Descriptor&);

		Descriptor(Descriptor&&);
		Descriptor& operator=(Descriptor&&);
		
		CD3DX12_CPU_DESCRIPTOR_HANDLE operator*();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor();

		bool IsValid() const;
	private:
		std::atomic_int * mRefCount;
		DescriptorPool* mPool;
		INT mIndex;
	};
	
	// Manages a fixed size heap providing access to descriptors on it and reusing unused indexes
	// It returns a wrapper of CD3DX12_CPU_DESCRIPTOR_HANDLE which dereferences the heap on access and keeps track of references count
	class DescriptorPool
	{
	public:
		~DescriptorPool();
		void Initialize(class Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool is_shader_visible, UINT size);

		Descriptor GetNextDescriptor();
		ID3D12DescriptorHeap* GetHeap() const { return mHeap; }
	private:
		friend Descriptor;

		std::mutex mLock;
		std::queue<INT> mFreeIndexes;

		ID3D12DescriptorHeap* mHeap = nullptr;
		UINT mEntrySize;
		UINT mSize;
		std::atomic<UINT> mCurrentTop;
		
		class Device* mDevicePtr;
	};
}