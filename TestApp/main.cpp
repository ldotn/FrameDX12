#define WIN32_LEAN_AND_MEAN // Exclude rarely used stuff from Windows headers
#define NOMINMAX
#include <Windows.h>
#include <crtdbg.h>
#include "../Core/Log.h"
#include "../Core/Window.h"
#include "../Device/Device.h"
#include "../Device/CommandGraph.h"
#include "../Resources/RenderTarget.h"
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
    Window win;

    // Create the device
    Device dev(&win);

    // Createt the resources
    RenderTarget backbuffer;
    backbuffer.CreateFromSwapchain(&dev);

    // Create the command graph
    CommandGraph commands(1, QueueType::Graphics, &dev);

    commands.AddNode("Clear", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        const float clearColor[] = { 1.0f, 0.0f, 1.0f, 1.0f };
        cl->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((*backbuffer).Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        cl->ClearRenderTargetView(*backbuffer.GetHandle(), clearColor, 0, nullptr);
    }, {});

    commands.AddNode("Present", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        cl->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((*backbuffer).Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    }, {"Clear"});

    commands.Build(&dev);

    // Enter the render loop
    win.CallDuringIdle([&](double ElapsedTime)
    {
        commands.Execute(&dev);
        dev.GetSwapChain()->Present(0, 0);

        return false;
    });

    return 0;
}