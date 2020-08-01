#pragma once
#include "../Core/stdafx.h"
#include "CommitedResource.h"

namespace FrameDX12
{
	struct StandardVertex
	{
		inline static D3D12_INPUT_ELEMENT_DESC sDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0                           , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT" , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "UV"      , 0, DXGI_FORMAT_R32G32_FLOAT   , 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT3 tangent;
		DirectX::XMFLOAT2 uv;
	};

	template<typename VertexT>
	class Mesh
	{
	public:
		// Creates a mesh from an OBJ file
		// Adds all necessary commands to the referenced graph. The commands are all copy so you can use the copy queue here
		void BuildFromOBJ(class CommandGraph& copy_graph, const std::string& path);

		// Sets the buffers and the draw command
		// Assumes that the IA is set to triangle list
		void Draw(ID3D12GraphicsCommandList* cl);
	private:
		std::vector<StandardVertex> mVertices;
		std::vector<uint32_t> mIndices;

		D3D12_VERTEX_BUFFER_VIEW mVBV;
		D3D12_INDEX_BUFFER_VIEW mIBV;
		CommitedResource mVertexBuffer;
		CommitedResource mIndexBuffer;
	};
}