#pragma once
#include "DescriptorVector.h"

namespace FrameDX12
{
	class Device;

	class CommitedResource
	{
	public:
		// By default the resource is created on the COPY_DEST state, to be filled
		void Create(Device* device,
					CD3DX12_RESOURCE_DESC description,
					D3D12_RESOURCE_STATES initial_states = D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_CLEAR_VALUE* clear_value = nullptr, // It's ok to pass a pointer to an rvalue here
					D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT,
					D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE); 

		void CreateDSV(Device* device);

		// The callback is used to override default settings
		// Note that for buffers you NEED to fill Buffer.NumElements as it can't know the element size
		void CreateSRV(Device* device, std::function<void(D3D12_SHADER_RESOURCE_VIEW_DESC*)> callback = {});
		
		void CreateCBV(Device* device);

		// The callback is used to override default settings
		// Note that for buffers you NEED to fill Buffer.NumElements and Buffer.StructureByteStride as it can't know the element size
		void CreateUAV(Device* device, std::function<void(D3D12_SHADER_RESOURCE_VIEW_DESC*)> callback = {}, ComPtr<ID3D12Resource> counter = nullptr);

		// Fills the resource from a provided CPU buffer
		// Note that this only adds the command to the list, you need to sync before using the resource
		void FillFromBuffer(Device* device, ID3D12GraphicsCommandList* cl, D3D12_SUBRESOURCE_DATA data, D3D12_RESOURCE_STATES new_states);

		void Transition(ID3D12GraphicsCommandList* cl, D3D12_RESOURCE_STATES new_states);

		Descriptor GetDSV() const { return mDSV; }
		Descriptor GetSRV() const { return mSRV; }
		Descriptor GetCBV() const { return mCBV; }
		Descriptor GetUAV() const { return mUAV; }
	private:
		ComPtr<ID3D12Resource> mResource;
		D3D12_RESOURCE_STATES mStates;
		CD3DX12_RESOURCE_DESC mDescription;

		Descriptor mDSV, mSRV, mCBV, mUAV;
	};
}