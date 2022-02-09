#pragma once
#include "stdafx.h"

namespace FrameDX12
{
	enum class KeyAction { Up, Down };


	class Window
	{
	public:
		Window(std::wstring name = L"FrameDX12 App", std::uint32_t size_x = 1600, std::uint32_t size_y = 900, bool fullscreen = false);

		// TODO : Delete everything on destruction

		// There can only be ONE keyboard callback function on the entire program, that's why it's static
		static std::function<void(WPARAM, KeyAction)> KeyboardCallback;
		static std::function<void(WPARAM, int, int)> MouseCallback;
		// Called first on the MessageProc routine, for extra customization. If it returns true the input is considered handled and the message proc returns
		static std::function<bool(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)> CustomMessageProc;

		// Calls the provided function when there are no messages to process
		// This is where you would do all your render stuff
		// It passes the elapsed time (in ms) to the function, and if the function returns true it exits
		void CallDuringIdle(std::function<bool(double)> loop_body);

		uint32_t GetSizeX() const { return mSizeX; }
		uint32_t GetSizeY() const { return mSizeY; }
		HWND GetHandle() const { return mWindowHandle; }
		bool IsFullscreen() const { return mFullscreen; }
		D3D12_VIEWPORT GetViewport(float max_depth = 1.0f) const;
		D3D12_RECT GetRect() const;
	private:
		static LRESULT WINAPI InternalMessageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		HWND mWindowHandle;
		std::chrono::time_point<std::chrono::high_resolution_clock> mLastLoopTime;
		uint32_t mSizeX, mSizeY;
		bool mFullscreen;
	};
}