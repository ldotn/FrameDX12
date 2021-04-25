#pragma once
#include "../Core/stdafx.h"
#include "../Device/Device.h"
#include "CommitedResource.h"

namespace FrameDX12
{
	// Constant buffer of the specified type that remains mapped for updates
	// There's an optional Count creation parameter to create an array of CBs that use the same underlying resource
	template<typename DataT>
	class ConstantBuffer
	{
		static_assert(alignof(DataT) == D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "Constant buffer data needs to be aligned to D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT bytes");
	public:
		void Create(Device * device, size_t count = 1)
		{
			mResource.Create(device, CD3DX12_RESOURCE_DESC::Buffer(sizeof(DataT) * count), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
			mResource->Map(0, &readRange, reinterpret_cast<void**>(&mMappedMem));

			mViews.resize(count);
			for (size_t idx = 0; idx < count;++idx)
			{
				mViews[idx] = device->GetDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetNextDescriptor();

				D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
				desc.SizeInBytes = sizeof(DataT);
				desc.BufferLocation = mResource->GetGPUVirtualAddress() + idx * sizeof(DataT);

				device->GetDevice()->CreateConstantBufferView(&desc, *mViews[idx]);
			}
		}

		Descriptor GetView(size_t index = 0) const { return mViews[index]; }
		void Update(const DataT& new_data, size_t index = 0)
		{
			memcpy(mMappedMem + index, &new_data, sizeof(DataT));
		}
	private:
		CommitedResource mResource;
		std::vector<Descriptor> mViews;

		// Write only memory
		DataT* mMappedMem;
	};
}
