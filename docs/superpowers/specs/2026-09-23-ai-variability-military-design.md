# CPU AI — Variability and Military Logic — Design

Date: 2026-09-23
Status: approved by the user ("write it up and start implementing")

## Goal

CPU players should be **diverse** (every game and every CPU different) and **militarily sensible**
(attack as a group and with superiority, retreat, defend proportionately, pick meaningful targets).
The economic logic (phases, prerequisites, farms, construction) stays; it is only fixed and wired up.

### What the user said
- Scope: "Variability + military" (not economy, not lobby UI).
- Difficulty and personality: "Random + config" — personality random per CPU, difficulty
  from `config.cfg` (`ai_level`, default medium), on a dedicated server `addcpu <level>`.

### Assumptions
- The AI runs only on the leader (see `docs/AI_SYSTEM.md`), so randomness in the AI does not desynchronise
  followers. Still, the AI **must not use the global `rand()`** (the simulation uses it
  in `dosimpletypes.cpp`) — it has its own generator.
- Unit strength is estimated from `.rac` data (hit points, weapon power); no new fields in `.rac`.

## Review findings addressed by the design

| # | Problem | Solution |
|---|---------|--------|
| 1 | Always `TAI_LEVEL_EASY` + `FLAVOR_AGGRESSIVE` | random personality + noise, level from config / `addcpu` |
| 2 | No randomness (buildings, scouting, unit choice) | own RNG, random choices within defined bounds |
| 3 | `attack_when_ready` is unused, attack based on `strcmp(name,"assault")` | attack driven by the military state machine and `attack_when_ready` |
| 4 | Target = first enemy in map traversal order | target scoring |
| 5 | Retaliation forever (`retaliate_enemy_pid`) | retaliation expires; triggered only with sufficient strength |
| 6 | Attack regardless of strength, units go one by one | rally point + attack with superiority + retreat |
| 7 | Defence sends the whole army | proportionate defence according to threat strength |
| 8 | Scouts head to the same point / same unit | dedicated scouts with different targets |
| 9 | `TaiSendArmyTowardPosition` overrides group pathing with individual `StartMoving` | a single movement mechanism (verify in code, fix) |

## Architecture

### New module `src/doai_logic.h` / `src/doai_logic.cpp` (no engine dependency)

Pure decision logic testable by a standalone C++ test (`make test-ai`).

- **`TAI_RNG`** — PCG32; `NextU32()`, `Uniform(a,b)`, `Chance(p)`, `Index(n)`, `PickWeighted(weights, n)`.
- **Personalities** — `TAI_PERSONALITY { const char *name; TAI_FLAVOR_PARAMS flavor; float attack_ratio; float retreat_ratio; int rally_size; float defense_commit; float scout_count; }`.
  Presets: `aggressive`, `commercial`, `calm`, `rusher`, `turtle`.
  `TAI_RollPersonality(rng)` picks a preset and adds ±10 % noise (flavor parameters clamped 0..1,
  `rally_size` ±1, min 2).
- **Strength** — `TAI_UNIT_SAMPLE { float life, dps; int x, y; bool structure, military, attacking_us; }`;
  `TAI_ArmyPower(samples, n)` = (Σ dps) × (Σ life) (Lanchester square-law estimate).
- **Decisions**
  - `TAI_ShouldAttack(my_power, enemy_power_est, my_count, pers, enemy_known)`:
    `my_count >= rally_size` and (`enemy_known` ? `my/enemy >= attack_ratio` : `my_count >= rally_size*3/2`).
  - `TAI_ShouldRetreat(my_power, enemy_power_local, pers)`: `my/enemy < retreat_ratio`.
  - `TAI_DefenseCommitCount(threat_power, my_unit_powers_sorted_by_distance, n, commit_factor)`:
    how many of the nearest units to send so that their strength ≥ `commit_factor × threat_power` (min 1, max n).
  - `TAI_TargetScore(sample, dist)`: attacking us +100, military +40, building with dps (tower) +30,
    other building +10, −dist·2, + (1−life_frac)·20 (finish off weakened ones).
  - `TAI_EnemyPowerEstimate(visible, remembered, seconds_since_seen)`: max(visible, remembered·0.5^(t/60)).
  - `TAI_RallyPoint(base, enemy_base, dist, map_w, map_h)`: a point on the base→enemy segment at distance `dist`, clamped to the map.
  - `TAI_RETALIATION` — `Hit(pid, now)`, `Active(now)` (expires after 90 s without a hit), `Clear()`.

### Changes in `src/doai.cpp` / `src/doai.h`

1. **AI player creation** — `TAI_PLAYER` takes its level from `TAI_LevelForSlot(slot)`:
   per-slot override (`player_array`, set by `addcpu <level>`), otherwise `config.ai_level`.
   Personality `TAI_RollPersonality(rng)`; RNG seed = time ^ (player_id · 0x9E3779B9).
   `TAI_STRATEGY` gets its flavor from the personality.
2. **Military state machine** `TAI_MILITARY_STATE { GATHER, ATTACK, RETREAT }` + independent defence:
   - always **defence** first: threats = visible enemy units within 12 fields of our buildings;
     sends `TAI_DefenseCommitCount` nearest units to the best-scored threat;
   - **GATHER**: unassigned units go to the rally point (`TAI_RallyPoint`, 8 fields from the base
     towards the target enemy). Transition to ATTACK when `phase.attack_when_ready` (or active retaliation)
     and `TAI_ShouldAttack`;
   - **ATTACK**: group movement to the target (best-scored visible target, otherwise the enemy
     base `initial_x/y`); at the target `StartAttacking`. Transition to RETREAT when
     `TAI_ShouldRetreat` based on strength around the army; to GATHER when the army < half of `rally_size`;
   - **RETREAT**: group movement to the rally point, on arrival (or after 20 s) GATHER.
   - Target enemy = nearest active enemy (by starting positions), during retaliation the attacker.
3. **Retaliation** via `TAI_RETALIATION` (expires), not the permanent `retaliate_enemy_pid`.
4. **Scouting** — `round(scout_count)` (1–2) dedicated scouts (their IDs are remembered);
   each gets a different random target (enemy starts + random map points), a new target on arrival.
   Scouts are not counted in the attacking army.
5. **Buildings** — `FindBuildPosition` walks a spiral from a randomly chosen corner/direction and picks
   one of the first 3 valid spots at random (keeping the edge and alley rules).
6. **Production** — instead of "always the heaviest", a weighted random choice among available units
   (weight = score^1 × (aggressivity ? prefers heavy : light)); the heavy unit quota stays.
7. **Diagnostics** — `logs <slot>` prints the personality, level, military state, strength ratio, retaliation.
8. **Config** — `ai_level` (`easy|medium|hard`, default `medium`) in `doconfig`; server `addcpu [level]`.

## Errors and edge cases

- No enemy alive / no active player → the state machine stays in GATHER.
- Zero enemy strength (invisible) → `enemy_known=false` branch (attack only with 1.5× `rally_size`).
- Division by zero in ratios → ratio against 0 = infinity (attack allowed), own 0 = 0.
- Map without `initial_x/y` (−1) → rally point = position of the first own building / unit.
- Unknown `ai_level` value in config → warning + medium.

## Testing

- **Unit (C++, `make test-ai`)**: RNG determinism for a seed, `Uniform` range, `PickWeighted` distribution;
  personalities within bounds and different for different seeds; `ArmyPower`; `ShouldAttack/Retreat` boundaries;
  `DefenseCommitCount`; `TargetScore` ordering; `EnemyPowerEstimate` decay; `RallyPoint` clamping;
  `TAI_RETALIATION` expiry.
- **Integration (headless server)**: `addcpu`×2 + `start` on `trial` and `orc_test`, ≥ 5 minutes,
  `logs on` + a new log of military transitions: no crash, both AIs reach ATTACK, at least one
  RETREAT or defence, 3 runs with different personalities / building positions.
- Client and server build without warnings in the new files.

## Out of scope

Lobby UI for selection, storehouses near resources, workers fleeing, new unit types.
