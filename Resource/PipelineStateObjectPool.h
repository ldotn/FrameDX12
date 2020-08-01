#pragma once
#include "../Core/stdafx.h"

inline bool operator==(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& lhs, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& rhs)
{
    return memcmp(&lhs, &rhs, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC)) == 0;
}
inline bool operator!=(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& lhs, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& rhs)
{
    return !(lhs == rhs);
}

namespace std
{
    template <>
    struct hash<D3D12_GRAPHICS_PIPELINE_STATE_DESC>
    {
        size_t operator()(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& key) const
        {
            uint8_t* bytes = (uint8_t*)&key;

            // TODO : Evaluate better hash methods
            size_t hash_val = 42;
            for (size_t i = 0; i < sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC); i++)
            {
                static_assert(sizeof(size_t) * 8 >= 53);
                hash_val ^= hash<uint8_t>()(bytes[i]) << (i % 53);
            }

            return hash_val;
        }
    };
}

namespace FrameDX12
{
    class Device;

	class PipelineStateObjectPool
	{
	public:
        PipelineStateObjectPool(Device* device) :
            mDevice(device)
        {}

        ID3D12PipelineState* GetPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
	private:
        Device* mDevice = nullptr;
        std::unordered_map<D3D12_GRAPHICS_PIPELINE_STATE_DESC, ComPtr<ID3D12PipelineState>> mPool;
	};
}