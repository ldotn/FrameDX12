#pragma once
#include "../Core/stdafx.h"
#include "CommitedResource.h"

namespace FrameDX12
{
	struct CPUVertex
	{

		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT4 tangent;
		DirectX::XMFLOAT3 bitangent;
		DirectX::XMFLOAT2 uv;

		uint32 _padding0;

		bool operator==(const CPUVertex& other) const
		{
			using namespace DirectX;

			return XMVector3Equal(XMLoadFloat3(&position), XMLoadFloat3(&other.position)) &&
				XMVector3Equal(XMLoadFloat3(&normal), XMLoadFloat3(&other.normal)) &&
				XMVector4Equal(XMLoadFloat4(&tangent), XMLoadFloat4(&other.tangent)) &&
				XMVector3Equal(XMLoadFloat3(&bitangent), XMLoadFloat3(&other.bitangent)) &&
				XMVector2Equal(XMLoadFloat2(&uv), XMLoadFloat2(&other.uv));
		}
	};

	// Generic way to describe a vertex, with a default implementation
	struct VertexDesc
	{
		// Returns the DirectX input layout desc
		std::function<D3D12_INPUT_LAYOUT_DESC()> GetGPUDesc = []()
		{
			D3D12_INPUT_LAYOUT_DESC input_layout;

			static D3D12_INPUT_ELEMENT_DESC vertex_desc[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, 0                           , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TANGENT" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // w is the sign, used to flip
				{ "UV"      , 0, DXGI_FORMAT_R32G32_FLOAT      , 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			input_layout.pInputElementDescs = vertex_desc;
			input_layout.NumElements = _countof(vertex_desc);

			return input_layout;
		};

		// Size of the vertex on the data to be copied over to the GPU
		size_t vertex_size = 48;

		// Function to add a new vertex to the data buffer. It returns the buffer ready to write the next vertex
		std::function<void* (void*, const CPUVertex&)> AppendVertex = [](void* RawBuffer, const CPUVertex& Vertex)
		{
			float* Buffer = reinterpret_cast<float*>(RawBuffer);

			*(Buffer++) = Vertex.position.x;
			*(Buffer++) = Vertex.position.y;
			*(Buffer++) = Vertex.position.z;

			*(Buffer++) = Vertex.normal.x;
			*(Buffer++) = Vertex.normal.y;
			*(Buffer++) = Vertex.normal.z;

			*(Buffer++) = Vertex.tangent.x;
			*(Buffer++) = Vertex.tangent.y;
			*(Buffer++) = Vertex.tangent.z;
			*(Buffer++) = Vertex.tangent.w;

			*(Buffer++) = Vertex.uv.x;
			*(Buffer++) = Vertex.uv.y;

			return Buffer;
		};
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
			VertexDesc vertex_layout;
		} mDesc;

	public:
		Mesh() = default;
		Mesh(Mesh&&) = default;
		Mesh(const Mesh&) = delete;
		~Mesh();

		// Creates a mesh from an OBJ file
		// Adds all necessary commands to the referenced graph. The commands are all copy so you can use the copy queue here
		// Remember to call Build and Execute on the graph, and wait for the results, before drawing!
		void BuildFromOBJ(class Device* device, class CommandGraph& copy_graph, const std::string& path, VertexDesc&& vertex_desc = VertexDesc());

		// Sets the buffers and the draw command
		// Assumes that the IA is set to triangle list
		void Draw(ID3D12GraphicsCommandList* cl, uint32_t instances_count = 1);

		Description GetDesc() const { return mDesc; }

		const std::vector<uint32_t>& GetIndices() const { return mIndices; }
		const std::vector<CPUVertex>& GetVertices() const { return mVertices; }

		// Returns an independent copy of this mesh
		//Mesh Duplicate(class CommandGraph& copy_graph) const;
	private:
		std::vector<uint32_t> mIndices;
		std::vector<CPUVertex> mVertices;
		void* mUserFormatedVB = nullptr;

		D3D12_VERTEX_BUFFER_VIEW mVBV;
		D3D12_INDEX_BUFFER_VIEW mIBV;
		CommitedResource mVertexBuffer;
		CommitedResource mIndexBuffer;
	};
}

namespace std
{
	template<> struct hash<FrameDX12::CPUVertex>
	{
		size_t operator()(FrameDX12::CPUVertex const& vertex) const
		{
			// Hashing the raw bytes interpreted as a string
			return hash<string>()(string((const char*)&vertex, sizeof(FrameDX12::CPUVertex)));
		}
	};
}