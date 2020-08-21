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

		bool operator==(const StandardVertex& other) const
		{
			using namespace DirectX;
			
			return XMVector3Equal(XMLoadFloat3(&position), XMLoadFloat3(&other.position)) &&
				   XMVector3Equal(XMLoadFloat3(&normal)  , XMLoadFloat3(&other.normal)  ) &&
				   XMVector3Equal(XMLoadFloat3(&tangent) , XMLoadFloat3(&other.tangent) ) &&
				   XMVector2Equal(XMLoadFloat2(&uv)      , XMLoadFloat2(&other.uv)      );
		}
	};

	class Mesh
	{
	private:
		struct Description
		{
			Description()
			{
				vertex_count = -1;
				triangle_count = -1;
				index_count = -1;
			}

			uint32_t vertex_count;
			uint32_t triangle_count;
			uint32_t index_count;
		} mDesc;

	public:
		Mesh() = default;
		Mesh(Mesh&&) = default;
		Mesh(const Mesh&) = delete;

		// Creates a mesh from an OBJ file
		// Adds all necessary commands to the referenced graph. The commands are all copy so you can use the copy queue here
		// Remember to call Build and Execute on the graph, and wait for the results, before drawing!
		void BuildFromOBJ(class Device* device, class CommandGraph& copy_graph, const std::string& path);

		// Sets the buffers and the draw command
		// Assumes that the IA is set to triangle list
		void Draw(ID3D12GraphicsCommandList* cl);

		Description GetDesc() const { return mDesc; }
	private:
		std::vector<StandardVertex> mVertices;
		std::vector<uint32_t> mIndices;

		D3D12_VERTEX_BUFFER_VIEW mVBV;
		D3D12_INDEX_BUFFER_VIEW mIBV;
		CommitedResource mVertexBuffer;
		CommitedResource mIndexBuffer;
	};
}

namespace std
{
	template<> struct hash<FrameDX12::StandardVertex>
	{
		size_t operator()(FrameDX12::StandardVertex const& vertex) const
		{
			// Hashing the raw bytes interpreted as a string
			return hash<string>()(string((const char*)&vertex, sizeof(FrameDX12::StandardVertex)));
		}
	};
}