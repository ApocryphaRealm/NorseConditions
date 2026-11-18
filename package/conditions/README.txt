NorseConditions - "only when..." for every Norse framework
==========================================================

A condition is one line of text. Every framework that takes conditions reads the same language:

    InFight AND HealthPercent < 50 AND (HasTag(Brigand) OR UnitType(Outcast_*)) AND NOT Target.IsAlly

AND, OR, NOT and brackets (also &&, ||, !); comparisons < <= > >= == !=; numbers. A condition on its own (IsAlly) is
true when it holds. Target. in front reads the other unit (the attacker, the victim, the lock-on target). A condition
that cannot be read - no other unit, no unit type - counts as false.

Conditions (v0.1):
  Health, MaxHealth, HealthPercent (0-100), IsDead
  HasTag(name)          NorseTagger gave the unit or its unit type that tag
  UnitType(pattern)     the unit type's name, * wildcards: UnitType(Outcast_*)
  Field(field, text)    a field of the unit type contains text: Field(Profiles, Brigands) is the faction
  Team, IsAlly, IsEnemy (team 0 is the player's side)
  InFight               a fight is on (NorseEvents)
  Map(text)             the current map's path contains text
  Chance(n)             true n percent of the times it is asked
  Distance              to the other unit, in cm

Presets: name a condition once in a file in this folder, then use it anywhere as @name:

    LowHealth = HealthPercent < 35
    HurtBrigand = @LowHealth AND HasTag(Brigand)

Console: nc.eval <condition> logs the result for every unit on the map; nc.list lists conditions and presets;
nc.reload re-reads this folder. A preset that does not compile is reported at game start.
