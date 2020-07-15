#pragma once
#include "stdafx.h"

namespace FrameDX12
{
	// Loops and calls f, while trying to keep all iterations of the same duration
	// SHOULD NOT BE ASSUMED TO BE ACCURATE!!!
	// It relies on this_thread::sleep_for, so it's only as accurate as sleep_for is
	// Can be interrupted
	// Allows the use of generic std::function functions, which is a big limitation of Win32 timers
	// A quick test based on the log printer loop of the test app gives the following results
	// System : Windows 10 Pro x64 10.0.17134, Xeon E5-2683 V3, 32 GB RAM, 2x Samsung Evo 850 (250 GB, RAID0)
	//		Period | Error (%)
	//		------------------
	//		 10 ms | 9 - 10 
	//		 25 ms | 3.5 - 4
	//		 50 ms | 1.5 - 2
	//		100 ms | 0.5 - 1
	//		200 ms | 0.4 - 0.5
	//		500 ms | 0.18 - 0.2
	template<typename PeriodT>
	void TimedLoop(std::function<void()> f, PeriodT Period, bool* BreakCondition = nullptr)
	{
		using namespace std;

		// Find the overhead of sleep
		// Assuming a constant overhead, something that IS WRONG
		auto t0 = chrono::high_resolution_clock::now();
		this_thread::sleep_for(1ns);
		auto t1 = chrono::high_resolution_clock::now();
		auto overhead = t1 - t0;

		while ((BreakCondition && *BreakCondition) || !BreakCondition)
		{
			t0 = chrono::high_resolution_clock::now();
			f();
			t1 = chrono::high_resolution_clock::now();

			// Don't sleep if the time is under the overhead of sleep_for
			// Also compensate for overhead here
			auto r = Period - (t1 - t0) - overhead;
			if (r > overhead)
				this_thread::sleep_for(r);
		}
	}

	template<typename T>
	T ceil(T x, T y) { return x / y + (x % y != 0); }

	template<typename T>
	T clamp(T x, T a, T b) { return x < a ? a : (x > b ? b : x); }

	template<typename T>
	T saturate(T x) { return clamp(x, T(0), T(1)); }

#define __unique_string_inner2(str,c) str #c
#define __unique_string_inner(str,c) __unique_string_inner2(str,c)
#define UNIQUE_STRING(base) __unique_string_inner( base, __COUNTER__ )

	// User-defined literal to write degrees to radians
	constexpr long double operator"" _deg(long double x)
	{
		return (x * 3.1415926535897932384626433832795) / 180.0;
	}

	constexpr long double operator"" _deg(unsigned long long int x)
	{
		return (((long double)x) * 3.1415926535897932384626433832795) / 180.0;
	}

#define ONCE_INNER(lambda, n0, n1) static struct once_wrapper_##n0##n1 { once_wrapper_##n0##n1(std::function<void()> f){ f(); }} once_wrapper_var_##n0##n1(lambda);
#define ONCE_EXPAND(lambda, n0, n1) ONCE_INNER(lambda,n0,n1)
#define ONCE(lambda) ONCE_EXPAND(lambda, __COUNTER__, __LINE__ )

	// Wrapper for an index reference to an entry on a std::vector
	// Ideally the optimizer will inline this calls and it should be 0 cost
	template<typename T>
	class VecRef
	{
	public:
		VecRef(size_t index, std::vector<T>& vec) :
			mVec(vec),
			mIndex(index)
		{
		}

		T& operator*()
		{
			return mVec[mIndex];
		}

		T& operator->() 
		{
			return mVec[mIndex];
		}

		const T& operator*() const
		{
			return mVec[mIndex];
		}

		const T& operator->() const
		{
			return mVec[mIndex];
		}
	private:
		std::vector<T>& mVec;
		size_t mIndex;
	};
}