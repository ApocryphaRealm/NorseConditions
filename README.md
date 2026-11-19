# NorseConditions

One "only when..." language for every Norse framework (NORSE: Oath of Blood, UE4SS v4.0.0), so pack authors learn a
single condition syntax instead of one per framework - the problem Skyrim's frameworks ran into. Modelled on Open
Animation Replacer's conditions and condition presets. Survey: `RESEARCH-more-frameworks.md` (#3).

```
InFight AND HealthPercent < 50 AND (HasTag(Brigand) OR UnitType(Outcast_*)) AND NOT Target.IsAlly
```

AND / OR / NOT and brackets (also && || !), comparisons, numbers, `Target.` for the other unit, `@Preset` for a named
condition. A condition that cannot be read (no other unit, no unit type) is false - never an error at run time; a
mistake in the text (unknown name, wrong argument count, unclosed bracket, preset loop) is an error with its position.
Author guide with every condition: `package\conditions\README.txt`.

## Parts

- `include/NorseConditions.hpp` - the language: game-free, header-only (registry, presets, parser, evaluator).
- `src/main.cpp` - the UE4SS mod: registers what each condition reads in Norse (Health, MaxHealth via the game's
  IHealth::GetMaxHealth, HealthPercent, IsDead, HasTag via NorseTagger, UnitType, Field, Team, IsAlly, IsEnemy,
  InFight via NorseEvents, Map, Chance, Distance), loads presets from `conditions\*.ini` in NorseManifest order, and
  exports `NC_Compile(text, error, size)` and `NC_Eval(handle, subject, target)` for the other frameworks.
  Console: `nc.eval <condition>` (result for every unit on the map), `nc.list`, `nc.reload`.
- `tools/conditions_test.cpp` - the offline test: `xmake build conditions_test && xmake run conditions_test` (27 checks).

## Status

0.1.0 tested offline and in game (TEST-RESULT-0.1.0.txt). No framework calls NC_Eval yet - AnimSwap, the HUD and
NorseImpact are its first users. Distance, Chance and Team were not exercised in the first test. Not yet: equipped item
and current mission conditions (how to read the mission is still to be found in game).

Build: `xmake f -p windows -a x64 -m release --toolchain=msvc -y`, then `xmake build main` (needs `../NorsePatcher`,
`../NorseManifest` and `../NorseEvents` beside it).

## Licence

GPL-3.0-or-later (see LICENSE).
