#include "RenderTarget.h"
#include "../Device/Device.h"
#include "../Core/Log.h"

using namespace FrameDX12;

void RenderTarget::CreateFromSwapchain(Device* device)
{
	mHandle.Construct([this, device](uint8_t i)
	{
		ThrowIfFailed(device->GetSwapChain()->GetBuffer(i, IID_PPV_ARGS(&mResource[i])));

		Descriptor handle = device->GetDescriptorVector(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).GetNextDescriptor();
		device->GetDevice()->CreateRenderTargetView(mResource[i].Get(), nullptr, *handle);
		return handle;
	});
}
