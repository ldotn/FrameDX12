#define WIN32_LEAN_AND_MEAN // Exclude rarely used stuff from Windows headers
#define NOMINMAX
#include <Windows.h>
#include <crtdbg.h>
#include "../Core/Log.h"
#include "../Core/Window.h"
#include "../Device/Device.h"
#include "../Device/CommandGraph.h"
#include "../Resources/RenderTarget.h"
#include "../Resources/CommitedResource.h"
#include <iostream>
#include "pix3.h"

using namespace FrameDX12;
using namespace std;
using namespace fpp;

constexpr int kWorkerCount = 2;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // -------------------------------
    //      General setup
    // -------------------------------

    // Init the log
    Log.CreateConsole();
    auto print_thread = Log.FirePrintThread();

    // Create the window
    Window window;

    // Create the device
    Device dev(&window);

    // -------------------------------
    //      Specific setup
    // -------------------------------

    // Create the resources
    RenderTarget backbuffer;
    backbuffer.CreateFromSwapchain(&dev);

    CommitedResource depth_buffer;
    {
        CD3DX12_RESOURCE_DESC depth_desc(
            D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            0,
            static_cast<UINT>(window.GetSizeX()),
            static_cast<UINT>(window.GetSizeY()),
            1,
            1,
            DXGI_FORMAT_D32_FLOAT,
            1,
            0,
            D3D12_TEXTURE_LAYOUT_UNKNOWN,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

        D3D12_CLEAR_VALUE clear_value;	// Performance tip: Tell the runtime at resource creation the desired clear value.
        clear_value.Format = DXGI_FORMAT_D32_FLOAT;
        clear_value.DepthStencil.Depth = 1.0f;
        clear_value.DepthStencil.Stencil = 0;

        depth_buffer.Create(&dev, depth_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value);
        depth_buffer.CreateDSV();
    }

    //  Create root signature
    ComPtr<ID3D12RootSignature> root_signature;
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[1]; // Perfomance TIP: Order from most frequent to least frequent.
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);		// 1 frequently changed constant buffer.

        CD3DX12_ROOT_PARAMETER rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(dev.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
    }

    // Create geometry
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
    };
    const D3D12_INPUT_ELEMENT_DESC vertex_desc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    vector<Vertex> vertices =
    {
        {DirectX::XMFLOAT3(-0.8,0.9,0)},
        {DirectX::XMFLOAT3(0,-0.8,0)},
        {DirectX::XMFLOAT3(0.8,0,0.5)}
    };
    vector<uint16_t> indices = { 2, 1, 0 };

    CommitedResource index_buffer, vertex_buffer;
    index_buffer.Create(&dev, CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(uint16_t)));
    vertex_buffer.Create(&dev, CD3DX12_RESOURCE_DESC::Buffer(vertices.size() * sizeof(Vertex)));

    CommandGraph copy_graph(kWorkerCount, QueueType::Copy, &dev);
    copy_graph.AddNode("Mesh0", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        // TODO : Store the node name and move this to the command graph
        PIXScopedEvent(cl, 0, L"Mesh0");

        // Need to set it to common when using the copy queue
        // TODO : It would be nice if the command graph could batch resource barriers
        vertex_buffer.FillFromBuffer(cl, vertices, D3D12_RESOURCE_STATE_COMMON);
        index_buffer.FillFromBuffer(cl, indices, D3D12_RESOURCE_STATE_COMMON);
    }, {});
    copy_graph.Build(&dev);
    copy_graph.Execute(&dev);

    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
    vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
    vertex_buffer_view.SizeInBytes = vertices.size() * sizeof(Vertex);
    vertex_buffer_view.StrideInBytes = sizeof(Vertex);

    D3D12_INDEX_BUFFER_VIEW index_buffer_view;
    index_buffer_view.BufferLocation = index_buffer->GetGPUVirtualAddress();
    index_buffer_view.SizeInBytes = indices.size() * sizeof(uint16_t);
    index_buffer_view.Format = DXGI_FORMAT_R16_UINT;

    // Load shaders
    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> pixel_shader;

#ifdef _DEBUG
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> error_blob;
    LogCheck(D3DCompileFromFile(L"SimpleShaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertex_shader, &error_blob), LogCategory::Error);
    if (error_blob)
    {
        PrintErrorBlob(error_blob);
        error_blob->Release();
    }

    LogCheck(D3DCompileFromFile(L"SimpleShaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixel_shader, &error_blob), LogCategory::Error);
    if (error_blob)
    {
        PrintErrorBlob(error_blob);
        error_blob->Release();
    }

    // Define pipeline state 
    D3D12_INPUT_LAYOUT_DESC input_layout;
    input_layout.pInputElementDescs = vertex_desc;
    input_layout.NumElements = _countof(vertex_desc);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state = {};
    pipeline_state.InputLayout = input_layout;
    pipeline_state.pRootSignature = root_signature.Get();
    pipeline_state.VS = { reinterpret_cast<UINT8*>(vertex_shader->GetBufferPointer()), vertex_shader->GetBufferSize() };
    pipeline_state.PS = { reinterpret_cast<UINT8*>(pixel_shader->GetBufferPointer()), pixel_shader->GetBufferSize() };
    pipeline_state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipeline_state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipeline_state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipeline_state.SampleMask = UINT_MAX;
    pipeline_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_state.NumRenderTargets = 1;
    pipeline_state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipeline_state.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipeline_state.SampleDesc.Count = 1;

    // -------------------------------
    //      Render setup
    // -------------------------------

    // Create the command graph
    CommandGraph commands(kWorkerCount, QueueType::Graphics, &dev);

    commands.AddNode("DrawSetup", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        PIXScopedEvent(cl, 0, L"DrawSetup");

        // TODO : Theres no need to do this each frame, it can be done just once
        vertex_buffer.Transition(cl, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        index_buffer.Transition(cl, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    }, {});

    commands.AddNode("Clear", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        PIXScopedEvent(cl, 0, L"Clear");

        backbuffer.Transition(cl, D3D12_RESOURCE_STATE_RENDER_TARGET);

        cl->ClearRenderTargetView(*backbuffer.GetHandle(), DirectX::Colors::Magenta, 0, nullptr);
        cl->ClearDepthStencilView(*depth_buffer.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }, {});

    // TODO : Add functions on the window to return this structs
    D3D12_VIEWPORT viewport = {};
    viewport.Width = window.GetSizeX();
    viewport.Height = window.GetSizeY();
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor_rect = {};
    scissor_rect.right = static_cast<LONG>(viewport.Width);
    scissor_rect.bottom = static_cast<LONG>(viewport.Height);

    commands.AddNode("Draw", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        PIXScopedEvent(cl, 0, L"Draw");

        // While this state is shared, and could be set earlier, doing execute command lists seems to clear it
        // TODO : Move this to a init stage on the node execution, to prevent having this fire on each repeat
        cl->RSSetViewports(1, &viewport);
        cl->RSSetScissorRects(1, &scissor_rect);
        cl->SetGraphicsRootSignature(root_signature.Get());
        cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cl->OMSetRenderTargets(1, &*backbuffer.GetHandle(), false, &*depth_buffer.GetDSV());

        cl->IASetIndexBuffer(&index_buffer_view);
        cl->IASetVertexBuffers(0, 1, &vertex_buffer_view);
        cl->DrawIndexedInstanced(indices.size(), 1, 0, 0, 0);
    }, {"Clear", "DrawSetup"});

    commands.AddNode("Present", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        PIXScopedEvent(cl, 0, L"Present");

        backbuffer.Transition(cl, D3D12_RESOURCE_STATE_PRESENT);
    }, {"Draw"});

    commands.Build(&dev);

    // -------------------------------
    //      Render loop
    // -------------------------------
    // Make sure all transfers finished
    dev.WaitForQueue(QueueType::Copy);

    // Enter the render loop
    window.CallDuringIdle([&](double elapsed_time)
    {
        commands.Execute(&dev, dev.GetPSO(pipeline_state));
        dev.GetSwapChain()->Present(0, 0);

        // Advance buffer index
        sCurrentResourceBufferIndex++;

        // If we are going to roll back to the first allocator, then we need to wait
        if (sCurrentResourceBufferIndex % kResourceBufferCount == 0)
        {
            dev.WaitForQueue(QueueType::Graphics);
            // Add the other queues if you are doing work there
        }

        return false;
    });

    return 0;
}