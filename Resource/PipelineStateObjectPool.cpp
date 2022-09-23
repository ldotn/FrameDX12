#include "PipelineStateObjectPool.h"
#include "../Device/Device.h"
#include "../Core/Log.h"

using namespace FrameDX12;

ID3D12PipelineState* PipelineStateObjectPool::GetPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
	auto entry = mPool.find(desc);
	if (entry != mPool.end())
	{
		return entry->second.Get();
	}
	else
	{
		ComPtr<ID3D12PipelineState> pso;
		ThrowIfFailed(mDevice->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)));
		mPool[desc] = pso;

		return pso.Get();
	}
}
ID3D12PipelineState* PipelineStateObjectPool::GetPSO(const D3D12_PIPELINE_STATE_STREAM_DESC& desc)
{
	auto entry = mPool.find(desc);
	if (entry != mPool.end())
	{
		return entry->second.Get();
	}
	else
	{
		ComPtr<ID3D12PipelineState> pso;
		ThrowIfFailed(mDevice->GetDevice<2>()->CreatePipelineState(&desc, IID_PPV_ARGS(&pso)));
		mPool[desc] = pso;

		return pso.Get();
	}
}