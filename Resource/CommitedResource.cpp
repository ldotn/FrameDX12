#include "CommitedResource.h"
#include "../Device/Device.h"
#include "../Core/Log.h"

using namespace FrameDX12;

thread_local std::vector<ComPtr<ID3D12Resource>> CommitedResource::mTempUploadResources;

void CommitedResource::Create(	Device* device, 
								CD3DX12_RESOURCE_DESC description,
								D3D12_RESOURCE_STATES initial_states,
								D3D12_CLEAR_VALUE* clear_value,
								D3D12_HEAP_TYPE heap_type,
								D3D12_HEAP_FLAGS heap_flags)
{
	mDevice = device;
	mStates = initial_states;
	
	auto heap_props = CD3DX12_HEAP_PROPERTIES(heap_type);
	ThrowIfFailed(mDevice->GetDevice()->CreateCommittedResource(
		&heap_props,
		heap_flags,
		&description,
		initial_states,
		clear_value,
		IID_PPV_ARGS(&mResource)));
}

void CommitedResource::CreateDSV()
{
	mDSV = mDevice->GetDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).GetNextDescriptor();
	mDevice->GetDevice()->CreateDepthStencilView(mResource.Get(), nullptr, *mDSV);
}

void CommitedResource::CreateCBV()
{
	mCBV = mDevice->GetDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetNextDescriptor();

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.SizeInBytes = mDescription.Width;
	desc.BufferLocation = mResource->GetGPUVirtualAddress();

	mDevice->GetDevice()->CreateConstantBufferView(&desc, *mCBV);
}

void CommitedResource::CreateSRV(D3D12_SHADER_RESOURCE_VIEW_DESC* desc_ptr)
{
	mSRV = mDevice->GetDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetNextDescriptor();
	mDevice->GetDevice()->CreateShaderResourceView(mResource.Get(), desc_ptr, *mSRV);
}

void CommitedResource::CreateUAV(D3D12_UNORDERED_ACCESS_VIEW_DESC* desc_ptr, ComPtr<ID3D12Resource> counter)
{
	mUAV = mDevice->GetDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetNextDescriptor();
	mDevice->GetDevice()->CreateUnorderedAccessView(mResource.Get(), counter.Get(), desc_ptr, *mUAV);
}

void CommitedResource::Transition(ID3D12GraphicsCommandList* cl, D3D12_RESOURCE_STATES new_states)
{
	if (mStates != new_states)
	{
		CD3DX12_RESOURCE_BARRIER transitions[] = { CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), mStates, new_states) };
		cl->ResourceBarrier(1, transitions);
		mStates = new_states;
	}
}