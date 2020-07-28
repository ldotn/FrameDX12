#pragma once
#include "../Core/stdafx.h"

namespace FrameDX12
{
	class RootSignature
	{
	public:
		RootSignature(class Device* device, CD3DX12_ROOT_SIGNATURE_DESC desc);
		ID3D12RootSignature* operator*() const { return mSignature.Get(); }
	private:
		ComPtr<ID3D12RootSignature> mSignature;
		CD3DX12_ROOT_SIGNATURE_DESC mDesc;
	};
}