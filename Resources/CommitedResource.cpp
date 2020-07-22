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
	mStates = initial_states;
	ID3D12Device* device_ptr = device->GetDevice();

	ThrowIfFailed(device_ptr->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heap_type),
		heap_flags,
		&description,
		initial_states,
		clear_value,
		IID_PPV_ARGS(&mResource)));
}

void CommitedResource::CreateDSV(Device* device)
{
	mDSV = device->GetDescriptorVector(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).GetNextDescriptor();
	device->GetDevice()->CreateDepthStencilView(mResource.Get(), nullptr, *mDSV);
}

void CommitedResource::CreateSRV(Device* device, std::function<void(D3D12_SHADER_RESOURCE_VIEW_DESC*)> callback)
{
	mSRV = device->GetDescriptorVector(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetNextDescriptor();

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // TODO : Check this
	srv_desc.Format = mDescription.Format;

	if (mDescription.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	}
	else if (mDescription.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
	{
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		srv_desc.Texture1D.MipLevels = mDescription.MipLevels;
	}
	else if (mDescription.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		if (mDescription.DepthOrArraySize > 1)
		{
			if (mDescription.SampleDesc.Count > 1)
			{
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
				srv_desc.Texture2DMSArray.ArraySize = mDescription.DepthOrArraySize;
			}
			else
			{
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				srv_desc.Texture2DArray.ArraySize = mDescription.DepthOrArraySize;
			}
		}
		else
		{
			if (mDescription.SampleDesc.Count > 1)
			{
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srv_desc.Texture2D.MipLevels = mDescription.MipLevels;
			}
		}
		// TODO : Look into cube textures
	}
	else if (mDescription.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
	{
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srv_desc.Texture3D.MipLevels = mDescription.MipLevels;
	}

	if(callback) callback(&srv_desc);
	device->GetDevice()->CreateShaderResourceView(mResource.Get(), &srv_desc, *mSRV);
}

void CommitedResource::CreateCBV(Device* device)
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.SizeInBytes = mDescription.Width;
	desc.BufferLocation = mResource->GetGPUVirtualAddress();

	device->GetDevice()->CreateConstantBufferView(&desc, *mCBV);
}

void CommitedResource::CreateUAV(Device* device, std::function<void(D3D12_SHADER_RESOURCE_VIEW_DESC*)> callback, ComPtr<ID3D12Resource> counter)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};

	desc.Format = mDescription.Format;

	if (mDescription.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	}
	else if (mDescription.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
	{
		desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
	}
	else if (mDescription.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		if (mDescription.DepthOrArraySize > 1)
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = mDescription.DepthOrArraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		}
		// TODO : Look into cube textures
	}
	else if (mDescription.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
	{
		desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		desc.Texture3D.WSize = mDescription.DepthOrArraySize;
	}

	device->GetDevice()->CreateUnorderedAccessView(mResource.Get(), counter.Get(), &desc, *mUAV);
}

void CommitedResource::Transition(ID3D12GraphicsCommandList* cl, D3D12_RESOURCE_STATES new_states)
{
	if (mStates != new_states)
	{
		cl->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), mStates, new_states));
		mStates = new_states;
	}
}

void CommitedResource::FillFromBuffer(Device* device, ID3D12GraphicsCommandList* cl, D3D12_SUBRESOURCE_DATA data, D3D12_RESOURCE_STATES new_states)
{
	Transition(cl, D3D12_RESOURCE_STATE_COPY_DEST);
	
	// TODO : Reuse the upload resource
	ComPtr<ID3D12Resource> upload_resource;
	ThrowIfFailed(device->GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(data.RowPitch),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&upload_resource)));

	UpdateSubresources<1>(cl, mResource.Get(), upload_resource.Get(), 0, 0, 1, &data);

	Transition(cl, new_states);
}
