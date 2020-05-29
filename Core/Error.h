#pragma once
#include "stdafx.h"

namespace FrameDX12
{
	enum class StatusCode
	{
		Ok = S_OK,
		Aborted = E_ABORT,
		AccessDenied = E_ACCESSDENIED,
		Failed = E_FAIL,
		InvalidHandle = E_HANDLE,
		InvalidArgument = E_INVALIDARG,
		InterfaceNotSupported = E_NOINTERFACE,
		NotImplemented = E_NOTIMPL,
		OutOfMemory = E_OUTOFMEMORY,
		InvalidPointer = E_POINTER,
		AdapterNotFound = D3D12_ERROR_ADAPTER_NOT_FOUND,
		DriverVersionMismatch = D3D12_ERROR_DRIVER_VERSION_MISMATCH,
		InvalidCall = DXGI_ERROR_INVALID_CALL,
		StillDrawing = DXGI_ERROR_WAS_STILL_DRAWING
	};

	inline std::wstring StatusCodeToString(StatusCode code)
	{
		switch (code)
		{
		case StatusCode::Ok:
			return L"Ok";
		case StatusCode::Aborted:
			return L"Aborted";
		case StatusCode::AccessDenied:
			return L"Acess Denied";
		case StatusCode::Failed:
			return L"Failed";
		case StatusCode::InvalidHandle:
			return L"Invalid Handle";
		case StatusCode::InvalidArgument:
			return L"Invalid Argument";
		case StatusCode::InterfaceNotSupported:
			return L"Interface Not Supported";
		case StatusCode::NotImplemented:
			return L"Not Implemented";
		case StatusCode::OutOfMemory:
			return L"Out Of Memory";
		case StatusCode::InvalidPointer:
			return L"Invalid Pointer";
		case StatusCode::AdapterNotFound:
			return L"Adapter Not Found";
		case StatusCode::DriverVersionMismatch:
			return L"Driver Version Mismatch";
		case StatusCode::InvalidCall:
			return L"Invalid Call";
		case StatusCode::StillDrawing:
			return L"Still Drawing";
		default:
			return L"Unknown Status Code";
		}
	}
	inline const char * StatusCodeToCString(StatusCode code)
	{
		switch (code)
		{
		case StatusCode::Ok:
			return "Ok";
		case StatusCode::Aborted:
			return "Aborted";
		case StatusCode::AccessDenied:
			return "Acess Denied";
		case StatusCode::Failed:
			return "Failed";
		case StatusCode::InvalidHandle:
			return "Invalid Handle";
		case StatusCode::InvalidArgument:
			return "Invalid Argument";
		case StatusCode::InterfaceNotSupported:
			return "Interface Not Supported";
		case StatusCode::NotImplemented:
			return "Not Implemented";
		case StatusCode::OutOfMemory:
			return "Out Of Memory";
		case StatusCode::InvalidPointer:
			return "Invalid Pointer";
		case StatusCode::AdapterNotFound:
			return "Adapter Not Found";
		case StatusCode::DriverVersionMismatch:
			return "Driver Version Mismatch";
		case StatusCode::InvalidCall:
			return "Invalid Call";
		case StatusCode::StillDrawing:
			return "Still Drawing";
		default:
			return "Unknown Status Code";
		}
	}

	struct WinError : public std::exception
	{
		StatusCode mError;

		WinError(HRESULT error) :
			mError((StatusCode)error)
		{
		}

		WinError(StatusCode error) :
			mError(error)
		{
		}

		const char* what() const throw ()
		{
			return StatusCodeToCString(mError);
		}
	};

#define LAST_ERROR (StatusCode)HRESULT_FROM_WIN32(GetLastError())
}
	

