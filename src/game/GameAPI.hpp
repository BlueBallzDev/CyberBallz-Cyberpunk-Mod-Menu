#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/ScriptGameInstance.hpp>

#include <memory>

// Thin, null-guarded helpers over the RED4ext RTTI/scripting system. Every
// game interaction in the feature modules goes through here so the binding
// surface is small and easy to validate against a given game build.
//
// All functions MUST be called from the game thread (RED4ext "Running" state
// OnUpdate). Calling the scripting VM from the render thread is not safe.
namespace cm::game
{
RED4ext::ScriptGameInstance GameInstance();

// The local player puppet (empty handle if not in-game).
RED4ext::Handle<RED4ext::IScriptable> GetPlayer();
bool PlayerValid();

// Resolve an RTTI class / method, returns nullptr on miss (logged once).
RED4ext::CClass* GetClass(const char* name);
RED4ext::CBaseFunction* GetMethod(const char* className, const char* funcName);

// Find a method by walking a concrete class' inheritance chain.
RED4ext::CBaseFunction* GetMethodFromClass(RED4ext::CClass* cls, const char* funcName);

// Find a class' static function (e.g. RPGManager.CreateStatModifier).
RED4ext::CBaseFunction* GetStaticFunction(const char* className, const char* funcName);

// Resolve an enum member to its value by name at runtime (patch-stable - avoids
// hardcoding numeric IDs that shift between game versions).
int64_t ResolveEnum(const char* enumName, const char* memberName, int64_t fallback);

// Diagnostic: log the static + instance function short names of a class (and
// its parents) whose name contains `filter` (nullptr = all). Used to discover
// the exact reflected name of a binding when a lookup misses.
void LogClassFunctions(const char* className, const char* filter);

// Diagnostic: log every GLOBAL RTTI function whose name contains `filter`.
// Reveals the exact registered name of statics exposed as globals.
void LogGlobalFunctions(const char* filter);

// Diagnostic: log every function (global + any class method) whose short name
// contains `filter`, with the owning class. Used to locate methods like
// SetWeather / wanted-level requests without typing console commands.
void LogFunctionsMatching(const char* filter);

// Diagnostic: log every function (global + any class method) whose RETURN TYPE
// name contains `typeFilter`. This is how we locate an accessor for a system
// whose "GetXSystem" getter was stripped (e.g. the weather script interface):
// we search for whatever function still hands back an instance of that type.
void LogFunctionsReturning(const char* typeFilter);

// Find a global function whose return type name contains `returnTypeFilter`
// and, if it takes 0 or 1 (GameInstance) parameters, call it and return the
// resulting handle. Used to obtain the weather script interface, which has no
// reflected getter on current builds. Returns an empty handle if none found.
RED4ext::Handle<RED4ext::IScriptable> CallGlobalReturning(const char* returnTypeFilter);

// Fetch a game system handle via a GameInstance static getter, e.g.
// "GetTransactionSystem", "GetStatPoolsSystem", "GetTimeSystem".
RED4ext::Handle<RED4ext::IScriptable> GetGameSystem(const char* getterFullName);

// Resolve a game system directly off the live GameInstance by its RTTI class
// name (e.g. "gameStatPoolsSystem" or the "gameIStatPoolsSystem" interface).
// This is the reliable path: it does not depend on a reflected "GetXSystem"
// getter existing as a global function (they don't on current game builds).
RED4ext::Handle<RED4ext::IScriptable> GetSystemByClass(const char* className);

// Read a property of any value type off an instance.
template <typename T>
bool GetProperty(const char* className, const char* propName, void* instance, T& out)
{
    auto cls = GetClass(className);
    if (!cls || !instance)
        return false;
    auto prop = cls->GetProperty(propName);
    if (!prop)
        return false;
    out = prop->GetValue<T>(instance);
    return true;
}

template <typename T>
bool SetProperty(const char* className, const char* propName, void* instance, const T& value)
{
    auto cls = GetClass(className);
    if (!cls || !instance)
        return false;
    auto prop = cls->GetProperty(propName);
    if (!prop)
        return false;
    prop->SetValue<T>(instance, value);
    return true;
}

// Call an instance method by name with arbitrary args. There is no variadic
// ExecuteFunction overload taking an instance pointer, so build the stack args
// manually (types are resolved from the function's RTTI signature).
template <typename... Args>
bool CallMethod(const RED4ext::Handle<RED4ext::IScriptable>& self, const char* className,
                const char* funcName, void* out, Args&&... args)
{
    if (!self)
        return false;
    auto fn = GetMethod(className, funcName);
    if (!fn)
        return false;

    // ExecuteFunction returns false WITHOUT running the function if it has a
    // return type but aOut is null. Supply scratch space when the caller didn't
    // ask for the return value (e.g. Bool-returning GiveItem/ApplyStatusEffect).
    alignas(16) uint8_t scratch[64];
    void* outPtr = (out == nullptr && fn->returnType) ? scratch : out;

    if constexpr (sizeof...(Args) == 0)
    {
        return RED4ext::ExecuteFunction(self.instance, fn, outPtr);
    }
    else
    {
        RED4ext::StackArgs_t stackArgs;
        stackArgs.reserve(sizeof...(Args));
        (stackArgs.emplace_back(nullptr,
                                const_cast<void*>(static_cast<const void*>(std::addressof(args)))),
         ...);
        return RED4ext::ExecuteFunction(self.instance, fn, outPtr, stackArgs);
    }
}

// Call a method resolving the function from the instance's *runtime* class
// (via IScriptable::GetType) - no need to know the class name. This is how we
// call game-system methods (weather, vehicle, status effects, ...) reliably.
template <typename... Args>
bool CallAuto(const RED4ext::Handle<RED4ext::IScriptable>& self, const char* funcName, void* out, Args&&... args)
{
    if (!self || !self.instance)
        return false;
    RED4ext::CClass* cls = self.instance->GetType();
    RED4ext::CBaseFunction* fn = GetMethodFromClass(cls, funcName);
    if (!fn)
        return false;

    // ExecuteFunction returns false WITHOUT running the function if it has a
    // return type but aOut is null. Supply scratch space when the caller didn't
    // ask for the return value (e.g. Bool-returning ApplyStatusEffect/AddModifier).
    alignas(16) uint8_t scratch[64];
    void* outPtr = (out == nullptr && fn->returnType) ? scratch : out;

    if constexpr (sizeof...(Args) == 0)
    {
        return RED4ext::ExecuteFunction(self.instance, fn, outPtr);
    }
    else
    {
        RED4ext::StackArgs_t stackArgs;
        stackArgs.reserve(sizeof...(Args));
        (stackArgs.emplace_back(nullptr,
                                const_cast<void*>(static_cast<const void*>(std::addressof(args)))),
         ...);
        return RED4ext::ExecuteFunction(self.instance, fn, outPtr, stackArgs);
    }
}
} // namespace cm::game
