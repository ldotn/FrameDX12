#pragma once
#include "../Core/stdafx.h"
#include <variant>
#include "../Core/Utils.h"

typedef std::variant<D3D12_GRAPHICS_PIPELINE_STATE_DESC, D3D12_PIPELINE_STATE_STREAM_DESC> D3D12_PSO_DESC_TYPES;

inline bool operator==(const D3D12_PSO_DESC_TYPES& lhs, const D3D12_PSO_DESC_TYPES& rhs)
{
    bool equal = false;
    if (lhs.index() == rhs.index())
    {
        if(lhs.index() == 0)
            equal = memcmp(&std::get<0>(lhs), &std::get<0>(rhs), sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC)) == 0;
        else
            equal = memcmp(&std::get<1>(lhs), &std::get<1>(rhs), sizeof(D3D12_PIPELINE_STATE_STREAM_DESC)) == 0;
    }
    
    return equal;
}
inline bool operator!=(const D3D12_PSO_DESC_TYPES& lhs, const D3D12_PSO_DESC_TYPES& rhs)
{
    return !(lhs == rhs);
}

namespace std
{
    template <>
    struct hash<D3D12_PSO_DESC_TYPES>
    {
        size_t operator()(const D3D12_PSO_DESC_TYPES& key) const
        {
            if (key.index() == 0)
                return FrameDX12::HashBytes(std::get<0>(key));
            else
                return FrameDX12::HashBytes(std::get<1>(key));
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

        // TODO : Remove the graphics one and use stream for all, converting is quite trivial with the d3dx_stream classes
        ID3D12PipelineState* GetPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
        ID3D12PipelineState* GetPSO(const D3D12_PIPELINE_STATE_STREAM_DESC& desc);
	private:
        Device* mDevice = nullptr;
        std::unordered_map<D3D12_PSO_DESC_TYPES, ComPtr<ID3D12PipelineState>> mPool;
	};
}