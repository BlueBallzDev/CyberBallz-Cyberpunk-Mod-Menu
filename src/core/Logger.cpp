#include "Logger.hpp"

#include <RED4ext/Api/v1/Sdk.hpp>

#include <Windows.h>
#include <Shlobj.h>
#include <cstdarg>
#include <mutex>
#include <share.h>
#include <string>

namespace cm::log
{
namespace
{
RED4ext::v1::PluginHandle g_handle = nullptr;
const RED4ext::v1::Sdk* g_sdk = nullptr;
FILE* g_file = nullptr;
std::mutex g_mutex;

std::wstring LogFilePath()
{
    // %LOCALAPPDATA%\CyberBallz\CyberBallz.log
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
    {
        out = path;
        out += L"\\CyberBallz";
        CreateDirectoryW(out.c_str(), nullptr);
        out += L"\\CyberBallz.log";
    }
    if (path)
        CoTaskMemFree(path);
    return out;
}
} // namespace

void Init(RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk)
{
    std::scoped_lock lock(g_mutex);
    g_handle = handle;
    g_sdk = sdk;
    if (!g_file)
    {
        const auto path = LogFilePath();
        if (!path.empty())
            // Plain byte stream. Must NOT use "ccs=UTF-8" here: that makes the
            // stream wide-oriented, and our narrow fprintf() would then trigger
            // the CRT invalid-parameter handler -> __fastfail (hard crash).
            // _wfsopen with _SH_DENYWR keeps the log readable by other processes
            // (so it can be tailed/diagnosed live while the game runs).
            g_file = _wfsopen(path.c_str(), L"w", _SH_DENYWR);
    }
}

void Shutdown()
{
    std::scoped_lock lock(g_mutex);
    if (g_file)
    {
        fflush(g_file);
        fclose(g_file);
        g_file = nullptr;
    }
    g_sdk = nullptr;
    g_handle = nullptr;
}

std::wstring DataFilePath(const char* leaf)
{
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
    {
        out = path;
        out += L"\\CyberBallz";
        CreateDirectoryW(out.c_str(), nullptr);
        out += L'\\';
        if (leaf)
            for (const char* p = leaf; *p; ++p)
                out += static_cast<wchar_t>(*p);
    }
    if (path)
        CoTaskMemFree(path);
    return out;
}

void Write(const char* level, const char* fmt, ...)
{
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::scoped_lock lock(g_mutex);

    if (g_sdk && g_sdk->logger && g_handle)
    {
        if (strcmp(level, "error") == 0)
            g_sdk->logger->Error(g_handle, buffer);
        else if (strcmp(level, "warn") == 0)
            g_sdk->logger->Warn(g_handle, buffer);
        else
            g_sdk->logger->Info(g_handle, buffer);
    }

    if (g_file)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(g_file, "[%02d:%02d:%02d.%03d] [%-5s] %s\n", st.wHour, st.wMinute, st.wSecond,
                st.wMilliseconds, level, buffer);
        fflush(g_file);
    }
}
} // namespace cm::log
