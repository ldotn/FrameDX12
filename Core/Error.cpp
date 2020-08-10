#pragma once
#include "Error.h"
#include "Log.h"
#include "Utils.h"

bool FrameDX12::LogErrorBlob(ComPtr<ID3DBlob> blob)
{
    using namespace std;
    
    if (blob)
    {
        // Error blobs, for example from D3DCompileFromFile, are ASCII strings
        LogMsg(StringToWString((char*)blob->GetBufferPointer()), LogCategory::Error);

        blob->Release();
        return true;
    }
    else
    {
        return false;
    }
}
