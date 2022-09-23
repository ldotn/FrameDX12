#if 1
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
#include "../Resource/StructuredBuffer.h"
#include "../Resource/Mesh.h"
#include "../Resource/ConstantBuffer.h"
#include <iostream>
#include "pix3.h"
#include <unordered_set>
#include <set>
#include "../Core/ShaderCompiler.h"

import Camera;

using namespace FrameDX12;
using namespace std;
using namespace fpp;
using namespace DirectX;

constexpr int kWorkerCount = 1;

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
    ShaderCompiler::Init();

    FlyCamera camera(window);

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

    // Load mesh
    CommandGraph copy_graph(kWorkerCount, QueueType::Copy, &dev);

    Mesh model;
    //model.BuildFromOBJ(&dev, copy_graph, "sphere.obj");
    //model.BuildFromOBJ(&dev, copy_graph, "monkey_lowpoly.obj");
    model.BuildFromOBJ(&dev, copy_graph, "monkey.obj");

    // Meshlet construction
    // Using up to 128 triangles and vertices per meshlet, because that's the max size for a MS group and is also a multiple of the common wavefront sizes (32/64)
    constexpr uint32 max_meshlet_trigs = 126; // from nvidia
    constexpr uint32 max_meshlet_vertices = 64; // from nvidia
    struct Meshlet
    {
        // The index for the first vertex of the meshlet
        uint32 start_vertex;
        // The number of triangles on the meshlet
        uint32 triangles_num;
        // Number of vertices for the meshlet
        uint32 vertices_num;
        // The index for the first triangle of the meshlet
        uint32 start_triangle;

        // TODO : Other static data like bounds and such
    };

    // The three indices that compose a triangle, relative to the start vertex of the meshlet
    struct Triangle
    {
        uint32 index_c : 10;
        uint32 index_b : 10;
        uint32 index_a  : 10;
        uint32 padding_ : 2;
    };

    // Create strips of adjacent triangles  (share an edge)
    const auto& indices = model.GetIndices();
    const auto& vertices = model.GetVertices();

    vector<uint32> unused_triangles(model.GetDesc().triangle_count);
    generate(unused_triangles.begin(), unused_triangles.end(), [n = 0] () mutable { return n++; });

    vector<Meshlet> meshlets;
    vector<Triangle> triangles;
    vector<uint32> vertices_ib;

    uint32 current_trig = 0;
    while (!unused_triangles.empty())
    {
        Meshlet out_meshlet;
        out_meshlet.triangles_num = 0;
        out_meshlet.vertices_num = 0;
        out_meshlet.start_vertex = vertices_ib.size();
        out_meshlet.start_triangle = triangles.size();

        uint32 meshlet_trigs[max_meshlet_trigs];
        set<uint32> meshlet_vertices; // using set, not unordered_set, so the first value is the smallest

        // Remove the triangle from the list
        swap(unused_triangles.front(), unused_triangles.back());
        unused_triangles.resize(unused_triangles.size() - 1);

        bool keep_current = false;

        while (out_meshlet.triangles_num < max_meshlet_trigs && meshlet_vertices.size() < max_meshlet_vertices)
        {
            uint32 vid_0 = indices[3 * current_trig];
            uint32 vid_1 = indices[3 * current_trig + 1];
            uint32 vid_2 = indices[3 * current_trig + 2];

            uint32 new_vcount = meshlet_vertices.size();
            if (!meshlet_vertices.contains(vid_0)) ++new_vcount;
            if (!meshlet_vertices.contains(vid_1)) ++new_vcount;
            if (!meshlet_vertices.contains(vid_2)) ++new_vcount;

            if (new_vcount > max_meshlet_vertices)
            {
                // Need to re-add to the unused trig list the one that was preemptively removed
                // This will be the starting triangle for the next meshlet
                unused_triangles.push_back(current_trig);
                keep_current = true;
                break;
            }

            meshlet_vertices.insert(vid_0);
            meshlet_vertices.insert(vid_1);
            meshlet_vertices.insert(vid_2);

            meshlet_trigs[out_meshlet.triangles_num] = current_trig;
            ++out_meshlet.triangles_num;

            // Find a triangle that shares an edge with any of the selected triangles
            bool found_adj = false;
            for (uint32& adj_trig : unused_triangles)
            {
                // If it shares two vertices, then it will share an edge
                uint32 adj_trig_a = indices[3 * adj_trig];
                uint32 adj_trig_b = indices[3 * adj_trig + 1];
                uint32 adj_trig_c = indices[3 * adj_trig + 2];

                bool share_a = meshlet_vertices.contains(adj_trig_a);
                bool share_b = meshlet_vertices.contains(adj_trig_b);
                bool share_c = meshlet_vertices.contains(adj_trig_c);

                if ((share_a && (share_b || share_c)) ||
                    (share_b && (share_a || share_c)) ||
                    (share_c && (share_a || share_b)))
                {
                    // Found an adjacent triangle, update the next trig
                    current_trig = adj_trig;
                    found_adj = true;

                    // Remove the triangle from the list
                    swap(adj_trig, unused_triangles.back()); // swap works because adj_trig is a ref
                    unused_triangles.resize(unused_triangles.size() - 1);

                    break;
                }
            }

            if (!found_adj) break;
        }

        if (!keep_current && unused_triangles.size() > 0) current_trig = unused_triangles.front();

        // Write the vertices to the ib, and then write the triangles to the trig buffer, with the relative indexes
        unordered_map<uint32, uint32> local_vid_remap;

        out_meshlet.vertices_num = meshlet_vertices.size();
        for (uint32 index : meshlet_vertices)
        {
            local_vid_remap[index] = vertices_ib.size() - out_meshlet.start_vertex;
            vertices_ib.push_back(index);
        }

        for (uint32 idx = 0; idx < out_meshlet.triangles_num; ++idx)
        {
            Triangle trig;
            trig.index_a = local_vid_remap[indices[3 * meshlet_trigs[idx]]];
            trig.index_b = local_vid_remap[indices[3 * meshlet_trigs[idx] + 1]];
            trig.index_c = local_vid_remap[indices[3 * meshlet_trigs[idx] + 2]];

            triangles.push_back(trig);
        }

        meshlets.push_back(out_meshlet);
        std::wcout << out_meshlet.triangles_num << L"/" << out_meshlet.vertices_num << L" ";
    }

    //go over all the meshlets not totally full and check if you can merge into them triangles from other meshlets

#if 0

    vector<set<uint32>> adjacency_strips(1);
    uint32 current_adj_trig = 0;
    while(!unused_triangles.empty())
    {
        adjacency_strips.back().insert(current_adj_trig);

        // Try to find an unused triangle that's adjacent to any triangle on the strip
        // I don't care about it being a sequence of adj trigs, I want a chunk
        bool found_adj = false;
        for (uint32 test_trig : adjacency_strips.back())
        {
            uint32 trig_a = indices[3 * test_trig];
            uint32 trig_b = indices[3 * test_trig + 1];
            uint32 trig_c = indices[3 * test_trig + 2];

            for (uint32& adj_trig_idx : unused_triangles)
            {
                if (adj_trig_idx != test_trig)
                {
                    // Check if the triangle shares and edge with the one being processed
                    // If it shares two vertices, then it will share an edge
                    uint32 adj_trig_a = indices[3 * adj_trig_idx];
                    uint32 adj_trig_b = indices[3 * adj_trig_idx + 1];
                    uint32 adj_trig_c = indices[3 * adj_trig_idx + 2];

                    bool share_a = ((trig_a == adj_trig_a) || (trig_a == adj_trig_b) || (trig_a == adj_trig_c));
                    bool share_b = ((trig_b == adj_trig_a) || (trig_b == adj_trig_b) || (trig_b == adj_trig_c));
                    bool share_c = ((trig_c == adj_trig_a) || (trig_c == adj_trig_b) || (trig_c == adj_trig_c));

                    if ((share_a && (share_b || share_c)) ||
                        (share_b && (share_a || share_c)) ||
                        (share_c && (share_a || share_b)))
                    {
                        // Found an adjacent triangle, update the next trig
                        current_adj_trig = adj_trig_idx;
                        found_adj = true;

                        // Remove the triangle from the list
                        swap(adj_trig_idx, unused_triangles.back()); // swap works because adj_trig_idx is a ref
                        unused_triangles.resize(unused_triangles.size() - 1);

                        break;
                    }
                }
            }

           if (found_adj) break;
        }

        // If none was found, grab the next unused and reset the strip
        //  also if the strip has more than max_meshlet_size triangles, just split it because I won't fit a bigger meshlet there anyway
        if (!found_adj || adjacency_strips.back().size() == max_meshlet_trigs)
        {
            std::wcout << adjacency_strips.back().size() << L" ";

            current_adj_trig = unused_triangles.front();
            swap(unused_triangles.front(), unused_triangles.back());
            unused_triangles.resize(unused_triangles.size() - 1);
            adjacency_strips.push_back({});
        }
    }

    std::wcout << endl;

    // Walk the adjacency strips and construct groups of up to max_meshlet_size triangles with up to max_meshlet_size vertices
    vector<Meshlet> meshlets;
    vector<Triangle> triangles;
    vector<uint32> vertices_ib;

    Meshlet out_meshlet;
    out_meshlet.triangles_num = 0;
    out_meshlet.vertices_num = 0;
    out_meshlet.start_vertex = 0;
    out_meshlet.start_triangle = 0;

    for (set<uint32>& adj_strip : adjacency_strips)
    {
        while (!adj_strip.empty())
        {
            set<uint32> meshlet_vertices; // using set, not unordered_set, so the first value is the smallest

            Meshlet out_meshlet;
            out_meshlet.triangles_num = 0;
            out_meshlet.vertices_num = 0;
            out_meshlet.start_vertex = vertices_ib.size();
            out_meshlet.start_triangle = triangles.size();

            auto trig_iter = adj_strip.begin();
            uint32 meshlet_trigs[max_meshlet_trigs];

            auto trig_delete_start = trig_iter;
            vector<pair<decltype(trig_iter), decltype(trig_iter)>> trigs_to_delete;
            do
            {
                uint32 current_trig = *trig_iter;
                uint32 vid_0 = indices[3 * current_trig    ];
                uint32 vid_1 = indices[3 * current_trig + 1];
                uint32 vid_2 = indices[3 * current_trig + 2];

                // If this triangle would cause the vertices list to grow over max_meshlet_size vertices reset the meshlet (otherwise it wont fit on a warp)
                uint32 new_vcount = meshlet_vertices.size();
                if (!meshlet_vertices.contains(vid_0)) ++new_vcount;
                if (!meshlet_vertices.contains(vid_1)) ++new_vcount;
                if (!meshlet_vertices.contains(vid_2)) ++new_vcount;

                if (new_vcount > max_meshlet_vertices)
                {
                    // Continue in case any other triangle on the strip reuses more vertices and could be aded
                    trigs_to_delete.push_back({ trig_delete_start, trig_iter });
                    trig_delete_start = trig_iter;
                    ++trig_iter;
                    continue;
                }

                meshlet_vertices.insert(vid_0);
                meshlet_vertices.insert(vid_1);
                meshlet_vertices.insert(vid_2);

                meshlet_trigs[out_meshlet.triangles_num] = current_trig;

                ++out_meshlet.triangles_num;
                ++trig_iter;
            } while (trig_iter != adj_strip.end() && out_meshlet.triangles_num < max_meshlet_trigs);

            // Erase all used triangles from the strip
            for(auto& [a,b] : trigs_to_delete)
                adj_strip.erase(a, b);
            adj_strip.erase(trig_delete_start, adj_strip.end());

            // Write the vertices to the ib, and then write the triangles to the trig buffer, with the relative indexes
            unordered_map<uint32, uint32> local_vid_remap;

            out_meshlet.vertices_num = meshlet_vertices.size();
            for (uint32 index : meshlet_vertices)
            {
                local_vid_remap[index] = vertices_ib.size() - out_meshlet.start_vertex;
                vertices_ib.push_back(index);
            }

            for (uint32 idx = 0; idx < out_meshlet.triangles_num; ++idx)
            {
                Triangle trig;
                trig.index_a = local_vid_remap[indices[3 * meshlet_trigs[idx]]];
                trig.index_b = local_vid_remap[indices[3 * meshlet_trigs[idx] + 1]];
                trig.index_c = local_vid_remap[indices[3 * meshlet_trigs[idx] + 2]];

                triangles.push_back(trig);
            }

            meshlets.push_back(out_meshlet);
            std::wcout << out_meshlet.triangles_num << L"/" << out_meshlet.vertices_num << L" ";
        }
    }
#endif
    StructuredBuffer<CPUVertex> vertex_buffer;
    StructuredBuffer<Meshlet> meshlet_buffer;
    CommitedResource triangles_buffer;
    CommitedResource vertices_index_buffer;

    vertex_buffer.Create(&dev, vertices.size(), false, false, vertices, &copy_graph);
    meshlet_buffer.Create(&dev, meshlets.size(), false, false, meshlets, &copy_graph);
    //triangles_buffer.Create(&dev, CD3DX12_RESOURCE_DESC::Tex1D(DXGI_FORMAT_R10G10B10A2_UINT, triangles.size()), D3D12_RESOURCE_STATE_COMMON);
    //vertices_index_buffer.Create(&dev, CD3DX12_RESOURCE_DESC::Tex1D(DXGI_FORMAT_R32_UINT, vertices_ib.size()), D3D12_RESOURCE_STATE_COMMON);
    // Using raw buffers as textures has a 16K max size limit which is a big problem
    triangles_buffer.Create(&dev, CD3DX12_RESOURCE_DESC::Buffer(triangles.size()*4), D3D12_RESOURCE_STATE_COMMON);
    vertices_index_buffer.Create(&dev, CD3DX12_RESOURCE_DESC::Buffer(vertices_ib.size()*4), D3D12_RESOURCE_STATE_COMMON);
    
    copy_graph.AddNode("Fill Meshlet Buffers", [&](ID3D12GraphicsCommandList* cl)
    {
        // Need to set it to common when using the copy queue
        triangles_buffer.FillFromBuffer(cl, triangles, D3D12_RESOURCE_STATE_COMMON);
        vertices_index_buffer.FillFromBuffer(cl, vertices_ib, D3D12_RESOURCE_STATE_COMMON);
    }, nullptr, {});

    // Create views
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        srv_desc.Buffer.NumElements = triangles.size();
        triangles_buffer.CreateSRV(&srv_desc);

        srv_desc.Buffer.NumElements = vertices_ib.size();
        vertices_index_buffer.CreateSRV(&srv_desc);
    }

    copy_graph.Build(&dev);
    copy_graph.Execute(&dev);

    // Define pipeline state 

    //  Create root signature
    ComPtr<ID3D12RootSignature> root_signature;
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[5]; // Perfomance TIP: Order from most frequent to least frequent.
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 4);		// 1 frequently changed constant buffer.
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);		
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);		
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);		
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);		

        CD3DX12_ROOT_PARAMETER rootParameters[5];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_MESH);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_MESH);
        rootParameters[3].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_MESH);
        rootParameters[4].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_MESH);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(dev.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
    }

    // Load shaders
    CompiledShader mesh_shader = ShaderCompiler::CompileShader(L"MeshletRendering.hlsl", L"ms_6_5", L"MSMain");
    CompiledShader pixel_shader = ShaderCompiler::CompileShader(L"MeshletRendering.hlsl", L"ps_6_5", L"PSMain");

    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC pipeline_state = {};
    pipeline_state.pRootSignature = root_signature.Get();
    pipeline_state.MS = { reinterpret_cast<UINT8*>(mesh_shader.shader->GetBufferPointer()), mesh_shader.shader->GetBufferSize() };
    pipeline_state.PS = { reinterpret_cast<UINT8*>(pixel_shader.shader->GetBufferPointer()), pixel_shader.shader->GetBufferSize() };
    pipeline_state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipeline_state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipeline_state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipeline_state.SampleMask = UINT_MAX;
    pipeline_state.NumRenderTargets = 1;
    pipeline_state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipeline_state.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipeline_state.SampleDesc = DefaultSampleDesc();

    auto pso_stream = CD3DX12_PIPELINE_MESH_STATE_STREAM(pipeline_state);

    D3D12_PIPELINE_STATE_STREAM_DESC pso_desc;
    pso_desc.pPipelineStateSubobjectStream = &pso_stream;
    pso_desc.SizeInBytes = sizeof(pso_stream);

    // Create CB
    struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) CBData
    {
        XMFLOAT4X4 World;
        XMFLOAT4X4 WVP;
    };
    ConstantBuffer<CBData> cb;
    uint32 max_draw_count = 6000;
    cb.Create(&dev, max_draw_count);

    // -------------------------------
    //      Render setup
    // -------------------------------

    // Create the command graph
    CommandGraph commands(kWorkerCount, QueueType::Graphics, &dev);

    commands.AddNode("Clear", [&](ID3D12GraphicsCommandList* cl)
    {
        backbuffer.Transition(cl, D3D12_RESOURCE_STATE_RENDER_TARGET);
        vertex_buffer.Transition(cl, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        meshlet_buffer.Transition(cl, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        triangles_buffer.Transition(cl, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        vertices_index_buffer.Transition(cl, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        cl->ClearRenderTargetView(*backbuffer.GetHandle(), DirectX::Colors::Magenta, 0, nullptr);
        cl->ClearDepthStencilView(*depth_buffer.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    }, nullptr, {});

    commands.AddNode("Draw", [&](ID3D12GraphicsCommandList* cl)
    {
        /*backbuffer.Transition(cl, D3D12_RESOURCE_STATE_RENDER_TARGET);
        vertex_buffer.Transition(cl, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        meshlet_buffer.Transition(cl, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        triangles_buffer.Transition(cl, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        vertices_index_buffer.Transition(cl, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        cl->ClearRenderTargetView(*backbuffer.GetHandle(), DirectX::Colors::Magenta, 0, nullptr);
        cl->ClearDepthStencilView(*depth_buffer.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        */
        D3D12_VIEWPORT viewports[] = { window.GetViewport() };
        D3D12_RECT view_rects[] = { window.GetRect() };
        cl->RSSetViewports(1, viewports);
        cl->RSSetScissorRects(1, view_rects);
        cl->SetGraphicsRootSignature(root_signature.Get());

        D3D12_CPU_DESCRIPTOR_HANDLE rts[] = { *backbuffer.GetHandle() };
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = *depth_buffer.GetDSV();
        cl->OMSetRenderTargets(1, rts, false, &dsv);

        // TODO : Move this to a function on the device that sets all the heaps
        ID3D12DescriptorHeap* desc_vec[] = { dev.GetDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap() };
        cl->SetDescriptorHeaps(1, desc_vec);
    },
    [&](ID3D12GraphicsCommandList* cl, uint32_t idx)
    {
        cl->SetGraphicsRootDescriptorTable(0, cb.GetView(idx).GetGPUDescriptor());
        cl->SetGraphicsRootDescriptorTable(1, vertex_buffer.GetSRV().GetGPUDescriptor());
        cl->SetGraphicsRootDescriptorTable(2, meshlet_buffer.GetSRV().GetGPUDescriptor());
        cl->SetGraphicsRootDescriptorTable(3, triangles_buffer.GetSRV().GetGPUDescriptor());
        cl->SetGraphicsRootDescriptorTable(4, vertices_index_buffer.GetSRV().GetGPUDescriptor());

        uint32 meshlet_count = meshlets.size();
        ((ID3D12GraphicsCommandList6*)cl)->DispatchMesh(meshlet_count, 1, 1);
    
        backbuffer.Transition(cl, D3D12_RESOURCE_STATE_PRESENT);
    }, {"Clear"}, 1);

    commands.AddNode("Present", [&](ID3D12GraphicsCommandList* cl)
    {
        //backbuffer.Transition(cl, D3D12_RESOURCE_STATE_PRESENT);
    }, nullptr, { "Draw" });

    commands.Build(&dev);

    // Create projection matrices
    auto proj_matrix = XMMatrixPerspectiveFovRH(80_deg, window.GetSizeX() / (float)window.GetSizeY(), 0.01, 1000);

    // -------------------------------
    //      Render loop
    // -------------------------------
    // Make sure all transfers finished
    dev.WaitForQueue(QueueType::Copy);

    // To print some debug metrics
    float execute_cl_time, frame_time;
    /*thread metrics_printer([&]()
    {
        FrameDX12::TimedLoop([&]()
        {
            system("cls");
            wcout << L"---- Metrics ----" << endl;
            wcout << L"Execute CL : " << to_wstring(execute_cl_time) << endl;
            wcout << L"Frame      : " << to_wstring(frame_time) << endl;
        }, 150ms);
    });
    metrics_printer.detach();*/

    uint64_t execute_ids[kResourceBufferCount] = {};

    // Enter the render loop
    window.CallDuringIdle([&](double elapsed_time_ms)
    {
        float delta_seconds = elapsed_time_ms / 1000.0f;

        camera.Tick(delta_seconds);

        static float game_seconds = 0;
        game_seconds += delta_seconds;

        int draw_count = 1;//max(1u, min(uint32(1000*game_seconds), max_draw_count));
        *(commands["Draw"].repeats) = draw_count;

        for (int idx = 0; idx < draw_count; idx++)
        {
            CBData data;
            //auto wvp = XMMatrixScaling(0.75, 0.75, 0.75);
            auto wvp = XMMatrixScaling(1, 1, 1);
            //wvp = XMMatrixMultiply(wvp, XMMatrixRotationRollPitchYaw(0, sin(idx + game_seconds * 0.5) + XM_PIDIV2, 0));
            //wvp = XMMatrixMultiply(wvp, XMMatrixTranslation(cos(idx + game_seconds * 0.75) * 5, sin(idx + game_seconds * 0.6) * 5.5, -idx - 5));

            XMStoreFloat4x4(&data.World, XMMatrixTranspose(wvp));

            wvp = XMMatrixMultiply(wvp, camera.GetViewMatrix());
            wvp = XMMatrixMultiply(wvp, proj_matrix);

            XMStoreFloat4x4(&data.WVP, XMMatrixTranspose(wvp));

            cb.Update(data, idx);
        }

        frame_time = elapsed_time_ms;
        
        // Make sure we are finished with this frame resources before executing
        dev.WaitForWork(QueueType::Graphics, execute_ids[sCurrentResourceBufferIndex]);

        auto start = chrono::high_resolution_clock::now();
        execute_ids[sCurrentResourceBufferIndex] = commands.Execute(&dev, dev.GetPSO(pso_desc));

        auto end = chrono::high_resolution_clock::now();
        execute_cl_time = chrono::duration_cast<chrono::nanoseconds>((end - start)).count() / 1e6;

        dev.GetSwapChain()->Present(0, 0);

        // Update the frame index
        sCurrentResourceBufferIndex = dev.GetSwapChain<3>()->GetCurrentBackBufferIndex();

        return false;
    });

    return 0;
}

#endif // 1
