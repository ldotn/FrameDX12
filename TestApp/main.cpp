#include <Windows.h>
#include <crtdbg.h>
#include "../Core/Log.h"
#include "../Core/Window.h"
#include "../Device/Device.h"
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

    std::vector<int> v0{ 10,2,6 };

    VecRef vref(1, v0);
    auto a = *vref;
    std::cout << *vref << std::endl;

    std::cout << v0[2] << std::endl;
    // Create the window
    Window win;

    // Create the device
    Device dev(&win);

    // Enter the render loop
    win.CallDuringIdle([](double ElapsedTime)
    {
        return false;
    });

    return 0;
}