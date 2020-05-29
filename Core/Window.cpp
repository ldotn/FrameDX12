#include "Window.h"
#include "Error.h"
#include "Log.h"

using namespace FrameDX12;

std::function<void(WPARAM, KeyAction)> Window::KeyboardCallback = [](WPARAM, KeyAction) {};
std::function<void(WPARAM, int, int)> Window::MouseCallback = [](WPARAM, int, int) {};

void FrameDX12::Window::CallDuringIdle(std::function<bool(double)> LoopBody)
{
	using namespace std;

	MSG msg;
	msg.message = WM_NULL;
	mLastLoopTime = chrono::high_resolution_clock::now();

	while (msg.message != WM_QUIT)
	{
		// Use PeekMessage() render on idle time
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			// Translate and dispatch the message
			if (!TranslateAccelerator(mWindowHandle, NULL, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			// Measure time between now and the last call
			auto time = chrono::duration_cast<chrono::nanoseconds>(chrono::high_resolution_clock::now() - mLastLoopTime);
			mLastLoopTime = chrono::high_resolution_clock::now();
			if (LoopBody(time.count()))
				return;
		}
	}
}

LRESULT WINAPI Window::InternalMessageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
		Window::KeyboardCallback(wParam, KeyAction::Down);
		break;
	case WM_KEYUP:
		Window::KeyboardCallback(wParam, KeyAction::Up);
		break;
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
		auto mouse_pos = MAKEPOINTS(lParam);
		Window::MouseCallback(wParam, mouse_pos.x, mouse_pos.y);
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

Window::Window(std::wstring Name, std::uint32_t SizeX, std::uint32_t SizeY, bool bFullscreen)
{
	// If no resolution was specified, get it from screen
	if (SizeX == 0 || SizeY == 0)
	{
		RECT desktop;

		const HWND hDesktop = GetDesktopWindow();

		ThrowIfFalse(GetWindowRect(hDesktop, &desktop));

		SizeX = desktop.right;
		SizeY = desktop.bottom;
	}

	// Create window
	// Register the window class
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, InternalMessageProc, 0L, 0L,
							  GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
							  Name.c_str(), NULL };

	ThrowIfFalse(RegisterClassEx(&wc));

	int x_border = GetSystemMetrics(SM_CXSIZEFRAME);
	int y_menu = GetSystemMetrics(SM_CYMENU);
	int y_border = GetSystemMetrics(SM_CYSIZEFRAME);

	// Create the application's window
	// This functions doesn't provide failure info with GetLastErro
	mWindowHandle = CreateWindow(wc.lpszClassName, Name.c_str(),
		bFullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW, 0, 0,
		SizeX + 2 * x_border,
		SizeY + 2 * y_border + y_menu,
		NULL, NULL, wc.hInstance, NULL);

	if (!mWindowHandle) throw WinError(LAST_ERROR);

	// Show the window
	ShowWindow(mWindowHandle, SW_SHOWDEFAULT);
	ThrowIfFalse(UpdateWindow(mWindowHandle));
}
