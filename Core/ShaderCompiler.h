#pragma once
#include "../Core/stdafx.h"
#include <d3d12shader.h>    // Shader reflection.
#include <dxcapi.h>         // Be sure to link with dxcompiler.lib.

namespace FrameDX12
{
	struct CompiledShader
	{
		ComPtr<IDxcBlob> shader;
		ComPtr<IDxcBlob> pdb;
		ComPtr<IDxcBlob> hash;

		ComPtr<IDxcBlob> reflection_raw;
		ComPtr<ID3D12ShaderReflection> reflection;
	};

	class ShaderCompiler
	{
	public:
		static void Init();
		static CompiledShader CompileShader(const std::wstring& path, const std::wstring& target, const std::wstring& entry_point);
	private:
		static ComPtr<IDxcUtils> sUtils;
		static ComPtr<IDxcCompiler3> sCompiler;
		static ComPtr<IDxcIncludeHandler> sIncludeHandler;
	};
}
	