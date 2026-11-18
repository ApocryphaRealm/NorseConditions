# Changelog

## 0.1.0 - 2026-11-17 (tested offline, 27 checks, and in game: TEST-RESULT-0.1.0.txt)
### Added
- The condition language (include/NorseConditions.hpp): AND/OR/NOT, brackets, comparisons, Target., @presets; errors with positions; unreadable values are false.
- Norse conditions: Health, MaxHealth, HealthPercent, IsDead, HasTag, UnitType, Field, Team, IsAlly, IsEnemy, InFight, Map, Chance, Distance.
- Presets from conditions/*.ini (NorseManifest order), checked at start; exports NC_Compile, NC_Eval; console nc.eval, nc.list, nc.reload.
