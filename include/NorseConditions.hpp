// NorseConditions.hpp - one "only when..." language for every Norse framework (2026-11-17). Header-only, game-free:
// the conditions themselves (what Health or HasTag means) are registered by the host - the NorseConditions mod for the
// game, tools/conditions_test.cpp for the offline test.
//
//   InFight AND HealthPercent < 50 AND (HasTag(Brigand) OR UnitType(Outcast_*)) AND NOT Target.IsAlly
//
// Grammar: or := and (OR and)* ; and := not (AND not)* ; not := NOT not | cmp ;
//          cmp := term ((< <= > >= == !=) term)? ; term := ( or ) | number | ref
//          ref := [Target.] Name [ ( arg, arg ) ] | @Preset
// AND OR NOT are case-insensitive (&& || ! also work). A ref without a comparison is true when it is not 0. A condition
// that cannot be read (no target, a unit with no unit type) makes its comparison false - never an error at run time.
// Unknown names, wrong argument counts and preset loops are compile errors with the position.
// Modelled on Open Animation Replacer's conditions and condition presets (Nexus 92109).
#pragma once

#include <cmath>
#include <cwctype>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace nc
{
struct Context
{
    void* subject = nullptr; // the unit the rule is about
    void* target = nullptr;  // the other unit (attacker, victim, lock-on target), or null
};

// args: the raw text of each argument, trimmed. Return nullopt when the value cannot be read.
using CondFn = std::function<std::optional<double>(void* unit, const Context& ctx, const std::vector<std::wstring>& args)>;

struct CondDef
{
    std::wstring name;
    int minArgs = 0, maxArgs = 0;
    CondFn fn;
    std::wstring help;
};

class Registry
{
  public:
    void Add(const std::wstring& name, int minArgs, int maxArgs, CondFn fn, const std::wstring& help = L"")
    {
        m_defs[Low(name)] = CondDef{name, minArgs, maxArgs, std::move(fn), help};
    }
    void AddPreset(const std::wstring& name, const std::wstring& expr) { m_presets[Low(name)] = expr; }
    const CondDef* Find(const std::wstring& name) const
    {
        auto it = m_defs.find(Low(name));
        return it == m_defs.end() ? nullptr : &it->second;
    }
    const std::wstring* Preset(const std::wstring& name) const
    {
        auto it = m_presets.find(Low(name));
        return it == m_presets.end() ? nullptr : &it->second;
    }
    const std::map<std::wstring, CondDef>& Defs() const { return m_defs; }
    const std::map<std::wstring, std::wstring>& Presets() const { return m_presets; }
    static std::wstring Low(std::wstring s)
    {
        for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
        return s;
    }

  private:
    std::map<std::wstring, CondDef> m_defs;
    std::map<std::wstring, std::wstring> m_presets;
};

struct Node
{
    enum Kind
    {
        Num,
        Ref,
        Not,
        And,
        Or,
        Cmp
    } kind = Num;
    double num = 0;
    const CondDef* def = nullptr;
    bool onTarget = false;
    std::vector<std::wstring> args;
    std::wstring op;
    std::unique_ptr<Node> a, b;
};

class Condition
{
  public:
    bool Ok() const { return m_root != nullptr; }
    const std::string& Error() const { return m_error; }
    const std::wstring& Text() const { return m_text; }

    // true/false; a condition that could not be compiled is always false
    bool Eval(const Context& ctx) const
    {
        if (!m_root) return false;
        auto v = Value(*m_root, ctx);
        return v && *v != 0;
    }

    static Condition Compile(const std::wstring& text, const Registry& reg)
    {
        Condition c;
        c.m_text = text;
        Parser p{text, reg, 0, {}, {}};
        try
        {
            auto root = p.Or();
            p.Skip();
            if (p.pos < text.size()) p.Fail("unexpected text '" + Narrow(text.substr(p.pos, 12)) + "'");
            c.m_root = std::move(root);
        }
        catch (const std::string& e)
        {
            c.m_error = e;
        }
        return c;
    }

  private:
    std::shared_ptr<Node> m_root;
    std::string m_error;
    std::wstring m_text;

    static std::string Narrow(const std::wstring& w)
    {
        std::string s;
        for (wchar_t c : w) s += c < 128 ? static_cast<char>(c) : '?';
        return s;
    }

    static std::optional<double> Value(const Node& n, const Context& ctx)
    {
        switch (n.kind)
        {
        case Node::Num: return n.num;
        case Node::Ref:
        {
            void* unit = n.onTarget ? ctx.target : ctx.subject;
            if (!unit) return std::nullopt;
            try
            {
                return n.def->fn(unit, ctx, n.args);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }
        case Node::Not:
        {
            auto v = Value(*n.a, ctx);
            return (v && *v != 0) ? 0.0 : 1.0; // an unreadable value is false, so NOT of it is true
        }
        case Node::And:
        {
            auto x = Value(*n.a, ctx);
            if (!x || *x == 0) return 0.0;
            auto y = Value(*n.b, ctx);
            return (y && *y != 0) ? 1.0 : 0.0;
        }
        case Node::Or:
        {
            auto x = Value(*n.a, ctx);
            if (x && *x != 0) return 1.0;
            auto y = Value(*n.b, ctx);
            return (y && *y != 0) ? 1.0 : 0.0;
        }
        case Node::Cmp:
        {
            auto x = Value(*n.a, ctx), y = Value(*n.b, ctx);
            if (!x || !y) return 0.0;
            const std::wstring& o = n.op;
            bool r = o == L"<" ? *x < *y : o == L"<=" ? *x <= *y : o == L">" ? *x > *y : o == L">=" ? *x >= *y
                   : o == L"==" ? std::fabs(*x - *y) < 1e-9 : std::fabs(*x - *y) >= 1e-9;
            return r ? 1.0 : 0.0;
        }
        }
        return std::nullopt;
    }

    struct Parser
    {
        const std::wstring& s;
        const Registry& reg;
        size_t pos;
        std::set<std::wstring> presetStack;
        std::wstring presetName;

        [[noreturn]] void Fail(const std::string& why)
        {
            throw std::string((presetName.empty() ? "" : "in preset @" + Narrow(presetName) + ": ") + why + " (at character " +
                              std::to_string(pos + 1) + ")");
        }
        void Skip()
        {
            while (pos < s.size() && iswspace(s[pos])) ++pos;
        }
        bool Word(const wchar_t* w)
        {
            Skip();
            size_t n = wcslen(w);
            if (pos + n > s.size() || _wcsnicmp(s.c_str() + pos, w, n) != 0) return false;
            if (pos + n < s.size() && (iswalnum(s[pos + n]) || s[pos + n] == L'_')) return false;
            pos += n;
            return true;
        }
        bool Sym(const wchar_t* w)
        {
            Skip();
            size_t n = wcslen(w);
            if (s.compare(pos, n, w) != 0) return false;
            pos += n;
            return true;
        }
        std::unique_ptr<Node> Make(Node::Kind k, std::unique_ptr<Node> a, std::unique_ptr<Node> b = nullptr)
        {
            auto n = std::make_unique<Node>();
            n->kind = k;
            n->a = std::move(a);
            n->b = std::move(b);
            return n;
        }
        std::unique_ptr<Node> Or()
        {
            auto l = And();
            while (Word(L"OR") || Sym(L"||")) l = Make(Node::Or, std::move(l), And());
            return l;
        }
        std::unique_ptr<Node> And()
        {
            auto l = NotE();
            while (Word(L"AND") || Sym(L"&&")) l = Make(Node::And, std::move(l), NotE());
            return l;
        }
        std::unique_ptr<Node> NotE()
        {
            if (Word(L"NOT")) return Make(Node::Not, NotE());
            Skip();
            if (pos < s.size() && s[pos] == L'!' && !(pos + 1 < s.size() && s[pos + 1] == L'='))
            {
                ++pos;
                return Make(Node::Not, NotE());
            }
            return Cmp();
        }
        std::unique_ptr<Node> Cmp()
        {
            auto l = Term();
            Skip();
            for (const wchar_t* op : {L"<=", L">=", L"==", L"!=", L"<", L">"})
                if (Sym(op))
                {
                    auto n = Make(Node::Cmp, std::move(l), Term());
                    n->op = op;
                    return n;
                }
            return l;
        }
        std::unique_ptr<Node> Term()
        {
            Skip();
            if (pos >= s.size()) Fail("the condition ends too early");
            if (Sym(L"("))
            {
                auto e = Or();
                if (!Sym(L")")) Fail("a ( is not closed");
                return e;
            }
            wchar_t c = s[pos];
            if (iswdigit(c) || ((c == L'-' || c == L'.') && pos + 1 < s.size() && iswdigit(s[pos + 1])))
            {
                size_t used = 0;
                double v = std::stod(s.substr(pos), &used);
                pos += used;
                auto n = std::make_unique<Node>();
                n->kind = Node::Num;
                n->num = v;
                return n;
            }
            if (c == L'@')
            {
                ++pos;
                std::wstring name = Ident();
                const std::wstring* text = reg.Preset(name);
                if (!text) Fail("no preset @" + Narrow(name));
                std::wstring key = Registry::Low(name);
                if (presetStack.count(key)) Fail("preset @" + Narrow(name) + " refers to itself");
                Parser sub{*text, reg, 0, presetStack, name};
                sub.presetStack.insert(key);
                auto e = sub.Or();
                sub.Skip();
                if (sub.pos < text->size()) sub.Fail("unexpected text");
                return e;
            }
            bool onTarget = false;
            size_t save = pos;
            std::wstring name = Ident();
            if (_wcsicmp(name.c_str(), L"Target") == 0 && pos < s.size() && s[pos] == L'.')
            {
                ++pos;
                onTarget = true;
                name = Ident();
            }
            const CondDef* d = reg.Find(name);
            if (!d)
            {
                pos = save;
                Fail("unknown condition '" + Narrow(name) + "'");
            }
            auto n = std::make_unique<Node>();
            n->kind = Node::Ref;
            n->def = d;
            n->onTarget = onTarget;
            Skip();
            if (pos < s.size() && s[pos] == L'(')
            {
                ++pos;
                std::wstring cur;
                int depth = 0;
                bool closed = false;
                for (; pos < s.size(); ++pos)
                {
                    wchar_t ch = s[pos];
                    if (ch == L'(') ++depth;
                    if (ch == L')' && depth-- == 0)
                    {
                        closed = true;
                        ++pos;
                        break;
                    }
                    if (ch == L',' && depth == 0)
                    {
                        n->args.push_back(TrimW(cur));
                        cur.clear();
                        continue;
                    }
                    cur += ch;
                }
                if (!closed) Fail("the ( after " + Narrow(name) + " is not closed");
                if (!TrimW(cur).empty() || !n->args.empty()) n->args.push_back(TrimW(cur));
            }
            int k = static_cast<int>(n->args.size());
            if (k < d->minArgs || k > d->maxArgs)
                Fail(Narrow(d->name) + " takes " + std::to_string(d->minArgs) + (d->maxArgs != d->minArgs ? "-" + std::to_string(d->maxArgs) : "") +
                     " argument(s), got " + std::to_string(k));
            return n;
        }
        std::wstring Ident()
        {
            Skip();
            size_t a = pos;
            while (pos < s.size() && (iswalnum(s[pos]) || s[pos] == L'_')) ++pos;
            if (a == pos) Fail("a condition name was expected");
            return s.substr(a, pos - a);
        }
        static std::wstring TrimW(const std::wstring& x)
        {
            size_t a = 0, b = x.size();
            while (a < b && iswspace(x[a])) ++a;
            while (b > a && iswspace(x[b - 1])) --b;
            return x.substr(a, b - a);
        }
    };
};
} // namespace nc
