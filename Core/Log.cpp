#include "stdafx.h"
#include "Log.h"
#include <iomanip>
#include <iostream>

using namespace FrameDX12;
using namespace std;

log_ FrameDX12::Log;

const wchar_t* kLogCategoryNames[(int)LogCategory::LogCategoryCount_] = { L"Info", L"Warning", L"Error", L"CriticalError" };

void log_::record_(const wstring & Message, LogCategory Category, int Line, const wstring & Function, const wstring & File)
{
	Entry e;
	e.Category = Category;
	e.Message = Message;
	e.Timestamp = chrono::system_clock::now();
	e.Line = Line;
	e.Function = Function;
	e.File = File;

	Records.push_back(e);

#ifdef _DEBUG
	wstringstream sstream;
	sstream << e;
	OutputDebugStringW(sstream.str().c_str());
#endif
}

std::wostream& FrameDX12::operator<<(std::wostream& OutputStream, const log_::Entry& Obj)
{
	auto time = chrono::system_clock::to_time_t(Obj.Timestamp);
	tm timeinfo;
	localtime_s(&timeinfo, &time);
	OutputStream << L"[" << put_time(&timeinfo, L"%T") << L"] " << kLogCategoryNames[(int)Obj.Category] << L" : " << Obj.Message << endl;
	OutputStream << L"    on line " << Obj.Line << L" of file " << Obj.File << L", function " << Obj.Function << endl;

	return OutputStream;
}

size_t log_::PrintAll(wostream & OutputStream)
{
	size_t count = 0;
	for(const auto& entry : Records)
	{
		OutputStream << entry;
		count++;
	}

	return count;
}

size_t log_::PrintRange(wostream & OutputStream,size_t Start, size_t End)
{
	size_t count = 0;
	for(size_t i = Start; i < End && i < Records.size(); i++)
	{
		OutputStream << Records[i];
		count++;
	}

	return count;
}

void log_::CreateConsole()
{
	AllocConsole();

	// Redirect std IO to the console
	FILE* fptr;
	freopen_s(&fptr, "CONIN$", "r", stdin);
	freopen_s(&fptr, "CONOUT$", "w", stdout);
	freopen_s(&fptr, "CONOUT$", "w", stderr);
}

std::thread log_::FirePrintThread(std::chrono::milliseconds Period)
{
	thread log_printer([Period]()
	{
		size_t index = 0;
		FrameDX12::TimedLoop([&]()
		{
			index += FrameDX12::Log.PrintRange(wcout, index);
		}, Period);
	});
	log_printer.detach();

	return log_printer;
}
