#pragma once
#include "DescriptorPool.h"
#include "../Device/Device.h"
#include "../Core/Log.h"

namespace FrameDX12
{
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

			// Try to find a temp resource big enough or create it if it doesn't exist
			// TODO : Check when we can delete them, not easy as they need to be alive by the time Close is called
			// TODO 2 : Making it threadlocal for now, but it could most likely be shared across threads
			// TODO 3 : You CAN'T reuse the same buffer on the same CL, you need to either use different buffers or do an Execute.
			//			 FIND A *NICE* WAY TO REUSE STUFF!
			ComPtr<ID3D12Resource> upload_resource;
			/*for (auto& resource : mTempUploadResources)
			{
				if (resource->GetDesc().Width >= data_desc.RowPitch)
				{
					upload_resource = resource;
					break;
				}
			}*/

			/*do a wrapper for the command lists, keep track from that graph they came and more importantly the temp resources associated with it
				then you know that temp resources can be reused after Close is called*/

			if (!upload_resource)
			{
				auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
				auto buff_desc = CD3DX12_RESOURCE_DESC::Buffer(data_desc.RowPitch);

				ThrowIfFailed(mDevice->GetDevice()->CreateCommittedResource(
					&heap_prop,
					D3D12_HEAP_FLAG_NONE,
					&buff_desc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&upload_resource)));
				mTempUploadResources.push_back(upload_resource);
			}

			UpdateSubresources<1>(cl, mResource.Get(), upload_resource.Get(), 0, 0, 1, &data_desc);

			Transition(cl, new_states);
		}

		template<typename T>
		void FillFromBuffer(ID3D12GraphicsCommandList* cl, const std::vector<T>& buffer, D3D12_RESOURCE_STATES new_states)
		{
			FillFromBuffer(cl, buffer.data(), buffer.size(), new_states);
		}

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

			return copy;
		}
	protected:
		static thread_local std::vector<ComPtr<ID3D12Resource>> mTempUploadResources;

		Device* mDevice;

		ComPtr<ID3D12Resource> mResource;
		D3D12_RESOURCE_STATES mStates;
		CD3DX12_RESOURCE_DESC mDescription;

		Descriptor mDSV, mSRV, mCBV, mUAV;
	};
}