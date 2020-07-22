#pragma once
#include "BufferedResource.h"
#include "DescriptorVector.h"

namespace FrameDX12
{
	class Device;

	class RenderTarget : public BufferedResource<ComPtr<ID3D12Resource>>
	{
	public:
		void Transition(ID3D12GraphicsCommandList* cl, D3D12_RESOURCE_STATES new_states);

		// Creates the render target taking the buffers from the swap chain
		// Assumes that
		//	- There are kResourceBufferCount buffers on the swap chain
		//	- The buffers are on state D3D12_RESOURCE_STATE_PRESENT
		void CreateFromSwapchain(Device* device);
		Descriptor GetHandle() const { return *mHandle; }
	private:
		D3D12_RESOURCE_STATES mStates;
		BufferedResource<Descriptor> mHandle;
	};
}