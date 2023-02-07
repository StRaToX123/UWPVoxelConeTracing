#pragma once
#include <string>
#include <wtypes.h>

inline std::wstring ExecutableDirectory()
{
    WCHAR buffer[MAX_PATH];
    GetModuleFileName(nullptr, buffer, MAX_PATH);

    return std::wstring(buffer);
}

inline std::string GetFilePath(const std::string& input)
{
    std::wstring exeDirL = ExecutableDirectory();
    std::string exeDir = std::string(exeDirL.begin(), exeDirL.end());
    std::string key("\\");
    exeDir = exeDir.substr(0, exeDir.rfind(key));
    exeDir.append(key);
    exeDir.append(input);

    return exeDir;
}

inline std::wstring GetFilePath(const std::wstring& input)
{
    std::wstring exeDir = ExecutableDirectory();
    std::wstring key(L"\\");
    exeDir = exeDir.substr(0, exeDir.rfind(key));
    exeDir.append(key);
    exeDir.append(input);

    return exeDir;
}



