#pragma once
#include "Error.h"
#include <iostream>

void FrameDX12::PrintErrorBlob(ComPtr<ID3DBlob> blob)
{
    using namespace std;

    // Error blobs, for example from D3DCompileFromFile, are ASCII strings
    // To be modern, I'm using UTF-8 on the output
    // That means I need to convert
    // BUT, assuming the error string is going to use only 7-bit ASCII characters
    //		(something that seems reasonable)
    // then you can just convert as is, because UTF-8 is backwards compatible with 7-bit ASCII
    wstring error_string;
    error_string.resize(blob->GetBufferSize(), L' ');
    char* raw_error_string = (char*)blob->GetBufferPointer();

    // Traverse the string and copy the values.
    // Ignore the ones that are outside the 7-bit ASCII range
    // Those characters are replaced by a whitespace
    for (auto [idx, c] : fpp::enumerate(error_string))
    {
        auto new_char = raw_error_string[idx];
        if (new_char < 127)
            c = new_char;
    }

    wcout << error_string;
}
