#pragma once
#include <RED4ext/Api/v1/Logger.hpp>
#include <RED4ext/Api/v1/PluginHandle.hpp>
#include <cstdio>
#include <string>

namespace RED4ext::v1
{
struct Sdk;
}

// Thin logging facade. Routes to the RED4ext logger when available (writes to
// red4ext/logs/CyberBallz.log) and mirrors to a private file so early-startup
// messages (before the SDK logger is wired) are never lost.
namespace cm::log
{
void Init(RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk);
void Shutdown();

void Write(const char* level, const char* fmt, ...);

// Absolute path of a file inside the plugin data dir (%LOCALAPPDATA%\CyberBallz),
// creating the directory if needed. Used for self-check reports etc. Empty on
// failure. `leaf` is an ASCII file name (e.g. "selfcheck_120000.md").
std::wstring DataFilePath(const char* leaf);
} // namespace cm::log

#define CM_INFO(...) ::cm::log::Write("info", __VA_ARGS__)
#define CM_WARN(...) ::cm::log::Write("warn", __VA_ARGS__)
#define CM_ERROR(...) ::cm::log::Write("error", __VA_ARGS__)
