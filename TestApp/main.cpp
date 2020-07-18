#define WIN32_LEAN_AND_MEAN // Exclude rarely used stuff from Windows headers
#define NOMINMAX
#include <Windows.h>
#include <crtdbg.h>
#include "../Core/Log.h"
#include "../Core/Window.h"
#include "../Device/Device.h"
#include "../Device/CommandGraph.h"
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

    // Create command graph
    CommandGraph commands(8, QueueType::Graphics, &dev);

    commands.AddNode("Present", [&](ID3D12GraphicsCommandList* cl, uint32_t)
    {
        dev.GetSwapChain()->Present(0, 0);
    }, {});

    commands.Build(&dev);

    // Enter the render loop
    win.CallDuringIdle([&](double ElapsedTime)
    {
        commands.Execute(&dev);

        return false;
    });

    return 0;
}