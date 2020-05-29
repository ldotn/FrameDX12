#include "stdafx.h"
#include "Log.h"
#include <iomanip>
#include <iostream>

using namespace FrameDX12;
using namespace std;

log_ FrameDX12::Log;

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

}

size_t log_::PrintAll(wostream & OutputStream)
{
	size_t count = 0;
	for(const auto& e : Records)
	{
		auto time = chrono::system_clock::to_time_t(e.Timestamp);
		tm timeinfo;
		localtime_s(&timeinfo, &time);
		OutputStream << L"[" << put_time(&timeinfo, L"%T") << L"] " << cat_name[(int)e.Category] << L" : " << e.Message << endl;
		OutputStream << L"    on line " << e.Line << L" of file " << e.File << L", function " << e.Function << endl;
		count++;
	}

	return count;
}

size_t log_::PrintRange(wostream & OutputStream,size_t Start, size_t End)
{
	size_t count = 0;
	for(size_t i = Start; i < End && i < Records.size(); i++)
	{
		const auto& e = Records[i];
		auto time = chrono::system_clock::to_time_t(e.Timestamp);
		tm timeinfo;
		localtime_s(&timeinfo, &time);
		OutputStream << L"[" << put_time(&timeinfo, L"%T") << L"] " << cat_name[(int)e.Category] << L" : " << e.Message << endl;
		OutputStream << L"    on line " << e.Line << L" of file " << e.File << L", function " << e.Function << endl;
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
