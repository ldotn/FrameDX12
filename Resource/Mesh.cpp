#include "Mesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "../Device/CommandGraph.h"
#include "../Core/Utils.h"
#include <DirectXMesh.h>

using namespace FrameDX12;

void Mesh::BuildFromOBJ(Device* device, CommandGraph& copy_graph, const std::string& path, VertexDesc&& vertex_desc)
{
    mDesc.vertex_layout = vertex_desc;

    using namespace std;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    bool succeeded = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());

    if (!warn.empty()) LogMsg(StringToWString(warn), LogCategory::Warning);
    if (!err.empty()) LogMsg(StringToWString(err), LogCategory::Error);

    if (!succeeded)
        return;

    // Create CPU-side vertex and index buffers
    unordered_map<CPUVertex, uint32_t> unique_vertices = {};

    size_t reserve_size = 0;
    for (const auto& shape : shapes)
        reserve_size += shape.mesh.indices.size() * 3;
    mVertices.reserve(reserve_size);
    mIndices.reserve(reserve_size);

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            CPUVertex vertex;
            vertex.position.x = attrib.vertices[3 * index.vertex_index + 0];
            vertex.position.y = attrib.vertices[3 * index.vertex_index + 1];
            vertex.position.z = attrib.vertices[3 * index.vertex_index + 2];

            vertex.normal.x = attrib.normals[3 * index.normal_index + 0];
            vertex.normal.y = attrib.normals[3 * index.normal_index + 1];
            vertex.normal.z = attrib.normals[3 * index.normal_index + 2];

            vertex.uv.x = attrib.texcoords[2 * index.texcoord_index + 0];
            vertex.uv.y = attrib.texcoords[2 * index.texcoord_index + 1];

            vertex.tangent.x = vertex.tangent.y = vertex.tangent.z = vertex.tangent.w = 0;
            vertex.bitangent.x = vertex.bitangent.y = vertex.bitangent.z = 0;

            if (!unique_vertices.count(vertex))
            {
                unique_vertices[vertex] = static_cast<uint32_t>(mVertices.size());
                mVertices.push_back(vertex);
            }

            mIndices.push_back(unique_vertices[vertex]);
        }
    }

    mDesc.index_count = mIndices.size();
    mDesc.vertex_count = mVertices.size();
    mDesc.triangle_count = mDesc.index_count / 3;

    // Using DirectXMesh to create the tangents
    // Need to unpack the struct for the dx tangent frame computation
    // TODO : Find a way to avoid this duplications
    vector<DirectX::XMFLOAT3> raw_vertices(mVertices.size());
    vector<DirectX::XMFLOAT3> raw_normals(mVertices.size());
    vector<DirectX::XMFLOAT2> raw_uvs(mVertices.size());

    for (auto [idx, vertex] : fpp::enumerate(mVertices))
    {
        raw_vertices[idx] = vertex.position;
        raw_normals[idx]  = vertex.normal;
        raw_uvs[idx]      = vertex.uv;
    }

    vector<DirectX::XMFLOAT4> raw_tangents(mVertices.size());
    vector<DirectX::XMFLOAT3> raw_bitangents(mVertices.size());
    DirectX::ComputeTangentFrame(mIndices.data(), mDesc.triangle_count, raw_vertices.data(), raw_normals.data(), raw_uvs.data(), mDesc.vertex_count, raw_tangents.data(), raw_bitangents.data());

    for (auto [idx, vertex] : fpp::enumerate(mVertices))
    {
        vertex.tangent = raw_tangents[idx];
        vertex.bitangent = raw_bitangents[idx];
    }

    // Convert the CPU vertex to the user defined representation
    size_t buffer_size = mVertices.size() * vertex_desc.vertex_size;
    void* user_vb = malloc(buffer_size);
    mUserFormatedVB = user_vb;

    for (const CPUVertex& vertex : mVertices)
        user_vb = mDesc.vertex_layout.AppendVertex(user_vb, vertex);

    // Create GPU-side buffers
    mIndexBuffer.Create(device, CD3DX12_RESOURCE_DESC::Buffer(mIndices.size() * sizeof(uint32_t)));
    mVertexBuffer.Create(device, CD3DX12_RESOURCE_DESC::Buffer(buffer_size));

    copy_graph.AddNode("", [this,buffer_size, &copy_graph](ID3D12GraphicsCommandList* cl)
    {
        // Need to set it to common when using the copy queue
        // TODO : It would be nice if the command graph could batch resource barriers
        auto new_state = copy_graph.GetQueueType() == QueueType::Copy ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_GENERIC_READ;
        mIndexBuffer.FillFromBuffer(cl, mIndices, new_state);
        mVertexBuffer.FillFromBuffer(cl, reinterpret_cast<char*>(mUserFormatedVB), buffer_size, new_state);
    }, nullptr, {});

    mVBV.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVBV.SizeInBytes = buffer_size;
    mVBV.StrideInBytes = mDesc.vertex_layout.vertex_size;

    mIBV.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    mIBV.SizeInBytes = mIndices.size() * sizeof(uint32_t);
    mIBV.Format = DXGI_FORMAT_R32_UINT;
}

void Mesh::Draw(ID3D12GraphicsCommandList* cl, uint32_t instances_count)
{
    // Remember, the transitions do nothing (that is, this function, not DX barriers) if the resource is already on the state you want
    mVertexBuffer.Transition(cl, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    mIndexBuffer.Transition(cl, D3D12_RESOURCE_STATE_INDEX_BUFFER);

    cl->IASetIndexBuffer(&mIBV);
    cl->IASetVertexBuffers(0, 1, &mVBV);
    cl->DrawIndexedInstanced(mIndices.size(), instances_count, 0, 0, 0);
}

Mesh::~Mesh()
{
    free(mUserFormatedVB);
}

/*Mesh Mesh::Duplicate(class CommandGraph& copy_graph) const
{
    Mesh copy;
    copy.mDesc = mDesc;
    copy.mIndices = mIndices;
    copy.mVertices = mVertices;

    size_t buffer_size = mVertices.size() * mDesc.vertex_layout.vertex_size;
    copy.mUserFormatedVB = malloc(buffer_size);
    memcpy(copy.mUserFormatedVB, mUserFormatedVB, buffer_size);

}*/