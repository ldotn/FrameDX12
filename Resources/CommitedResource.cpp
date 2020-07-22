#include "CommitedResource.h"
#include "../Device/Device.h"
#include "../Core/Log.h"

using namespace FrameDX12;

void CommitedResource::Create(	Device* device, 
								CD3DX12_RESOURCE_DESC description,
								D3D12_RESOURCE_STATES initial_states,
								D3D12_CLEAR_VALUE* clear_value,
								D3D12_HEAP_TYPE heap_type,
								D3D12_HEAP_FLAGS heap_flags)
{
	mDevice = device;
	mStates = initial_states;

	ThrowIfFailed(mDevice->GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heap_type),
		heap_flags,
		&description,
		initial_states,
		clear_value,
		IID_PPV_ARGS(&mResource)));
}

void CommitedResource::CreateDSV()
{
	mDSV = mDevice->GetDescriptorVector(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).GetNextDescriptor();
	mDevice->GetDevice()->CreateDepthStencilView(mResource.Get(), nullptr, *mDSV);
}

void CommitedResource::CreateCBV()
{
	mCBV = mDevice->GetDescriptorVector(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetNextDescriptor();

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.SizeInBytes = mDescription.Width;
	desc.BufferLocation = mResource->GetGPUVirtualAddress();

	mDevice->GetDevice()->CreateConstantBufferView(&desc, *mCBV);
}

void CommitedResource::CreateSRV(D3D12_SHADER_RESOURCE_VIEW_DESC* desc_ptr)
{
	mSRV = mDevice->GetDescriptorVector(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetNextDescriptor();
	mDevice->GetDevice()->CreateShaderResourceView(mResource.Get(), desc_ptr, *mSRV);
}

void CommitedResource::CreateUAV(D3D12_UNORDERED_ACCESS_VIEW_DESC* desc_ptr, ComPtr<ID3D12Resource> counter)
{
	mUAV = mDevice->GetDescriptorVector(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetNextDescriptor();
	mDevice->GetDevice()->CreateUnorderedAccessView(mResource.Get(), counter.Get(), desc_ptr, *mUAV);
}

void CommitedResource::Transition(ID3D12GraphicsCommandList* cl, D3D12_RESOURCE_STATES new_states)
{
	if (mStates != new_states)
	{
		cl->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), mStates, new_states));
		mStates = new_states;
	}
}

void CommitedResource::FillFromBuffer(ID3D12GraphicsCommandList* cl, D3D12_SUBRESOURCE_DATA data, D3D12_RESOURCE_STATES new_states)
{
	Transition(cl, D3D12_RESOURCE_STATE_COPY_DEST);
	
	// TODO : Reuse the upload resource
	ComPtr<ID3D12Resource> upload_resource;
	ThrowIfFailed(mDevice->GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(data.RowPitch),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&upload_resource)));

	UpdateSubresources<1>(cl, mResource.Get(), upload_resource.Get(), 0, 0, 1, &data);

	Transition(cl, new_states);
}
