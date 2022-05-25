module;
#include <DirectXMath.h>
#include "Core/Window.h"

export module Camera;

using namespace DirectX;

namespace FrameDX12
{
	export class FlyCamera
	{
	public:
		FlyCamera(const Window& window)
		{
			mPosition = mDirection = XMVectorZero();
			mYaw = mPitch = 0;

			mWindowHandle = window.GetHandle();
			mUp = XMVectorSet(0, 1, 0, 0);
			mSpeed = 0.5;
			mMouseSensitivity = 0.1;
			mInvertVertical = true;
		}

		// TODO : Use the keyboard / mouse callbacks
		void Tick(float DeltaT)
		{
			XMVECTOR velocity = XMVectorZero();

			bool any_key_down = false;
			if (GetKeyState('W') & 0x8000)
			{
				velocity = XMVectorSubtract(velocity, mDirection);
				any_key_down = true;
			}
			if (GetKeyState('S') & 0x8000)
			{
				velocity = XMVectorAdd(velocity, mDirection);
				any_key_down = true;
			}
			if (GetKeyState('A') & 0x8000)
			{
				velocity = XMVectorSubtract(velocity, XMVector3Cross(mUp, mDirection));
				any_key_down = true;
			}
			if (GetKeyState('D') & 0x8000)
			{
				velocity = XMVectorAdd(velocity, XMVector3Cross(mUp, mDirection));
				any_key_down = true;
			}
			
			if (any_key_down)
			{
				velocity = XMVector3NormalizeEst(velocity);

				float speed = mSpeed;
				if (GetKeyState(VK_SHIFT) & 0x8000)
				{
					speed *= 10.0f;
				}

				mPosition = XMVectorAdd(mPosition, XMVectorScale(velocity, speed));
			}

			// Not caching the rect because it changes when you move the window
			RECT screen_rect;
			GetClientRect(mWindowHandle, &screen_rect);

			POINT mouse_pos;
			GetCursorPos(&mouse_pos);

			mYaw += ((float)(mouse_pos.x - screen_rect.right / 2)) * DeltaT * mMouseSensitivity;
			mPitch += ((float)(mouse_pos.y - screen_rect.bottom / 2)) * DeltaT * mMouseSensitivity;
			
			mDirection = XMVector3Transform(XMVectorSet(0,0,1,0), 
				XMMatrixRotationRollPitchYaw(mInvertVertical ? -mPitch : mPitch, -mYaw, 0));

			SetCursorPos(screen_rect.right / 2, screen_rect.bottom / 2);
		}

		// TODO : Add interpolation for when the renderer goes faster than the game thread
		XMMATRIX GetViewMatrix()
		{
			return XMMatrixLookAtLH(mPosition, XMVectorAdd(mPosition, mDirection), mUp);
		}

		float mSpeed, mMouseSensitivity;
		bool mInvertVertical;
	private:
		XMVECTOR mPosition, mUp, mDirection;

		float mYaw, mPitch;

		HWND mWindowHandle;
	};
}
