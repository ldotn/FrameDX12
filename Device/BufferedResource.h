#pragma once
#include <cstdint>
#include <atomic>

namespace FrameDX12
{
	template<typename Type>
	class BufferedResource
	{
	public:
		static constexpr size_t sBufferCount = 3;
		static std::atomic<int> sCurrentBuffer;

		T& operator*()
		{
			return mResource[sCurrentBuffer];
		}

		T& operator->()
		{
			return mResource[sCurrentBuffer];
		}

		const T& operator*() const
		{
			return mResource[sCurrentBuffer];
		}

		const T& operator->() const
		{
			return mResource[sCurrentBuffer];
		}
	private:
		Type mResource[kBufferCount];
	};
}