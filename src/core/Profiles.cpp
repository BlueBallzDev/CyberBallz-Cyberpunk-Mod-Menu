#include "Profiles.hpp"
#include "State.hpp"
#include "Toggles.hpp"
#include "Json.hpp"
#include "Logger.hpp"

#include <Windows.h>
#include <Shlobj.h>
#include <shellapi.h>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace cm::profiles
{
namespace
{
std::wstring Dir()
{
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
    {
        out = path;
        out += L"\\CyberBallz\\profiles";
    }
    if (path)
        CoTaskMemFree(path);
    return out;
}

std::wstring Widen(const std::string& s)
{
    if (s.empty())
        return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

std::wstring PathFor(const std::string& name)
{
    return Dir() + L"\\" + Widen(name) + L".json";
}

bool ReadFileW(const std::wstring& path, std::string& out)
{
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f)
        return false;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    fclose(f);
    return true;
}
} // namespace

std::vector<std::string> List()
{
    std::vector<std::string> out;
    std::error_code ec;
    const fs::path dir(Dir());
    if (!fs::exists(dir, ec))
        return out;
    for (const auto& e : fs::directory_iterator(dir, ec))
    {
        if (ec)
            break;
        if (e.is_regular_file() && e.path().extension() == ".json")
            out.push_back(e.path().stem().string());
    }
    return out;
}

bool Save(const std::string& name)
{
    if (name.empty())
        return false;
    std::error_code ec;
    fs::create_directories(fs::path(Dir()), ec);

    std::string j = "{\n";
    auto B = [&](const char* k, bool v) { j += "  \""; j += k; j += "\": "; j += (v ? "true" : "false"); j += ",\n"; };
    auto F = [&](const char* k, float v)
    {
        char line[96];
        std::snprintf(line, sizeof(line), "  \"%s\": %.4f,\n", k, v);
        j += line;
    };

    // Every toggle + every numeric value, straight from the registries. Floats
    // get a "val." prefix so a value id can't collide with a toggle id (e.g. the
    // sellMult/carryCapacity/timeScale/fov/damageMult bool+float pairs).
    for (const auto& t : toggles::All())
        if (t.flag)
            B(t.id, t.flag->load());
    for (const auto& v : toggles::Floats())
        if (v.value)
            F((std::string("val.") + v.id).c_str(), v.value->load());
    j += "  \"_v\": 3\n}\n"; // trailing key (no comma) keeps JSON valid

    FILE* f = nullptr;
    if (_wfopen_s(&f, PathFor(name).c_str(), L"wb") != 0 || !f)
    {
        CM_WARN("Profiles: failed to write '%s'", name.c_str());
        return false;
    }
    fwrite(j.data(), 1, j.size(), f);
    fclose(f);
    CM_INFO("Profiles: saved '%s' (%zu bytes)", name.c_str(), j.size());
    return true;
}

bool Load(const std::string& name)
{
    std::string text;
    if (!ReadFileW(PathFor(name), text))
        return false;
    json::Value root;
    if (!json::Parse(text, root) || !root.IsObject())
    {
        CM_WARN("Profiles: parse failed for '%s'", name.c_str());
        return false;
    }
    for (const auto& t : toggles::All())
        if (t.flag)
            if (const json::Value* v = root.Find(t.id))
                t.flag->store(v->AsBool(t.flag->load()));
    for (const auto& v : toggles::Floats())
        if (v.value)
            if (const json::Value* jv = root.Find((std::string("val.") + v.id).c_str()); jv && jv->IsNumber())
                v.value->store(static_cast<float>(jv->number));
    CM_INFO("Profiles: loaded '%s'", name.c_str());
    return true;
}

bool Delete(const std::string& name)
{
    std::error_code ec;
    return fs::remove(fs::path(PathFor(name)), ec);
}

void OpenFolder()
{
    std::error_code ec;
    fs::create_directories(fs::path(Dir()), ec);
    ShellExecuteW(nullptr, L"open", Dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void ClearAll()
{
    // Turn off every registered toggle (covers all features automatically).
    for (const auto& t : toggles::All())
        if (t.flag)
            t.flag->store(false);
}

const std::vector<Persona>& Personas()
{
    static const std::vector<Persona> kPersonas = {
        {"Street Samurai", "God mode, no ragdoll/fall dmg, fast move, infinite ammo + no recoil"},
        {"Netrunner", "Infinite RAM, quickhack tuning, undetectable"},
        {"Photo Mode", "Freeze time + world, hide HUD for screenshots"},
        {"Chaos", "Everything offensive on - max carnage"},
        {"Clean Slate", "All toggles off (safe baseline)"},
    };
    return kPersonas;
}

void ApplyPersona(int index)
{
    auto& s = State::Get();
    ClearAll();
    switch (index)
    {
    case 0: // Street Samurai
        s.godMode = true;
        s.noFallDamage = true;
        s.noRagdoll = true;
        s.moveSpeedEnabled = true;
        s.moveSpeedMult = 1.5f;
        s.infiniteStamina = true;
        s.infiniteAmmoNoReload = true;
        s.noRecoil = true;
        break;
    case 1: // Netrunner
        s.infiniteMemory = true;
        s.ramRegenBoost = true;
        s.quickhackCostZero = true;
        s.quickhackNoCooldown = true;
        s.undetectable = true;
        break;
    case 2: // Photo Mode
        s.photoFreeze = true;
        s.freezeTime = true;
        s.godMode = true;
        break;
    case 3: // Chaos
        s.godMode = true;
        s.noRagdoll = true;
        s.oneHitKill = true;
        s.infiniteAmmoNoReload = true;
        s.rapidFireEnabled = true;
        s.rapidFireMult = 6.0f;
        s.noRecoil = true;
        s.noSpread = true;
        s.sandevistanInfinite = true;
        break;
    case 4: // Clean Slate
    default:
        break; // ClearAll already ran
    }
    CM_INFO("Profiles: applied persona %d", index);
}
} // namespace cm::profiles
