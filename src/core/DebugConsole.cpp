#include "DebugConsole.hpp"
#include "State.hpp"
#include "Toggles.hpp"
#include "Logger.hpp"
#include "game/Features.hpp"

#include <RED4ext/RTTISystem.hpp>
#include <RED4ext/RTTITypes.hpp>
#include <RED4ext/Scripting/Functions.hpp>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>
#include <sstream>

namespace cm::debug
{
namespace
{
std::mutex g_mutex;
std::deque<std::string> g_output;
std::vector<std::string> g_queue;
std::atomic<bool> g_open{false};
constexpr size_t kMaxLines = 400;

void Out(const std::string& s)
{
    {
        std::scoped_lock lock(g_mutex);
        g_output.push_back(s);
        while (g_output.size() > kMaxLines)
            g_output.pop_front();
    }
    // Mirror to the log file so console/RTTI-explorer output can be read off the
    // log (remote diagnosis without screenshots).
    CM_INFO("console: %s", s.c_str());
}

void Outf(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Out(buf);
}

std::vector<std::string> Tokenize(const std::string& s)
{
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok)
        out.push_back(tok);
    return out;
}

bool Contains(const char* hay, const std::string& lowerNeedle)
{
    if (!hay)
        return false;
    std::string h(hay);
    for (auto& c : h)
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return h.find(lowerNeedle) != std::string::npos;
}

std::string Lower(std::string v)
{
    for (auto& c : v)
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return v;
}

// RTTI explorer commands (game thread only)
void RttiDump(const std::string& className)
{
    auto rtti = RED4ext::CRTTISystem::Get();
    auto cls = rtti ? rtti->GetClass(className.c_str()) : nullptr;
    if (!cls)
    {
        Outf("  class '%s' not found", className.c_str());
        return;
    }
    Outf("== %s ==", className.c_str());
    int n = 0;
    for (RED4ext::CClass* c = cls; c != nullptr && n < 120; c = c->parent)
    {
        for (uint32_t i = 0; i < c->staticFuncs.Size() && n < 120; ++i)
            if (auto* f = c->staticFuncs[i])
            {
                Outf("  [static] %s", f->fullName.ToString());
                ++n;
            }
        for (uint32_t i = 0; i < c->funcs.Size() && n < 120; ++i)
            if (auto* f = c->funcs[i])
            {
                Outf("  %s", f->fullName.ToString());
                ++n;
            }
    }
    Outf("  (%d functions shown)", n);
}

void RttiFind(const std::string& sub)
{
    auto rtti = RED4ext::CRTTISystem::Get();
    if (!rtti)
        return;
    const std::string needle = Lower(sub);
    int hits = 0;

    RED4ext::DynArray<RED4ext::CBaseFunction*> globals;
    rtti->GetGlobalFunctions(globals);
    for (uint32_t i = 0; i < globals.Size() && hits < 80; ++i)
        if (auto* f = globals[i]; f && Contains(f->shortName.ToString(), needle))
        {
            Outf("  [global] %s", f->fullName.ToString());
            ++hits;
        }

    RED4ext::DynArray<RED4ext::CBaseFunction*> methods;
    rtti->GetClassFunctions(methods);
    for (uint32_t i = 0; i < methods.Size() && hits < 80; ++i)
    {
        auto* f = methods[i];
        if (f && Contains(f->shortName.ToString(), needle))
        {
            auto* cf = reinterpret_cast<RED4ext::CClassFunction*>(f);
            const char* owner = (cf && cf->parent) ? cf->parent->GetName().ToString() : "?";
            Outf("  %s::%s", owner, f->shortName.ToString());
            ++hits;
        }
    }
    Outf("  (%d match%s for '%s')", hits, hits == 1 ? "" : "es", sub.c_str());
}

void RttiClass(const std::string& sub)
{
    auto rtti = RED4ext::CRTTISystem::Get();
    auto base = rtti ? rtti->GetClass("IScriptable") : nullptr;
    if (!base)
    {
        Out("  IScriptable not found");
        return;
    }
    RED4ext::DynArray<RED4ext::CClass*> classes;
    rtti->GetClasses(base, classes, nullptr, true);
    const std::string needle = Lower(sub);
    int hits = 0;
    for (uint32_t i = 0; i < classes.Size() && hits < 80; ++i)
        if (auto* c = classes[i]; c && Contains(c->GetName().ToString(), needle))
        {
            Outf("  %s", c->GetName().ToString());
            ++hits;
        }
    Outf("  (%d class match%s for '%s')", hits, hits == 1 ? "" : "es", sub.c_str());
}

void PrintHelp()
{
    Out("Commands:");
    Out("  help                      this list");
    Out("  clear                     clear the log");
    Out("  list                      list feature ids");
    Out("  feature <id> [on|off]     toggle a feature (e.g. feature godmode on)");
    Out("  money <amount>            add eddies");
    Out("  give <tdbid> [qty]        give an item");
    Out("  heal                      restore the player");
    Out("  tp                        teleport to tracked objective");
    Out("  selfcheck                 write a self-check report (bindings + feature state)");
    Out("  rtti find <substr>        find functions by name (e.g. rtti find GetData)");
    Out("  rtti dump <class>         list a class's functions (e.g. rtti dump gameRPGManager)");
    Out("  rtti class <substr>       list class names");
}

void Execute(const std::string& cmd)
{
    auto t = Tokenize(cmd);
    if (t.empty())
        return;
    const std::string& c = t[0];
    auto& st = State::Get();

    if (c == "help") { PrintHelp(); }
    else if (c == "clear") { ClearOutput(); }
    else if (c == "list")
    {
        for (const auto& tg : toggles::All())
            Outf("  %-16s %s%s", tg.id, tg.label, tg.experimental ? "  (experimental)" : "");
    }
    else if (c == "feature" && t.size() >= 2)
    {
        const toggles::Toggle* tg = toggles::Find(t[1]);
        if (!tg) { Outf("  unknown feature '%s' (try 'list')", t[1].c_str()); return; }
        bool target = !tg->flag->load();
        if (t.size() >= 3)
        {
            const std::string v = Lower(t[2]);
            if (v == "on" || v == "1" || v == "true") target = true;
            else if (v == "off" || v == "0" || v == "false") target = false;
        }
        tg->flag->store(target);
        Outf("  %s = %s", tg->id, target ? "ON" : "OFF");
    }
    else if (c == "money" && t.size() >= 2)
    {
        Action a{ActionType::AddMoney};
        a.value = atof(t[1].c_str());
        st.Push(a);
        Outf("  queued +%s eddies", t[1].c_str());
    }
    else if (c == "give" && t.size() >= 2)
    {
        Action a{ActionType::GiveItem};
        a.text = t[1];
        a.quantity = t.size() >= 3 ? atoi(t[2].c_str()) : 1;
        st.Push(a);
        Outf("  queued give %s x%d", a.text.c_str(), a.quantity);
    }
    else if (c == "heal") { st.Push({ActionType::HealPlayer}); Out("  queued heal"); }
    else if (c == "tp") { st.Push({ActionType::TeleportToObjective}); Out("  queued teleport to objective"); }
    else if (c == "selfcheck")
    {
        // Runs on the game thread (Pump()), so the read-backs are VM-safe.
        cm::features::RunSelfCheck();
        Out("  self-check report written to %LOCALAPPDATA%\\CyberBallz (see log)");
    }
    else if (c == "rtti" && t.size() >= 3 && t[1] == "find") { RttiFind(t[2]); }
    else if (c == "rtti" && t.size() >= 3 && t[1] == "dump") { RttiDump(t[2]); }
    else if (c == "rtti" && t.size() >= 3 && t[1] == "class") { RttiClass(t[2]); }
    else { Outf("  unknown command '%s' (try 'help')", c.c_str()); }
}
} // namespace

void Submit(const std::string& command)
{
    if (command.empty())
        return;
    Out("> " + command);
    std::scoped_lock lock(g_mutex);
    g_queue.push_back(command);
}

void Print(const std::string& line) { Out(line); }

std::vector<std::string> Output()
{
    std::scoped_lock lock(g_mutex);
    return std::vector<std::string>(g_output.begin(), g_output.end());
}

void ClearOutput()
{
    std::scoped_lock lock(g_mutex);
    g_output.clear();
}

void Pump()
{
    std::vector<std::string> pending;
    {
        std::scoped_lock lock(g_mutex);
        pending.swap(g_queue);
    }
    for (const auto& cmd : pending)
        Execute(cmd);
}

bool IsOpen() { return g_open.load(); }
void Toggle() { g_open.store(!g_open.load()); }
void SetOpen(bool open) { g_open.store(open); }
} // namespace cm::debug
