#pragma once
#include <string>

// Lightweight toast notifications rendered in the top-right corner. Safe to
// call Add() from the render thread (UI actions).
namespace cm::gui::notify
{
enum class Type
{
    Info,
    Success,
    Warning,
    Error,
};

void Add(Type type, std::string message);
void Render(); // called once per frame after the menu draws
bool HasActive(); // any toasts currently showing?

inline void Info(std::string m) { Add(Type::Info, std::move(m)); }
inline void Success(std::string m) { Add(Type::Success, std::move(m)); }
inline void Warning(std::string m) { Add(Type::Warning, std::move(m)); }
inline void Error(std::string m) { Add(Type::Error, std::move(m)); }
} // namespace cm::gui::notify
