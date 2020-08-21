#include "RenderTarget.h"
#include "../Device/Device.h"
#include "../Core/Log.h"

using namespace FrameDX12;

void RenderTarget::CreateFromSwapchain(Device* device)
{
	mStates = D3D12_RESOURCE_STATE_PRESENT;
	mHandle.Construct([this, device](uint8_t i)
	{
		ThrowIfFailed(device->GetSwapChain()->GetBuffer(i, IID_PPV_ARGS(&mResource[i])));

		Descriptor handle = device->GetDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).GetNextDescriptor();
		device->GetDevice()->CreateRenderTargetView(mResource[i].Get(), nullptr, *handle);
		return handle;
	});
}

void RenderTarget::Transition(ID3D12GraphicsCommandList* cl, D3D12_RESOURCE_STATES new_states)
{
	if (mStates != new_states)
	{
		cl->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetResource().Get(), mStates, new_states));
		mStates = new_states;
	}
}