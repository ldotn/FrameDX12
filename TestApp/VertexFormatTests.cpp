#if 0
#define WIN32_LEAN_AND_MEAN // Exclude rarely used stuff from Windows headers
#define NOMINMAX
#include <Windows.h>
#include <crtdbg.h>
#include "../Core/Log.h"
#include "../Core/Window.h"
#include "../Device/Device.h"
#include "../Device/CommandGraph.h"
#include "../Resource/RenderTarget.h"
#include "../Resource/CommitedResource.h"
#include "../Resource/Mesh.h"
#include "../Resource/ConstantBuffer.h"
#include "../Resource/StructuredBuffer.h"
#include <iostream>
#include "pix3.h"

using namespace FrameDX12;
using namespace std;
using namespace fpp;
using namespace DirectX;

constexpr int kWorkerCount = 4;

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
        //ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);		// 1 frequently changed constant buffer.
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);		// no cb on this, using a structured buffer

        CD3DX12_ROOT_PARAMETER rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(dev.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
    }

    // Load shaders
    
    ComPtr<ID3DBlob> pixel_shader;

#ifdef _DEBUG
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    // TODO : Handle failure
    ComPtr<ID3DBlob> error_blob;
    ComPtr<ID3DBlob> vertex_shader_standard;
    LogCheck(D3DCompileFromFile(L"VertexFormatShaders.hlsl", nullptr, nullptr, "VS_Standard", "vs_5_1", compileFlags, 0, &vertex_shader_standard, &error_blob), LogCategory::Error);
    LogErrorBlob(error_blob);

    ComPtr<ID3DBlob> vertex_shader_axis_angle;
    LogCheck(D3DCompileFromFile(L"VertexFormatShaders.hlsl", nullptr, nullptr, "VS_AxisAngle", "vs_5_0", compileFlags, 0, &vertex_shader_axis_angle, &error_blob), LogCategory::Error);
    LogErrorBlob(error_blob);

    LogCheck(D3DCompileFromFile(L"VertexFormatShaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixel_shader, &error_blob), LogCategory::Error);
    LogErrorBlob(error_blob);

    // Load mesh
    CommandGraph copy_graph(kWorkerCount, QueueType::Copy, &dev);

    VertexDesc axis_angle_vtx;
    axis_angle_vtx.GetGPUDesc = []()
    {
        D3D12_INPUT_LAYOUT_DESC input_layout;

        static D3D12_INPUT_ELEMENT_DESC vertex_desc[] =
        {
            { "Position", 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, 0                           , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "Axis"    , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "Angle"   , 0, DXGI_FORMAT_R16G16_SNORM      , 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // Cos and Sin of the angle to align the tangent frame
            { "UV"      , 0, DXGI_FORMAT_R32G32_FLOAT      , 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        input_layout.pInputElementDescs = vertex_desc;
        input_layout.NumElements = _countof(vertex_desc);

        return input_layout;
    };
    axis_angle_vtx.AppendVertex = [](void* RawBuffer, const CPUVertex& Vertex)
    {
        float* Buffer = reinterpret_cast<float*>(RawBuffer);
        
        *(Buffer++) = Vertex.position.x;
        *(Buffer++) = Vertex.position.y;
        *(Buffer++) = Vertex.position.z;
        
        // https://en.wikipedia.org/wiki/Axis%E2%80%93angle_representation#Log_map_from_SO(3)_to_%7F'%22%60UNIQ--postMath-0000000D-QINU%60%22'%7F(3)
        // https://dev.theomader.com/qtangents/
        /*
        *   Vertex.normal.x, Vertex.normal.y, Vertex.normal.z,
            Vertex.tangent.x, Vertex.tangent.y, Vertex.tangent.z,
            Vertex.bitangent.x, Vertex.bitangent.y, Vertex.bitangent.z
        */
        /*
        *   Vertex.normal.x, Vertex.tangent.x, Vertex.bitangent.x,
            Vertex.normal.y, Vertex.tangent.y, Vertex.bitangent.y,
            Vertex.normal.z, Vertex.tangent.z, Vertex.bitangent.z
        */
        /*
        *   Vertex.bitangent.x, Vertex.bitangent.y, Vertex.bitangent.z,
            Vertex.tangent.x, Vertex.tangent.y, Vertex.tangent.z,
            Vertex.normal.x, Vertex.normal.y, Vertex.normal.z
        */
        XMMATRIX rot_mat = XMMatrixSet(Vertex.bitangent.x, Vertex.bitangent.y, Vertex.bitangent.z, 0.0f,
            Vertex.tangent.x, Vertex.tangent.y, Vertex.tangent.z, 0.0f,
            Vertex.normal.x, Vertex.normal.y, Vertex.normal.z, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f);

        
        float nflip = XMVectorGetX(XMMatrixDeterminant(rot_mat)) < 0 ? 1.0f : -1.0f;
        
        double r_trace = nflip * Vertex.normal.x + Vertex.tangent.y + Vertex.bitangent.z;
        double angle = acos((r_trace - 1.0) / 2.0);
        double axis_norm_factor = 2.0 * sin(angle);

        /*
        *(Buffer++) = (Vertex.bitangent.y - Vertex.tangent.z  ) / axis_norm_factor;
        *(Buffer++) = (Vertex.normal.z    - Vertex.bitangent.x) / axis_norm_factor;
        *(Buffer++) = (Vertex.tangent.x   - Vertex.normal.y   ) / axis_norm_factor;*/

        /*
        *(Buffer++) = (Vertex.tangent.z - Vertex.bitangent.y  ) / axis_norm_factor;
        *(Buffer++) = (Vertex.bitangent.x - Vertex.normal.z) / axis_norm_factor;
        *(Buffer++) = (Vertex.normal.y - Vertex.tangent.x ) / axis_norm_factor;*/

        *(Buffer++) = (nflip * Vertex.normal.y - Vertex.tangent.z) / axis_norm_factor;
        *(Buffer++) = (Vertex.bitangent.z - nflip * Vertex.normal.x) / axis_norm_factor;
        *(Buffer++) = (Vertex.tangent.x - Vertex.bitangent.y) / axis_norm_factor;

        int16_t* BufferSNORM = reinterpret_cast<int16_t*>(Buffer);

        *(BufferSNORM++) = cos(angle) * std::numeric_limits<int16_t>::max();
        *(BufferSNORM++) = sin(angle) * std::numeric_limits<int16_t>::max();

        Buffer = reinterpret_cast<float*>(BufferSNORM);

        *(Buffer++) = Vertex.uv.x;
        *(Buffer++) = Vertex.uv.y;

        return Buffer;
    };
    axis_angle_vtx.vertex_size = 36;

    Mesh obj_standard, obj_axis_angle;
    obj_standard.BuildFromOBJ(&dev, copy_graph, "monkey.obj");
    obj_axis_angle.BuildFromOBJ(&dev, copy_graph, "monkey.obj", std::move(axis_angle_vtx));

    constexpr uint32_t kInstancesCount = 10000;

    // Define pipeline states
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_standard = {};
    pipeline_state_standard.InputLayout = obj_standard.GetDesc().vertex_layout.GetGPUDesc();
    pipeline_state_standard.pRootSignature = root_signature.Get();
    pipeline_state_standard.VS = { reinterpret_cast<UINT8*>(vertex_shader_standard->GetBufferPointer()), vertex_shader_standard->GetBufferSize() };
    pipeline_state_standard.PS = { reinterpret_cast<UINT8*>(pixel_shader->GetBufferPointer()), pixel_shader->GetBufferSize() };
    pipeline_state_standard.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipeline_state_standard.RasterizerState.FrontCounterClockwise = TRUE;
    pipeline_state_standard.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipeline_state_standard.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipeline_state_standard.SampleMask = UINT_MAX;
    pipeline_state_standard.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_state_standard.NumRenderTargets = 1;
    pipeline_state_standard.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipeline_state_standard.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipeline_state_standard.SampleDesc.Count = 1;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_axis_angle = pipeline_state_standard;
    pipeline_state_axis_angle.InputLayout = obj_axis_angle.GetDesc().vertex_layout.GetGPUDesc();
    pipeline_state_axis_angle.VS = { reinterpret_cast<UINT8*>(vertex_shader_axis_angle->GetBufferPointer()), vertex_shader_axis_angle->GetBufferSize() };

    struct InstanceData
    {
        XMFLOAT4X4 World;
        XMFLOAT4X4 WVP;
    };
    StructuredBuffer<InstanceData> instances_data_buffer;

    // Setup transformation data for instances
    auto view_matrix = XMMatrixLookAtRH(XMVectorSet(0, 1, -2, 0), XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 1, 0, 0));
    auto proj_matrix = XMMatrixPerspectiveFovRH(90_deg, window.GetSizeX() / (float)window.GetSizeY(), 0.01, 1000);

    vector<InstanceData> instances_data(kInstancesCount);
    for (int idx = 0; idx < kInstancesCount; idx++)
    {
        auto wvp = XMMatrixScaling(0.75, 0.75, 0.75);
        wvp = XMMatrixMultiply(wvp, XMMatrixRotationRollPitchYaw(0, sin(idx + 0.5), 0));
        wvp = XMMatrixMultiply(wvp, XMMatrixTranslation(cos(idx +  0.75) * 2, sin(idx) * 2.5, idx));

        XMStoreFloat4x4(&instances_data[idx].World, XMMatrixTranspose(wvp));

        wvp = XMMatrixMultiply(wvp, view_matrix);
        wvp = XMMatrixMultiply(wvp, proj_matrix);

        XMStoreFloat4x4(&instances_data[idx].WVP, XMMatrixTranspose(wvp));
    }
    
    instances_data_buffer.Create(&dev, kInstancesCount, false, false, instances_data, &copy_graph);

    // Execute all the copy commands
    copy_graph.Build(&dev);
    copy_graph.Execute(&dev);

    // -------------------------------
    //      Render setup
    // -------------------------------

    // Create the command graph
    CommandGraph commands(kWorkerCount, QueueType::Graphics, &dev);

    // For this demo we can do everything on one node
    commands.AddNode("Clear Draw Present", nullptr, [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        // Clear
        backbuffer.Transition(cl, D3D12_RESOURCE_STATE_RENDER_TARGET);

        cl->ClearRenderTargetView(*backbuffer.GetHandle(), DirectX::Colors::Magenta, 0, nullptr);
        cl->ClearDepthStencilView(*depth_buffer.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // Draw
        D3D12_VIEWPORT viewports[] = { window.GetViewport() };
        D3D12_RECT view_rects[] = { window.GetRect() };
        cl->RSSetViewports(1, viewports);
        cl->RSSetScissorRects(1, view_rects);
        cl->SetGraphicsRootSignature(root_signature.Get());
        cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        D3D12_CPU_DESCRIPTOR_HANDLE rts[] = { *backbuffer.GetHandle() };
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = *depth_buffer.GetDSV();
        cl->OMSetRenderTargets(1, rts, false, &dsv);

        // TODO : Move this to a function on the device that sets all the heaps
        ID3D12DescriptorHeap* desc_vec[] = { dev.GetDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap() };
        cl->SetDescriptorHeaps(1, desc_vec);

        cl->SetGraphicsRootDescriptorTable(0, instances_data_buffer.GetSRV().GetGPUDescriptor());
        obj_axis_angle.Draw(cl, kInstancesCount);

        // Present
        backbuffer.Transition(cl, D3D12_RESOURCE_STATE_PRESENT);
    }, {});

    commands.Build(&dev);

    // -------------------------------
    //      Render loop
    // -------------------------------
    // Make sure all transfers finished
    dev.WaitForQueue(QueueType::Copy);

    // To print some debug metrics
    float execute_cl_time, frame_time;
    thread metrics_printer([&]()
    {
        FrameDX12::TimedLoop([&]()
        {
            system("cls");
            wcout << L"---- Metrics ----" << endl;
            wcout << L"Execute CL : " << to_wstring(execute_cl_time) << endl;
            wcout << L"Frame      : " << to_wstring(frame_time) << endl;
        }, 150ms);
    });
    metrics_printer.detach();

    uint64_t execute_ids[kResourceBufferCount] = {};

    // Enter the render loop
    window.CallDuringIdle([&](double elapsed_time)
    {
        float delta_seconds = elapsed_time / 1000.0f;

        frame_time = elapsed_time;

        // Make sure we are finished with this frame resources before executing
        dev.WaitForWork(QueueType::Graphics, execute_ids[sCurrentResourceBufferIndex]);

        auto start = chrono::high_resolution_clock::now();
        execute_ids[sCurrentResourceBufferIndex] = commands.Execute(&dev, dev.GetPSO(pipeline_state_axis_angle));
        auto end = chrono::high_resolution_clock::now();
        execute_cl_time = chrono::duration_cast<chrono::nanoseconds>((end - start)).count() / 1e6;


        // DEBUG!
        assert(dev.GetSwapChain<3>()->GetCurrentBackBufferIndex() == sCurrentResourceBufferIndex);


        dev.GetSwapChain()->Present(0, 0);

        // Advance buffer index
        sCurrentResourceBufferIndex = (sCurrentResourceBufferIndex + 1) % kResourceBufferCount;

        return false;
    });

    return 0;
}
#endif // 1
