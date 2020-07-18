#pragma once
#include "stdafx.h"

namespace FrameDX12
{
	extern std::atomic<int> sCurrentResourceBufferIndex;
	static constexpr uint8_t kResourceBufferCount = 3;

	template<typename Type>
	class BufferedResource
	{
	public:


		BufferedResource() = default;
		BufferedResource(const BufferedResource&) = default;
		BufferedResource(BufferedResource&&) = default;
		BufferedResource(std::function<Type(uint8_t)> constructor)
		{
			for (uint8_t i = 0; i < kResourceBufferCount; i++) mResource[i] = constructor(i);
		}

		Type& operator*()
		{
			return mResource[sCurrentResourceBufferIndex % kResourceBufferCount];
		}

		Type& operator->()
		{
			return mResource[sCurrentResourceBufferIndex % kResourceBufferCount];
		}

		const Type& operator*() const
		{
			return mResource[sCurrentResourceBufferIndex % kResourceBufferCount];
		}

		const Type& operator->() const
		{
			return mResource[sCurrentResourceBufferIndex % kResourceBufferCount];
		}
	private:
		Type mResource[kResourceBufferCount];
	};
}