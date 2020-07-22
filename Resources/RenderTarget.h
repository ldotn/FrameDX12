#pragma once
#include "BufferedResource.h"
#include "DescriptorVector.h"

namespace FrameDX12
{
	class Device;

	class RenderTarget : public BufferedResource<ComPtr<ID3D12Resource>>
	{
	public:
		void CreateFromSwapchain(Device* device);
		Descriptor GetHandle() const { return *mHandle; }
	private:
		BufferedResource<Descriptor> mHandle;
	};
}