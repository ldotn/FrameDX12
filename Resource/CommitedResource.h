#pragma once
#include "DescriptorPool.h"
#include "../Device/Device.h"
#include "../Core/Log.h"

namespace FrameDX12
{
	class UploadAllocator
	{
	public:
		UploadAllocator(Device* device, size_t size) :
			mDevice(device),
			mSize(size),
			mFreeSize(size)
		{
			auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto buff_desc = CD3DX12_RESOURCE_DESC::Buffer(size);

			ThrowIfFailed(mDevice->GetDevice()->CreateCommittedResource(
				&heap_prop,
				D3D12_HEAP_FLAG_NONE,
				&buff_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&mUploadResource)));

			mFreeRegions.push_back({ 0, size - 1 });
		}

		~UploadAllocator()
		{
			// All CLs MUST have finished with their resources
			// We will very likely crash after this
			LogAssert(mSize == mFreeSize, LogCategory::CriticalError);

			// TODO : Ensure the device is valid somehow
			mUploadResource->Release();
		}
		
		struct AllocatedRegion
		{
			//uint8* MappedPtr;
			ID3D12Resource* BackingResource;
			size_t Offset;
		};

		// Thread safe (with a simple mutex, need to improve that later)
		AllocatedRegion Allocate(ID3D12GraphicsCommandList* cl, size_t size, bool& success_out);
		void ReleaseRegions(ID3D12GraphicsCommandList* cl);
		
		static void ReleaseRegionsOnAllAllocators(ID3D12GraphicsCommandList* cl)
		{
			std::scoped_lock(sGlobalLock);

			for (auto allocator : sAllocators) allocator->ReleaseRegions(cl);
		}
		static std::shared_ptr<UploadAllocator> CreateNewAllocator(Device* device, size_t size, bool use_mutex = true)
		{
			// Needed most of the time, but not when called from GetOrCreateAllocator
			if(use_mutex) std::scoped_lock(sGlobalLock);

			std::shared_ptr<UploadAllocator> allocator(new UploadAllocator(device, size));
			sAllocators.push_back(allocator);

			return allocator;
		}
		static std::shared_ptr<UploadAllocator> GetOrCreateAllocator(Device* device, size_t required_size)
		{
			std::scoped_lock(sGlobalLock);

			size_t max_size = required_size;
			for (auto allocator : sAllocators)
			{
				if (allocator->mFreeSize >= required_size)
				{
					return allocator;
				}

				max_size = std::max(max_size, allocator->mSize);
			}

			// Create a new one that's bigger than all the rest to try to avoid having to create a new one again
			max_size *= 3; // TODO : Tune/expose this

			return CreateNewAllocator(device, max_size, false);
		}
	private:
		struct Region
		{
			uint64 Start;
			uint64 End;
		};

		std::unordered_map<void*, std::vector<Region>> mUsedRegions;
		std::list<Region> mFreeRegions;

		ComPtr<ID3D12Resource> mUploadResource;
		//uint8* mMappedData;
		size_t mSize;
		size_t mFreeSize;

		Device* mDevice;

		// TODO : Reduce locking
		std::mutex mLock;

		static std::mutex sGlobalLock;
		static std::vector<std::shared_ptr<UploadAllocator>> sAllocators;
	};

	class CommitedResource
	{
	public:
		// TODO : Store the clear value and provide a Clear function that takes a CL and calls Clear with that value

		// By default the resource is created on the COPY_DEST state, to be filled
		void Create(Device* device,
					CD3DX12_RESOURCE_DESC description,
					D3D12_RESOURCE_STATES initial_states = D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_CLEAR_VALUE* clear_value = nullptr, // It's ok to pass a pointer to an rvalue here
					D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT,
					D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE); 

		void CreateDSV();
		void CreateCBV();

		// Note : 1) It's ok to pass an rvalue pointer as desc, 2) some resources REQUIRE a desc to be provided
		void CreateSRV(D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr);
		// Note : 1) It's ok to pass an rvalue pointer as desc, 2) some resources REQUIRE a desc to be provided
		void CreateUAV(D3D12_UNORDERED_ACCESS_VIEW_DESC * desc, ComPtr<ID3D12Resource> counter = nullptr);

		void Transition(ID3D12GraphicsCommandList* cl, D3D12_RESOURCE_STATES new_states);

		Descriptor GetDSV() const { return mDSV; }
		Descriptor GetSRV() const { return mSRV; }
		Descriptor GetCBV() const { return mCBV; }
		Descriptor GetUAV() const { return mUAV; }

		const CD3DX12_RESOURCE_DESC& GetDesc() const { return mDescription; }
		ID3D12Resource* operator->() { return mResource.Get(); }

		CommitedResource Duplicate() const
		{
			CommitedResource copy;
			copy.mDevice = mDevice;
			copy.mStates = mStates;
			copy.mDescription = mDescription;

			// TODO!
			return copy;
		}

		// Fills the resource from a provided CPU buffer
		// Note that this only adds the command to the list, you need to sync before using the resource
		template<typename T>
		void FillFromBuffer(ID3D12GraphicsCommandList* cl, T* buffer, size_t buffer_size, D3D12_RESOURCE_STATES new_states)
		{
			Transition(cl, D3D12_RESOURCE_STATE_COPY_DEST);

			D3D12_SUBRESOURCE_DATA data_desc = {};
			data_desc.pData = buffer;
			data_desc.RowPitch = buffer_size * sizeof(T);
			data_desc.SlicePitch = data_desc.RowPitch;

			// Keep an array of fixed size resource buffers mapped and track the used regions from those buffers
			// When this function is called try to find an unused region of the right size 
			// If there are update with that region and register the region as used
			// If there are not, create a new buffer and go to the prev step
			// When Close (Im thinking it should be Execute instead) is called on a command list, also call ReleaseUploadRegions so this function knows those regions are free again
			
			auto allocator_ptr = UploadAllocator::GetOrCreateAllocator(mDevice, data_desc.RowPitch);
			bool succeded_allocation;
			auto upload_region = allocator_ptr->Allocate(cl, data_desc.RowPitch, succeded_allocation);

			UpdateSubresources<1>(cl, mResource.Get(), upload_region.BackingResource, upload_region.Offset, 0, 1, &data_desc);

			Transition(cl, new_states);
		}

		template<typename T>
		void FillFromBuffer(ID3D12GraphicsCommandList* cl, const std::vector<T>& buffer, D3D12_RESOURCE_STATES new_states)
		{
			FillFromBuffer(cl, buffer.data(), buffer.size(), new_states);
		}
	protected:
		Device* mDevice;

		ComPtr<ID3D12Resource> mResource;
		D3D12_RESOURCE_STATES mStates;
		CD3DX12_RESOURCE_DESC mDescription;

		Descriptor mDSV, mSRV, mCBV, mUAV;
	};
}