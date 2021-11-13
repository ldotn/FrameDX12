#pragma once
#include "CommitedResource.h"

namespace FrameDX12
{
	template<typename DataT>
	class StructuredBuffer : private CommitedResource
	{
	public:
		// If initial data is provided, it MUST be valid until the graph nodes are executed (don't need to wait for the gpu to be done though)
		void Create(Device* device, size_t size, bool needs_uav = false, bool will_be_updated = true, const std::vector<DataT>& initial_data = {}, CommandGraph* copy_graph = nullptr)
		{
			mSize = size;

			D3D12_RESOURCE_FLAGS flags = needs_uav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
			D3D12_RESOURCE_STATES initial_state = initial_data.size() > 0 ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ;
			D3D12_HEAP_TYPE heap_type = will_be_updated ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
			CommitedResource::Create(device, CD3DX12_RESOURCE_DESC::Buffer(sizeof(DataT) * mSize, flags), initial_state, nullptr, heap_type);

			if (initial_data.size() > 0)
			{
				if (LogAssertAndContinue(copy_graph != nullptr, LogCategory::Error))
				{
					copy_graph->AddNode("", [this, &initial_data, copy_graph](ID3D12GraphicsCommandList* cl)
					{
						// Need to set it to common when using the copy queue
						// TODO : It would be nice if the command graph could batch resource barriers
						auto new_state = copy_graph->GetQueueType() == QueueType::Copy ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_GENERIC_READ;
						FillFromBuffer(cl, initial_data, new_state);
					}, nullptr, {});
				}
			}

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.Buffer.NumElements = mSize;
			srv_desc.Buffer.StructureByteStride = sizeof(DataT);
			CreateSRV(&srv_desc);

			if (needs_uav)
			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
				uav_desc.Format = DXGI_FORMAT_UNKNOWN;
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				uav_desc.Buffer.StructureByteStride = sizeof(DataT);
				uav_desc.Buffer.NumElements = mSize;
				CreateUAV(&uav_desc);
			}
		}

		// Maps the resource so it can be updated from the CPU. Optionally it can also map it so it can be read too
		void Map(bool map_for_read = false)
		{
			CD3DX12_RANGE empty_range(0, 0);
			mResource->Map(0, map_for_read ? nullptr : &empty_range, (void**)&mMappedMem);
		}

		void Unmap()
		{
			mMappedMem = nullptr;
			mResource->Unmap(0, nullptr);
		}

		void Update(const DataT& new_data, size_t index = 0)
		{
			if (LogAssertAndContinue(mMappedMem != nullptr, LogCategory::Warning))
			{
				memcpy(mMappedMem + index, &new_data, sizeof(DataT));
			}
		}
		void Update(const std::vector<DataT>& new_data, size_t base_index = 0)
		{
			if (LogAssertAndContinue(mMappedMem != nullptr && base_index + new_data.size() <= mSize, LogCategory::Warning))
			{
				memcpy(mMappedMem + base_index, new_data.data(), sizeof(DataT)*new_data.size());
			}
		}

		DataT* GetMappedPointer() const { return mMappedMem; }

		Descriptor GetSRV() const { return CommitedResource::GetSRV(); }
		Descriptor GetUAV() const { return CommitedResource::GetUAV(); }
		ID3D12Resource* operator->() { return mResource.Get(); }
	private:
		size_t  mSize;
		DataT* mMappedMem = nullptr;
	};
}