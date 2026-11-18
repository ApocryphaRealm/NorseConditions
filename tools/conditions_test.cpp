// conditions_test.cpp - offline test of NorseConditions.hpp: fake units, every grammar rule, error messages.
#include "NorseConditions.hpp"

#include <cstdio>

static int g_pass = 0, g_fail = 0;
static void Check(bool ok, const char* what)
{
    if (ok) ++g_pass;
    else
    {
        ++g_fail;
        std::printf("FAIL: %s\n", what);
    }
}

struct Unit
{
    double hp, maxHp;
    std::wstring type;
    std::set<std::wstring> tags;
    bool ally;
};

int main()
{
    nc::Registry r;
    auto U = [](void* u) { return static_cast<Unit*>(u); };
    r.Add(L"Health", 0, 0, [&](void* u, const nc::Context&, const auto&) { return std::optional<double>(U(u)->hp); });
    r.Add(L"HealthPercent", 0, 0, [&](void* u, const nc::Context&, const auto&) { return std::optional<double>(100 * U(u)->hp / U(u)->maxHp); });
    r.Add(L"HasTag", 1, 1, [&](void* u, const nc::Context&, const auto& a) { return std::optional<double>(U(u)->tags.count(a[0]) ? 1 : 0); });
    r.Add(L"IsAlly", 0, 0, [&](void* u, const nc::Context&, const auto&) { return std::optional<double>(U(u)->ally ? 1 : 0); });
    r.Add(L"UnitType", 1, 1, [&](void* u, const nc::Context&, const auto& a) -> std::optional<double> {
        if (U(u)->type.empty()) return std::nullopt;
        return U(u)->type.rfind(a[0].substr(0, a[0].size() - 1), 0) == 0 ? 1 : 0; // "X*" prefix only, enough here
    });
    r.Add(L"Pair", 2, 2, [&](void*, const nc::Context&, const auto& a) { return std::optional<double>(a[0] == L"a" && a[1] == L"b(c)" ? 1 : 0); });
    r.AddWorld(L"InTown", 0, 0, [&](void*, const nc::Context&, const auto&) { return std::optional<double>(1); });
    r.AddPreset(L"Hurt", L"HealthPercent < 50");
    r.AddPreset(L"HurtBrigand", L"@Hurt AND HasTag(Brigand)");
    r.AddPreset(L"Loop1", L"@Loop2");
    r.AddPreset(L"Loop2", L"@Loop1");

    Unit brig{10, 40, L"Outcast_Nithing", {L"Brigand"}, false};
    Unit hero{30, 30, L"", {}, true};
    nc::Context c{&brig, &hero};
    auto T = [&](const wchar_t* e, const nc::Context& ctx = nc::Context{}) {
        auto cond = nc::Condition::Compile(e, r);
        if (!cond.Ok()) std::printf("  compile error in '%ls': %s\n", e, cond.Error().c_str());
        return cond.Ok() && cond.Eval(ctx.subject ? ctx : nc::Context{&brig, &hero});
    };
    auto E = [&](const wchar_t* e) { return nc::Condition::Compile(e, r).Error(); };

    Check(T(L"Health < 20"), "comparison");
    Check(!T(L"Health >= 20"), "comparison false");
    Check(T(L"HealthPercent == 25"), "computed value");
    Check(T(L"HasTag(Brigand)"), "a ref with an argument is true");
    Check(!T(L"HasTag(Nope)"), "a missing tag is false");
    Check(T(L"HasTag(Brigand) AND NOT IsAlly"), "AND NOT");
    Check(T(L"HasTag(Nope) OR Health < 20"), "OR");
    Check(T(L"hastag(Brigand) and not isally"), "keywords and names are case-insensitive");
    Check(T(L"HasTag(Brigand) && !IsAlly"), "&& and !");
    Check(!T(L"NOT (HasTag(Brigand) OR IsAlly)"), "brackets");
    Check(T(L"Target.IsAlly AND Target.Health == 30"), "Target. reads the other unit");
    Check(!T(L"Target.IsAlly", nc::Context{&brig, nullptr}), "no target: a Target. condition is false");
    Check(T(L"NOT Target.IsAlly", nc::Context{&brig, nullptr}), "no target: NOT of it is true");
    Check(!T(L"UnitType(Outcast_*)", nc::Context{&hero, &brig}), "an unreadable value (no unit type) is false");
    Check(T(L"UnitType(Outcast_*)"), "unit type");
    Check(T(L"@Hurt"), "a preset");
    Check(T(L"@HurtBrigand AND NOT IsAlly"), "a preset using a preset");
    Check(T(L"Pair(a, b(c))"), "two arguments, one with brackets inside");
    Check(T(L"Health < 20 AND Health > -5.5"), "negative and decimal numbers");
    Check(E(L"Healt < 3").find("unknown condition 'Healt'") != std::string::npos, "unknown name error");
    Check(E(L"HasTag").find("takes 1 argument") != std::string::npos, "argument count error");
    Check(E(L"(Health < 3").find("not closed") != std::string::npos, "unclosed bracket error");
    Check(E(L"Health < ").find("ends too early") != std::string::npos, "early end error");
    Check(E(L"@Loop1").find("refers to itself") != std::string::npos, "preset loop error");
    Check(E(L"@Nope").find("no preset") != std::string::npos, "missing preset error");
    Check(E(L"Health < 3 Health").find("unexpected text") != std::string::npos, "trailing text error");
    Check(!nc::Condition::Compile(L"Healt", r).Eval(c), "a broken condition is always false");
    nc::Context none{};
    Check(nc::Condition::Compile(L"InTown", r).Eval(none), "a world condition is asked with no unit (NorseSound MapLoaded)");
    Check(!nc::Condition::Compile(L"Health > 0", r).Eval(none), "a unit condition with no unit is false, never a crash");
    Check(!nc::Condition::Compile(L"InTown AND Health > 0", r).Eval(none), "a mix with no unit: the unit part is false");
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
