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

using namespace FrameDX12;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // Init the log
    Log.CreateConsole();
    auto print_thread = Log.FirePrintThread();

    // Create the window
    Window window;

    // Create the device
    Device dev(&window);

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

    // Create the command graph
    CommandGraph commands(1, QueueType::Graphics, &dev);

    commands.AddNode("Clear", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        backbuffer.Transition(cl, D3D12_RESOURCE_STATE_RENDER_TARGET);

        const float clearColor[] = { 1.0f, 0.0f, 1.0f, 1.0f };
        cl->ClearRenderTargetView(*backbuffer.GetHandle(), clearColor, 0, nullptr);
        cl->ClearDepthStencilView(*depth_buffer.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }, {});

    commands.AddNode("Present", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        backbuffer.Transition(cl, D3D12_RESOURCE_STATE_PRESENT);
    }, {"Clear"});

    commands.Build(&dev);

    // Enter the render loop
    window.CallDuringIdle([&](double elapsed_time)
    {
        commands.Execute(&dev);
        dev.GetSwapChain()->Present(0, 0);

        return false;
    });

    return 0;
}