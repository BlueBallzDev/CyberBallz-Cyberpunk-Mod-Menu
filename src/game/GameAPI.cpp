#include "GameAPI.hpp"
#include "core/Logger.hpp"

#include <RED4ext/GameEngine.hpp>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace cm::game
{
namespace
{
// Avoid log spam: only warn once per missing symbol.
std::unordered_set<std::string> g_warned;
void WarnOnce(const std::string& key, const char* what, const char* name)
{
    if (g_warned.insert(key).second)
        CM_WARN("%s not found in RTTI: %s", what, name);
}
} // namespace

RED4ext::ScriptGameInstance GameInstance()
{
    return RED4ext::ScriptGameInstance();
}

RED4ext::Handle<RED4ext::IScriptable> GetPlayer()
{
    RED4ext::ScriptGameInstance gi;
    RED4ext::Handle<RED4ext::IScriptable> handle;
    RED4ext::ExecuteGlobalFunction("GetPlayer;GameInstance", &handle, gi);
    return handle;
}

bool PlayerValid()
{
    return static_cast<bool>(GetPlayer());
}

RED4ext::CClass* GetClass(const char* name)
{
    auto rtti = RED4ext::CRTTISystem::Get();
    if (!rtti)
        return nullptr;
    auto cls = rtti->GetClass(name);
    if (!cls)
        WarnOnce(std::string("cls:") + name, "Class", name);
    return cls;
}

RED4ext::CBaseFunction* GetMethod(const char* className, const char* funcName)
{
    auto cls = GetClass(className);
    if (!cls)
        return nullptr;
    // Walk the inheritance chain: many methods (GetWorldPosition, GetEntityID,
    // ...) are declared on a base class such as entEntity / gameObject.
    const RED4ext::CName fn(funcName);
    for (RED4ext::CClass* c = cls; c != nullptr; c = c->parent)
    {
        if (auto f = c->GetFunction(fn))
            return reinterpret_cast<RED4ext::CBaseFunction*>(f);
    }
    WarnOnce(std::string("fn:") + className + "." + funcName, "Method", funcName);
    return nullptr;
}

RED4ext::CBaseFunction* GetMethodFromClass(RED4ext::CClass* cls, const char* funcName)
{
    if (!cls)
        return nullptr;
    const RED4ext::CName fn(funcName);
    for (RED4ext::CClass* c = cls; c != nullptr; c = c->parent)
    {
        if (auto f = c->GetFunction(fn))
            return reinterpret_cast<RED4ext::CBaseFunction*>(f);
    }
    WarnOnce(std::string("autofn:") + funcName, "Method (auto)", funcName);
    return nullptr;
}

RED4ext::CBaseFunction* GetStaticFunction(const char* className, const char* funcName)
{
    auto cls = GetClass(className);
    if (!cls)
        return nullptr;
    const RED4ext::CName sn(funcName);

    // CClass::GetFunction walks staticFuncs -> funcs -> parent chain by short name.
    if (auto* f = cls->GetFunction(sn))
        return reinterpret_cast<RED4ext::CBaseFunction*>(f);

    // Many "static" helpers (RPGManager.CreateStatModifier, PlayerDevelopmentSystem
    // .GetData, ...) are actually registered as GLOBAL functions whose full name is
    // "Class::Func;<paramsig>". A direct GetFunction("Class::Func") misses because
    // of the param suffix, so enumerate globals and match by short name, preferring
    // the one whose full name starts with "Class::Func".
    if (auto rtti = RED4ext::CRTTISystem::Get())
    {
        RED4ext::DynArray<RED4ext::CBaseFunction*> globals;
        rtti->GetGlobalFunctions(globals);
        const std::string prefix = std::string(className) + "::" + funcName;
        RED4ext::CBaseFunction* shortMatch = nullptr;
        for (uint32_t i = 0; i < globals.Size(); ++i)
        {
            auto* g = globals[i];
            if (!g || g->shortName != sn)
                continue;
            const char* full = g->fullName.ToString();
            if (full && std::string(full).rfind(prefix, 0) == 0)
                return g; // exact "Class::Func..." match
            if (!shortMatch)
                shortMatch = g; // fall back to any global with this short name
        }
        if (shortMatch)
            return shortMatch;
    }

    WarnOnce(std::string("static:") + className + "." + funcName, "Static method", funcName);
    return nullptr;
}

int64_t ResolveEnum(const char* enumName, const char* memberName, int64_t fallback)
{
    auto rtti = RED4ext::CRTTISystem::Get();
    if (!rtti)
        return fallback;
    auto e = rtti->GetEnum(enumName);
    if (!e)
    {
        WarnOnce(std::string("enum:") + enumName, "Enum", enumName);
        return fallback;
    }
    const RED4ext::CName target(memberName);
    for (uint32_t i = 0; i < e->hashList.Size() && i < e->valueList.Size(); ++i)
        if (e->hashList[i] == target)
            return e->valueList[i];
    WarnOnce(std::string("enumval:") + enumName + "." + memberName, "Enum member", memberName);
    return fallback;
}

RED4ext::Handle<RED4ext::IScriptable> GetGameSystem(const char* getterFullName)
{
    RED4ext::ScriptGameInstance gi;
    RED4ext::Handle<RED4ext::IScriptable> handle;
    // GameInstance system getters are global static functions taking the
    // game instance, e.g. "GetTransactionSystem;GameInstance".
    RED4ext::ExecuteGlobalFunction(getterFullName, &handle, gi);
    return handle;
}

void LogClassFunctions(const char* className, const char* filter)
{
    auto cls = GetClass(className);
    if (!cls)
    {
        CM_WARN("LogClassFunctions: class '%s' not found", className);
        return;
    }
    auto matches = [&](const RED4ext::CName& n) -> bool
    {
        if (!filter || !*filter)
            return true;
        const char* s = n.ToString();
        if (!s)
            return false;
        // case-insensitive substring
        std::string hay(s), needle(filter);
        std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
        std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
        return hay.find(needle) != std::string::npos;
    };

    CM_INFO("---- functions of '%s' matching '%s' ----", className, filter ? filter : "*");
    for (RED4ext::CClass* c = cls; c != nullptr; c = c->parent)
    {
        for (uint32_t i = 0; i < c->staticFuncs.Size(); ++i)
            if (auto* f = c->staticFuncs[i]; f && matches(f->shortName))
                CM_INFO("  [static] %s  (full: %s)", f->shortName.ToString(), f->fullName.ToString());
        for (uint32_t i = 0; i < c->funcs.Size(); ++i)
            if (auto* f = c->funcs[i]; f && matches(f->shortName))
                CM_INFO("  [method] %s  (full: %s)", f->shortName.ToString(), f->fullName.ToString());
    }
    CM_INFO("---- end '%s' ----", className);
}

void LogGlobalFunctions(const char* filter)
{
    auto rtti = RED4ext::CRTTISystem::Get();
    if (!rtti)
        return;
    RED4ext::DynArray<RED4ext::CBaseFunction*> fns;
    rtti->GetGlobalFunctions(fns);
    std::string needle(filter ? filter : "");
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
    CM_INFO("---- global functions matching '%s' ----", filter ? filter : "*");
    int hits = 0;
    for (uint32_t i = 0; i < fns.Size(); ++i)
    {
        auto* f = fns[i];
        if (!f)
            continue;
        const char* full = f->fullName.ToString();
        const char* shortn = f->shortName.ToString();
        std::string hay(shortn ? shortn : "");
        std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
        if (needle.empty() || hay.find(needle) != std::string::npos)
        {
            CM_INFO("  [global] %s  (full: %s)", shortn ? shortn : "?", full ? full : "?");
            ++hits;
        }
    }
    CM_INFO("---- %d global match(es) for '%s' ----", hits, filter ? filter : "*");
}

void LogFunctionsMatching(const char* filter)
{
    auto rtti = RED4ext::CRTTISystem::Get();
    if (!rtti || !filter)
        return;
    std::string needle(filter);
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
    auto matches = [&](const char* s) {
        if (!s) return false;
        std::string h(s);
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        return h.find(needle) != std::string::npos;
    };

    CM_INFO("---- functions matching '%s' (global + methods) ----", filter);
    int hits = 0;
    RED4ext::DynArray<RED4ext::CBaseFunction*> globals;
    rtti->GetGlobalFunctions(globals);
    for (uint32_t i = 0; i < globals.Size() && hits < 60; ++i)
        if (auto* f = globals[i]; f && matches(f->shortName.ToString()))
        {
            CM_INFO("  [global] %s", f->fullName.ToString());
            ++hits;
        }
    RED4ext::DynArray<RED4ext::CBaseFunction*> methods;
    rtti->GetClassFunctions(methods);
    for (uint32_t i = 0; i < methods.Size() && hits < 60; ++i)
    {
        auto* f = methods[i];
        if (f && matches(f->shortName.ToString()))
        {
            auto* cf = reinterpret_cast<RED4ext::CClassFunction*>(f);
            const char* owner = (cf && cf->parent) ? cf->parent->GetName().ToString() : "?";
            CM_INFO("  %s::%s", owner, f->shortName.ToString());
            ++hits;
        }
    }
    CM_INFO("---- %d match(es) for '%s' ----", hits, filter);
}

namespace
{
// Return type name of a function (e.g. "handle:worldWeatherScriptInterface"),
// empty string when the function has no return value.
std::string ReturnTypeName(RED4ext::CBaseFunction* f)
{
    if (!f || !f->returnType || !f->returnType->type)
        return {};
    const char* n = f->returnType->type->GetName().ToString();
    return n ? std::string(n) : std::string{};
}
} // namespace

void LogFunctionsReturning(const char* typeFilter)
{
    auto rtti = RED4ext::CRTTISystem::Get();
    if (!rtti || !typeFilter)
        return;
    std::string needle(typeFilter);
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
    auto matches = [&](const std::string& s) {
        std::string h(s);
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        return h.find(needle) != std::string::npos;
    };

    CM_INFO("---- functions RETURNING a type matching '%s' ----", typeFilter);
    int hits = 0;
    RED4ext::DynArray<RED4ext::CBaseFunction*> globals;
    rtti->GetGlobalFunctions(globals);
    for (uint32_t i = 0; i < globals.Size() && hits < 80; ++i)
    {
        auto* f = globals[i];
        const std::string rn = ReturnTypeName(f);
        if (!rn.empty() && matches(rn))
        {
            CM_INFO("  [global] %s -> %s (params=%u)", f->fullName.ToString(), rn.c_str(), f->params.Size());
            ++hits;
        }
    }
    RED4ext::DynArray<RED4ext::CBaseFunction*> methods;
    rtti->GetClassFunctions(methods);
    for (uint32_t i = 0; i < methods.Size() && hits < 80; ++i)
    {
        auto* f = methods[i];
        const std::string rn = ReturnTypeName(f);
        if (!rn.empty() && matches(rn))
        {
            auto* cf = reinterpret_cast<RED4ext::CClassFunction*>(f);
            const char* owner = (cf && cf->parent) ? cf->parent->GetName().ToString() : "?";
            CM_INFO("  %s::%s -> %s (params=%u)", owner, f->shortName.ToString(), rn.c_str(), f->params.Size());
            ++hits;
        }
    }
    CM_INFO("---- %d function(s) return a type matching '%s' ----", hits, typeFilter);
}

namespace
{
// Invoke a 0- or 1-arg (GameInstance) getter and return its handle result.
RED4ext::Handle<RED4ext::IScriptable> TryInvokeGetter(RED4ext::CBaseFunction* f, const char* rn)
{
    if (!f || f->params.Size() > 1)
        return {};
    CM_INFO("CallGlobalReturning: invoking '%s' -> %s (params=%u)", f->fullName.ToString(), rn,
            f->params.Size());
    RED4ext::Handle<RED4ext::IScriptable> handle;
    bool ok = false;
    if (f->params.Size() == 0)
    {
        ok = RED4ext::ExecuteFunction(static_cast<void*>(nullptr), f, &handle);
    }
    else
    {
        RED4ext::ScriptGameInstance gi;
        RED4ext::StackArgs_t args;
        args.emplace_back(nullptr, &gi);
        ok = RED4ext::ExecuteFunction(static_cast<void*>(nullptr), f, &handle, args);
    }
    CM_INFO("CallGlobalReturning: '%s' -> ok=%d handle=%d", f->shortName.ToString(), ok ? 1 : 0, handle ? 1 : 0);
    return handle;
}
} // namespace

RED4ext::Handle<RED4ext::IScriptable> CallGlobalReturning(const char* returnTypeFilter)
{
    auto rtti = RED4ext::CRTTISystem::Get();
    if (!rtti || !returnTypeFilter)
        return {};
    std::string needle(returnTypeFilter);
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
    auto match = [&](const std::string& rn) {
        if (rn.empty())
            return false;
        std::string h(rn);
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        return h.find(needle) != std::string::npos;
    };

    // 1) Global functions (the conventional home of GetXSystem-style getters).
    RED4ext::DynArray<RED4ext::CBaseFunction*> globals;
    rtti->GetGlobalFunctions(globals);
    for (uint32_t i = 0; i < globals.Size(); ++i)
    {
        const std::string rn = ReturnTypeName(globals[i]);
        if (match(rn))
            if (auto h = TryInvokeGetter(globals[i], rn.c_str()))
                return h;
    }

    // 2) Class STATIC functions across every class (some accessors live here and
    // are invisible to GetGlobalFunctions). Statics take no instance, so they are
    // safe to invoke with a null context.
    RED4ext::DynArray<RED4ext::CClass*> classes;
    if (auto base = rtti->GetClass("IScriptable"))
        rtti->GetClasses(base, classes, nullptr, true);
    for (uint32_t ci = 0; ci < classes.Size(); ++ci)
    {
        auto* c = classes[ci];
        if (!c)
            continue;
        for (uint32_t fi = 0; fi < c->staticFuncs.Size(); ++fi)
        {
            auto* f = reinterpret_cast<RED4ext::CBaseFunction*>(c->staticFuncs[fi]);
            const std::string rn = ReturnTypeName(f);
            if (match(rn))
                if (auto h = TryInvokeGetter(f, rn.c_str()))
                    return h;
        }
    }
    return {};
}

RED4ext::Handle<RED4ext::IScriptable> GetSystemByClass(const char* className)
{
    auto engine = RED4ext::CGameEngine::Get();
    if (!engine || !engine->framework || !engine->framework->gameInstance)
        return {};
    // Cache the class lookup (CClass* is stable for the process); the live
    // instance is still re-fetched each call so save reloads are handled. This
    // keeps the per-frame god-mode/stat-pool path off the RTTI string hash.
    static std::unordered_map<std::string, RED4ext::CClass*> s_classCache;
    RED4ext::CClass* cls = nullptr;
    if (auto it = s_classCache.find(className); it != s_classCache.end())
        cls = it->second;
    else
    {
        cls = GetClass(className); // warns once if the class itself is missing
        s_classCache.emplace(className, cls);
    }
    if (!cls)
        return {};
    // GameInstance::GetSystem maps interface->implementation internally, so the
    // concrete ("gameStatPoolsSystem") or interface ("gameIStatPoolsSystem")
    // class both resolve to the same live instance.
    auto* raw = engine->framework->gameInstance->GetSystem(cls);
    if (!raw)
        return {};
    return RED4ext::Handle<RED4ext::IScriptable>(raw);
}
} // namespace cm::game
