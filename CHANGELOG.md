# Changelog

## 1.0.0 - 2026-11-18 - untested

### Added
- First release. It contains everything built and tested in 0.1.0, listed here and kept as history below:
- The condition language (include/NorseConditions.hpp): AND/OR/NOT, brackets, comparisons, Target., @presets; errors with positions; unreadable values are false.
- Norse conditions: Health, MaxHealth, HealthPercent, IsDead, HasTag, UnitType, Field, Team, IsAlly, IsEnemy, InFight, Map, Chance, Distance.
- Presets from conditions/*.ini (NorseManifest order), checked at start; exports NC_Compile, NC_Eval; console nc.eval, nc.list, nc.reload.
- Conditions MainHand(text), OffHand(text), Equipped(text): what the unit carries (each slot's item followed to its item asset). Tested in game: docs-equipment-test.txt.
- main.dll carries a Windows version resource (file and product version 1.0.0, shown in its Properties > Details).

### Fixed
- World conditions (Map, InFight, Chance) read as false when the moment has no unit (a map load): they are now asked without one (AddWorld). 3 offline tests added.

## 0.1.0 - 2026-11-17 (tested offline, 27 checks, and in game: TEST-RESULT-0.1.0.txt)
### Added
- The condition language (include/NorseConditions.hpp): AND/OR/NOT, brackets, comparisons, Target., @presets; errors with positions; unreadable values are false.
- Norse conditions: Health, MaxHealth, HealthPercent, IsDead, HasTag, UnitType, Field, Team, IsAlly, IsEnemy, InFight, Map, Chance, Distance.
- Presets from conditions/*.ini (NorseManifest order), checked at start; exports NC_Compile, NC_Eval; console nc.eval, nc.list, nc.reload.

### Fixed (2026-11-18)
- World conditions (Map, InFight, Chance) read as false when the moment has no unit (a map load): they are now asked without one (AddWorld). 3 offline tests added.

### Added (2026-11-18)
- Conditions MainHand(text), OffHand(text), Equipped(text): what the unit carries (each slot's item followed to its item asset). Tested in game: docs-equipment-test.txt.
