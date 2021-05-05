#pragma once
#include "stdafx.h"
#include <concurrent_vector.h>
#include "Utils.h"
#include "Error.h"

namespace FrameDX12
{
	enum class LogCategory
	{
		Info,
		Warning,
		Error,
		CriticalError,
		LogCategoryCount_
	};

#define __MAKE_WIDE(x) L##x
#define MAKE_WIDE(x) __MAKE_WIDE(x) // Double macro to make it expand x if x is a macro

	// Stores a new entry to the log (thread safe)
#define LogMsg(msg,cat) FrameDX12::Log.record_(msg,cat,__LINE__,MAKE_WIDE(__FUNCTION__),MAKE_WIDE(__FILE__))
	// Checks a condition and if false it stores to the log and executes the "failed" line (thread safe)
#define LogAssertEx(cond,cat,failed) if(!(cond)){ FrameDX12::Log.record_(std::wstring(#cond L" != true (Last Win32 error : ") + FrameDX12::StatusCodeToString(LAST_ERROR) + std::wstring(L")"),cat,__LINE__,MAKE_WIDE(__FUNCTION__),MAKE_WIDE(__FILE__)); failed; }
	// Checks an HRESULT/StatusCode and if it's not S_OK it stores to the log and executes the "failed" line (thread safe)
#define LogCheckEx(cond,cat,failed) {auto scode = (FrameDX12::StatusCode)(cond); if(scode != FrameDX12::StatusCode::Ok){ FrameDX12::Log.record_(std::wstring(#cond L" failed with code ") + FrameDX12::StatusCodeToString(scode),cat,__LINE__,MAKE_WIDE(__FUNCTION__),MAKE_WIDE(__FILE__)); failed;} }
	// Checks a condition and stores to the log if false (thread safe)
#define LogAssert(cond,cat) LogAssertEx(cond, cat, ;)
	// Checks an HRESULT/StatusCode and if it's not S_OK it stores to the log (thread safe)
#define LogCheck(cond,cat) LogCheckEx(cond, cat, ;)

/* Checks an HRESULT/StatusCode and if it's not S_OK it stores to the log. It returns the HRESULT converted to StatusCode. (thread safe)
	 Can be used inside an if */
#define LogAssertAndContinue(cond,cat) [&](){ bool b = (cond); if(!b) FrameDX12::Log.record_(std::wstring(#cond L" != true (Last Win32 error : ") + FrameDX12::StatusCodeToString(LAST_ERROR) + std::wstring(L")"),cat,__LINE__,MAKE_WIDE(__FUNCTION__),MAKE_WIDE(__FILE__)); return b; }() 

#define LogCheckAndContinue(cond,cat) [&](){auto scode = ( FrameDX12::StatusCode)(cond); if(scode !=  FrameDX12::StatusCode::Ok) { FrameDX12::Log.record_(std::wstring(#cond L" failed with code ") + FrameDX12::StatusCodeToString(scode) ,cat,__LINE__,MAKE_WIDE(__FUNCTION__),MAKE_WIDE(__FILE__));} return scode; }()
	
#define ThrowIfFailed(cond) LogCheckEx(cond, LogCategory::Error, throw WinError(scode) )
#define ThrowIfFalse(cond) LogAssertEx(cond, LogCategory::Error, throw WinError(LAST_ERROR))

	extern class log_
	{
	public:
		struct Entry
		{
			LogCategory Category;
			std::wstring Message;

			std::chrono::system_clock::time_point Timestamp;
			int Line;
			std::wstring Function;
			std::wstring File;

			friend std::wostream& operator<<(std::wostream& OutputStream, const Entry& Obj);
		};

		// Need to define the functions outside of the header otherwise you get a duplicated definition on the concurrent_vector for some reason
		void record_(const std::wstring& Message,LogCategory Category,int Line,const std::wstring& Function,const std::wstring& File);

		// Prints the entire log to the supplied stream
		// The stream can be a file, wcout, or any other wostream
		// Returns the number of printed items
		size_t PrintAll(std::wostream& OutputStream);

		// Equal to PrintAll, but only prints a range of logs instead of all of them
		// If End = -1 it prints all the logs from Start
		// Returns the number of printed items
		size_t PrintRange(std::wostream& OutputStream,size_t Start, size_t End = -1);

		// Opens a console window and redirects the output to it
		void CreateConsole();

		// Creates a detached thread that on a loop clears the console and prints the logs
		// It prints with a fixed period in ms (250 by default)
		std::thread FirePrintThread(std::chrono::milliseconds Period = std::chrono::milliseconds(250));
	private:
		concurrency::concurrent_vector<Entry> Records;
	} Log;

	std::wostream& operator<<(std::wostream& OutputStream, const log_::Entry& Obj);
}