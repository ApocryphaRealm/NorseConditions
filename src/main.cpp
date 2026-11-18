// NorseConditions - the "only when..." language of the Norse frameworks, in game (UE4SS v4.0.0 C++ mod), 2026-11-17.
// The language is include/NorseConditions.hpp; this mod registers what each condition reads in NORSE, loads named
// presets from conditions\*.ini (NorseManifest order), and exports it to the other frameworks:
//   int NC_Compile(const char* text, char* error, int errorSize)   a handle > 0, or 0 (error filled in)
//   int NC_Eval(int handle, void* subject, void* target)           1 / 0
// Console: nc.eval <condition> (logs the result for every unit on the map), nc.list (conditions and presets), nc.reload.
#include "NorseReflect.hpp"
#include "NorseManifest.hpp"
#include "NorseFilters.hpp"
#include "NorseEvents.h"
#include "NorseConditions.hpp"

#include <cmath>
#include <map>
#include <mutex>
#include <random>

namespace fs = std::filesystem;
using namespace nrn;
using namespace nrn::ue;
using namespace norse;

namespace
{
    fs::path g_mod, g_out;
    nc::Registry g_reg;
    std::vector<nc::Condition> g_compiled; // handle = index + 1
    std::mutex g_mx;
    bool g_inFight = false;
    std::wstring g_map;
    UObject* g_teamLib = nullptr;

    fs::path ModDir()
    {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&ModDir), &self);
        wchar_t buf[MAX_PATH * 2]{};
        GetModuleFileNameW(self, buf, static_cast<DWORD>(std::size(buf)));
        return fs::path(buf).parent_path().parent_path();
    }

    // ------------------------------------------------------------------ what the conditions read
    UObject* UnitComp(void* unit)
    {
        auto** p = Reflect::Get().Prop<UObject*>(static_cast<UObject*>(unit), L"UnitComponent");
        return p ? *p : nullptr;
    }

    UObject* UnitType(void* unit)
    {
        UObject* uc = UnitComp(unit);
        auto** p = uc ? Reflect::Get().Prop<UObject*>(uc, L"CombatUnit") : nullptr;
        return p ? *p : nullptr;
    }

    std::optional<double> Health(void* unit)
    {
        UObject* uc = UnitComp(unit);
        auto* hp = uc ? Reflect::Get().Prop<int32_t>(uc, L"RemainingHealth") : nullptr;
        if (!hp) return std::nullopt;
        return static_cast<double>(*hp);
    }

    std::optional<double> MaxHealth(void* unit)
    {
        UObject* uc = UnitComp(unit);
        if (!uc) return std::nullopt;
        Buf p;
        if (!Reflect::Get().Call(uc, L"GetMaxHealth", p.b, 4, sizeof(p.b))) return std::nullopt; // IHealth::GetMaxHealth -> int32
        int32_t v = 0;
        std::memcpy(&v, p.b, 4);
        return static_cast<double>(v);
    }

    std::optional<int> Team(void* unit)
    {
        if (!g_teamLib)
            g_teamLib = RC::Unreal::UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, L"/Script/Norse.Default__TeamIDLibrary", false);
        uint8_t t = 255;
        if (!g_teamLib || !GetTeamIDSafe(g_teamLib, static_cast<UObject*>(unit), t)) return std::nullopt;
        return static_cast<int>(t);
    }

    std::optional<FVec> Where(void* unit)
    {
        FVec v{};
        if (!GetVec(static_cast<UObject*>(unit), L"K2_GetActorLocation", v)) return std::nullopt;
        return v;
    }

    std::wstring NameOf(UObject* o)
    {
        return o ? ShortName(o->GetFullName(nullptr)) : L"";
    }

    using Args = std::vector<std::wstring>;
    std::optional<double> B(bool v)
    {
        return v ? 1.0 : 0.0;
    }

    void Register()
    {
        g_reg = nc::Registry{};
        g_reg.Add(L"Health", 0, 0, [](void* u, const nc::Context&, const Args&) { return Health(u); }, L"health left");
        g_reg.Add(L"MaxHealth", 0, 0, [](void* u, const nc::Context&, const Args&) { return MaxHealth(u); }, L"full health");
        g_reg.Add(L"HealthPercent", 0, 0, [](void* u, const nc::Context&, const Args&) -> std::optional<double> {
            auto h = Health(u), m = MaxHealth(u);
            if (!h || !m || *m <= 0) return std::nullopt;
            return 100.0 * *h / *m;
        }, L"health left, 0-100");
        g_reg.Add(L"IsDead", 0, 0, [](void* u, const nc::Context&, const Args&) -> std::optional<double> {
            auto h = Health(u);
            if (!h) return std::nullopt;
            return B(*h <= 0);
        }, L"health 0");
        g_reg.Add(L"HasTag", 1, 1, [](void* u, const nc::Context&, const Args& a) -> std::optional<double> {
            const TaggerApi* t = Tagger();
            if (!t) return std::nullopt;
            std::string tag = Narrow(a[0]);
            UObject* type = UnitType(u);
            return B(t->HasTag(u, tag.c_str()) || (type && t->HasTag(type, tag.c_str())));
        }, L"NorseTagger gave the unit (or its unit type) the tag");
        g_reg.Add(L"UnitType", 1, 1, [](void* u, const nc::Context&, const Args& a) -> std::optional<double> {
            UObject* type = UnitType(u);
            if (!type) return std::nullopt; // the player's party has no unit type asset
            return B(Glob(Lower(a[0]).c_str(), Lower(NameOf(type)).c_str()));
        }, L"the unit type's name, * wildcards (Outcast_*)");
        g_reg.Add(L"Field", 2, 2, [](void* u, const nc::Context&, const Args& a) -> std::optional<double> {
            UObject* type = UnitType(u);
            if (!type) return std::nullopt;
            auto* cls = reinterpret_cast<UStruct*>(Reflect::Get().ClassOf(type));
            FProperty* p = cls ? FindProp(cls, a[0]) : nullptr;
            if (!p) return std::nullopt;
            std::wstring v = Lower(Export(p, reinterpret_cast<uint8_t*>(type) + p->GetOffset_Internal(), type));
            return B(v.find(Lower(a[1])) != std::wstring::npos);
        }, L"a field of the unit type contains text - Field(Profiles, Brigands) is the faction");
        g_reg.Add(L"Team", 0, 0, [](void* u, const nc::Context&, const Args&) -> std::optional<double> {
            auto t = Team(u);
            if (!t) return std::nullopt;
            return static_cast<double>(*t);
        }, L"team number (0 = the player's side, 255 = none yet)");
        g_reg.Add(L"IsAlly", 0, 0, [](void* u, const nc::Context&, const Args&) -> std::optional<double> {
            auto t = Team(u);
            if (!t) return std::nullopt;
            return B(*t == 0);
        }, L"on the player's side");
        g_reg.Add(L"IsEnemy", 0, 0, [](void* u, const nc::Context&, const Args&) -> std::optional<double> {
            auto t = Team(u);
            if (!t) return std::nullopt;
            return B(*t != 0);
        }, L"not on the player's side");
        g_reg.AddWorld(L"InFight", 0, 0, [](void*, const nc::Context&, const Args&) { return B(g_inFight); },
                  L"a fight is on (NorseEvents FightStarted, until FightEnded or a new map)");
        g_reg.AddWorld(L"Map", 1, 1, [](void*, const nc::Context&, const Args& a) { return B(Lower(g_map).find(Lower(a[0])) != std::wstring::npos); },
                  L"the current map's path contains text");
        g_reg.AddWorld(L"Chance", 1, 1, [](void*, const nc::Context&, const Args& a) {
            static std::mt19937 rng{std::random_device{}()};
            return B(std::uniform_real_distribution<double>(0, 100)(rng) < _wtof(a[0].c_str()));
        }, L"true n percent of the times it is asked");
        g_reg.Add(L"Distance", 0, 0, [](void* u, const nc::Context& c, const Args&) -> std::optional<double> {
            if (!c.target) return std::nullopt;
            void* other = (u == c.subject) ? c.target : c.subject;
            auto a = Where(u), b = Where(other);
            if (!a || !b) return std::nullopt;
            return std::sqrt((a->X - b->X) * (a->X - b->X) + (a->Y - b->Y) * (a->Y - b->Y) + (a->Z - b->Z) * (a->Z - b->Z));
        }, L"distance to the other unit, in cm");
    }

    void LoadPresets()
    {
        fs::path dir = g_mod / "conditions";
        std::vector<fs::path> files;
        std::error_code ec;
        if (fs::is_directory(dir, ec))
            for (const auto& f : fs::directory_iterator(dir, ec))
                if (f.is_regular_file() && Lower(f.path().extension().wstring()) == L".ini") files.push_back(f.path());
        nm::Manifest man;
        man.Resolve(g_mod.parent_path().parent_path());
        files = man.Arrange(files, [](const std::string& s) { NRN_INFO("load order: %s", s.c_str()); });
        size_t n = 0;
        for (const auto& f : files)
            for (const auto& raw : ReadLines(f))
            {
                std::wstring l = Trim(StripComment(raw));
                size_t eq = l.find(L'=');
                if (l.empty() || l[0] == L'[' || eq == std::wstring::npos) continue;
                std::wstring name = Trim(l.substr(0, eq));
                if (!name.empty() && name[0] == L'@') name = name.substr(1);
                g_reg.AddPreset(name, Trim(l.substr(eq + 1)));
                ++n;
            }
        // check every preset compiles, so a broken one is reported at start, not when first used
        for (const auto& [key, text] : g_reg.Presets())
        {
            auto c = nc::Condition::Compile(L"@" + key, g_reg);
            if (!c.Ok()) NRN_WARN("preset @%s does not compile: %s", Narrow(key).c_str(), c.Error().c_str());
        }
        NRN_INFO("presets: %zu from %zu file(s) in %s", n, files.size(), Narrow(dir.wstring()).c_str());
    }

    void OnEvent(const NE_Event* e, void*)
    {
        if (!e || !e->name) return;
        if (std::strcmp(e->name, "FightStarted") == 0) g_inFight = true;
        else if (std::strcmp(e->name, "FightEnded") == 0) g_inFight = false;
    }

    bool g_subscribed = false;
    void Subscribe()
    {
        if (g_subscribed) return;
        if (const ne::ApiTable* api = ne::Api())
        {
            api->Subscribe("FightStarted", &OnEvent, nullptr);
            api->Subscribe("FightEnded", &OnEvent, nullptr);
            g_subscribed = true;
            NRN_INFO("InFight follows NorseEvents (FightStarted / FightEnded)");
        }
    }

    void EvalAll(const std::wstring& text)
    {
        auto c = nc::Condition::Compile(text, g_reg);
        if (!c.Ok())
        {
            NRN_WARN("nc.eval: %s", c.Error().c_str());
            return;
        }
        std::vector<UObject*> units;
        try
        {
            RC::Unreal::UObjectGlobals::FindAllOf(L"BP_Metahuman_Unit_C", units);
        }
        catch (...)
        {
        }
        int yes = 0;
        for (UObject* u : units)
        {
            bool r = c.Eval(nc::Context{u, nullptr});
            yes += r;
            auto h = Health(u);
            NRN_INFO("  %s %s (type %s, health %s)", r ? "TRUE " : "false", Narrow(NameOf(u)).c_str(), Narrow(NameOf(UnitType(u))).c_str(),
                     h ? std::to_string(static_cast<int>(*h)).c_str() : "?");
        }
        NRN_INFO("nc.eval '%s': true for %d of %zu unit(s)", Narrow(text).c_str(), yes, units.size());
    }

    bool RunCommand(const std::wstring& c)
    {
        std::wstring cmd = Trim(c), arg;
        if (size_t sp = cmd.find(L' '); sp != std::wstring::npos)
        {
            arg = Trim(cmd.substr(sp + 1));
            cmd = cmd.substr(0, sp);
        }
        cmd = Lower(cmd);
        NRN_INFO("command: %s %s", Narrow(cmd).c_str(), Narrow(arg).c_str());
        if (cmd == L"nc.eval" && !arg.empty()) EvalAll(arg);
        else if (cmd == L"nc.list")
        {
            for (const auto& [k, d] : g_reg.Defs())
                NRN_INFO("  %s%s - %s", Narrow(d.name).c_str(), d.maxArgs ? "(...)" : "", Narrow(d.help).c_str());
            for (const auto& [k, t] : g_reg.Presets()) NRN_INFO("  @%s = %s", Narrow(k).c_str(), Narrow(t).c_str());
        }
        else if (cmd == L"nc.reload")
        {
            Register();
            LoadPresets();
        }
        else NRN_WARN("unknown command - nc.eval <condition> | nc.list | nc.reload");
        return true;
    }
} // namespace

extern "C" __declspec(dllexport) int NC_Compile(const char* text, char* error, int errorSize)
{
    if (!text) return 0;
    std::lock_guard lk(g_mx);
    auto c = nc::Condition::Compile(nm::Widen(text), g_reg);
    if (!c.Ok())
    {
        if (error && errorSize > 0) strncpy_s(error, static_cast<size_t>(errorSize), c.Error().c_str(), _TRUNCATE);
        return 0;
    }
    g_compiled.push_back(std::move(c));
    return static_cast<int>(g_compiled.size());
}

extern "C" __declspec(dllexport) int NC_Eval(int handle, void* subject, void* target)
{
    if (handle <= 0 || handle > static_cast<int>(g_compiled.size())) return 0;
    return g_compiled[static_cast<size_t>(handle - 1)].Eval(nc::Context{subject, target}) ? 1 : 0;
}

class NorseConditions : public RC::CppUserModBase
{
  public:
    NorseConditions()
    {
        ModName = L"NorseConditions";
        ModVersion = L"" NC_VERSION;
        ModDescription = L"One 'only when...' language for every Norse framework: health, tags, unit type, faction, team, fight, map.";
        ModAuthors = L"ApocryphaRealm";
        for (auto& s : g_nopVtbl) s = reinterpret_cast<void*>(&NopFn);
        g_errOut.vtbl = g_nopVtbl;
        g_mod = ModDir();
        std::wstring dir;
        for (const auto& raw : ReadLines(g_mod / "NorseConditions.ini"))
        {
            std::wstring l = Trim(raw);
            if (l.rfind(L"sLogDir=", 0) == 0) dir = Trim(l.substr(8));
        }
        g_out = dir.empty() ? g_mod : fs::path(dir);
        Log::Get().Open(g_out / "NorseConditions.log", 2);
        Log::Get().Write(Level::Error, "NorseConditions v%s", NC_VERSION);
        Register();
        LoadPresets();
    }

    auto on_unreal_init() -> void override
    {
        Subscribe();
        try
        {
            RC::Unreal::Hook::RegisterInitGameStatePostCallback([](RC::Unreal::AGameModeBase* gm) {
                g_inFight = false;
                std::wstring full = gm ? reinterpret_cast<UObject*>(gm)->GetFullName(nullptr) : L"";
                g_map = full.substr(full.find(L' ') + 1);
                g_map = g_map.substr(0, g_map.find(L':'));
                Subscribe();
                NRN_INFO("map %s", Narrow(g_map).c_str());
            });
            RC::Unreal::Hook::RegisterProcessConsoleExecCallback(
                    [](UObject*, const wchar_t* cmd, RC::Unreal::FOutputDevice&, UObject*) -> bool {
                        if (!cmd || _wcsnicmp(cmd, L"nc.", 3) != 0) return false;
                        static ULONGLONG last = 0;
                        static std::wstring lastCmd;
                        ULONGLONG now = GetTickCount64();
                        if (lastCmd == cmd && now - last < 250) return true;
                        last = now;
                        lastCmd = cmd;
                        try
                        {
                            return RunCommand(cmd);
                        }
                        catch (...)
                        {
                            NRN_ERROR("command threw");
                            return true;
                        }
                    });
        }
        catch (const std::exception& e)
        {
            NRN_ERROR("callback registration failed: %s", e.what());
        }
    }
};

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod()
{
    return new NorseConditions();
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
