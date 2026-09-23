# CPU AI — variabilita a vojenská logika — návrh

Datum: 2026-09-23
Stav: schváleno uživatelem („sepiš a pusť se do implementace“)

## Cíl

CPU hráči mají být **různorodí** (každá hra i každý CPU jiný) a **vojensky rozumní**
(útočí se skupinou a s převahou, ustupují, brání se přiměřeně, vybírají smysluplné cíle).
Ekonomická logika (fáze, prerekvizity, farmy, stavba) zůstává, jen se opraví a napojí.

### Co řekl uživatel
- Rozsah: „Variabilita + vojsko“ (ne ekonomika, ne lobby UI).
- Obtížnost a osobnost: „Náhodně + config“ — osobnost náhodně na CPU, obtížnost
  z `config.cfg` (`ai_level`, výchozí medium), na dedikovaném serveru `addcpu <level>`.

### Předpoklady
- AI běží jen na lídrovi (viz `docs/AI_SYSTEM.md`), náhoda v AI tedy nedesynchronizuje
  followery. Přesto AI **nesmí používat globální `rand()`** (simulace ho používá
  v `dosimpletypes.cpp`) — má vlastní generátor.
- Síla jednotky se odhaduje z dat `.rac` (životy, síla zbraně), žádné nové údaje v `.rac`.

## Nálezy review, které návrh řeší

| # | Problém | Řešení |
|---|---------|--------|
| 1 | Vždy `TAI_LEVEL_EASY` + `FLAVOR_AGGRESSIVE` | náhodná osobnost + šum, level z configu / `addcpu` |
| 2 | Žádná náhoda (stavby, průzkum, výběr jednotek) | vlastní RNG, náhodné volby v definovaných mezích |
| 3 | `attack_when_ready` se nepoužívá, útok podle `strcmp(name,"assault")` | útok řídí vojenský automat a `attack_when_ready` |
| 4 | Cíl = první nepřítel v pořadí průchodu mapou | skórování cílů |
| 5 | Odveta navždy (`retaliate_enemy_pid`) | odveta vyprší; spouští se jen se sílou |
| 6 | Útok bez ohledu na sílu, jednotky chodí po jedné | shromaždiště + útok s převahou + ústup |
| 7 | Obrana posílá celou armádu | přiměřená obrana podle síly hrozby |
| 8 | Průzkumníci míří na stejný bod / stejná jednotka | vyčlenění průzkumníci s různými cíli |
| 9 | `TaiSendArmyTowardPosition` přebije skupinovou cestu jednotlivými `StartMoving` | jen jeden mechanismus přesunu (ověřit v kódu, opravit) |

## Architektura

### Nový modul `src/doai_logic.h` / `src/doai_logic.cpp` (bez závislosti na enginu)

Čistá rozhodovací logika testovatelná samostatným C++ testem (`make test-ai`).

- **`TAI_RNG`** — PCG32; `NextU32()`, `Uniform(a,b)`, `Chance(p)`, `Index(n)`, `PickWeighted(weights, n)`.
- **Osobnosti** — `TAI_PERSONALITY { const char *name; TAI_FLAVOR_PARAMS flavor; float attack_ratio; float retreat_ratio; int rally_size; float defense_commit; float scout_count; }`.
  Presety: `aggressive`, `commercial`, `calm`, `rusher`, `turtle`.
  `TAI_RollPersonality(rng)` vybere preset a přidá šum ±10 % (flavor parametry clamp 0..1,
  `rally_size` ±1, min 2).
- **Síla** — `TAI_UNIT_SAMPLE { float life, dps; int x, y; bool structure, military, attacking_us; }`;
  `TAI_ArmyPower(samples, n)` = (Σ dps) × (Σ life) (Lanchesterův čtvercový odhad).
- **Rozhodnutí**
  - `TAI_ShouldAttack(my_power, enemy_power_est, my_count, pers, enemy_known)`:
    `my_count >= rally_size` a (`enemy_known` ? `my/enemy >= attack_ratio` : `my_count >= rally_size*3/2`).
  - `TAI_ShouldRetreat(my_power, enemy_power_local, pers)`: `my/enemy < retreat_ratio`.
  - `TAI_DefenseCommitCount(threat_power, my_unit_powers_sorted_by_distance, n, commit_factor)`:
    kolik nejbližších jednotek poslat, aby jejich síla ≥ `commit_factor × threat_power` (min 1, max n).
  - `TAI_TargetScore(sample, dist)`: útočí na nás +100, vojenská +40, stavba s dps (věž) +30,
    ostatní stavba +10, −dist·2, + (1−life_frac)·20 (dorazit oslabené).
  - `TAI_EnemyPowerEstimate(visible, remembered, seconds_since_seen)`: max(visible, remembered·0.5^(t/60)).
  - `TAI_RallyPoint(base, enemy_base, dist, map_w, map_h)`: bod na úsečce základna→nepřítel ve vzdálenosti `dist`, oříznutý do mapy.
  - `TAI_RETALIATION` — `Hit(pid, now)`, `Active(now)` (vyprší po 90 s bez zásahu), `Clear()`.

### Změny v `src/doai.cpp` / `src/doai.h`

1. **Tvorba AI hráče** — `TAI_PLAYER` si vezme level podle `TAI_LevelForSlot(slot)`:
   per-slot override (`player_array`, nastaví `addcpu <level>`) jinak `config.ai_level`.
   Osobnost `TAI_RollPersonality(rng)`; RNG seed = čas ^ (player_id · 0x9E3779B9).
   `TAI_STRATEGY` dostane flavor z osobnosti.
2. **Vojenský automat** `TAI_MILITARY_STATE { GATHER, ATTACK, RETREAT }` + nezávislá obrana:
   - vždy nejdřív **obrana**: hrozby = viditelné nepřátelské jednotky do 12 polí od našich staveb;
     pošle `TAI_DefenseCommitCount` nejbližších jednotek na nejlépe skórovanou hrozbu;
   - **GATHER**: nevyčleněné jednotky jdou na shromaždiště (`TAI_RallyPoint`, 8 polí ze základny
     k cílovému nepříteli). Přechod do ATTACK, když `phase.attack_when_ready` (nebo aktivní odveta)
     a `TAI_ShouldAttack`;
   - **ATTACK**: skupinový přesun k cíli (nejlépe skórovaný viditelný cíl, jinak nepřátelská
     základna `initial_x/y`); u cíle `StartAttacking`. Přechod do RETREAT, když
     `TAI_ShouldRetreat` podle síly v okolí armády; do GATHER, když armáda < polovina `rally_size`;
   - **RETREAT**: skupinový přesun na shromaždiště, po příchodu (nebo 20 s) GATHER.
   - Cílový nepřítel = nejbližší aktivní nepřítel (podle startovních pozic), při odvetě útočník.
3. **Odveta** přes `TAI_RETALIATION` (vyprší), ne trvalé `retaliate_enemy_pid`.
4. **Průzkum** — `round(scout_count)` (1–2) vyčlenění průzkumníci (pamatuje si jejich ID);
   každý dostane jiný náhodný cíl (nepřátelské starty + náhodné body mapy), nový cíl po dojití.
   Průzkumníci se nepočítají do útočné armády.
5. **Stavby** — `FindBuildPosition` projde spirálu z náhodně zvoleného rohu/směru a vybere
   náhodně jedno z prvních 3 platných míst (se zachováním pravidel okraje a uliček).
6. **Výroba** — místo „vždy nejtěžší“ vážený náhodný výběr mezi dostupnými jednotkami
   (váha = skóre^1 × (aggressivity ? preferuje těžké : lehké)); kvóta těžkých jednotek zůstává.
7. **Diagnostika** — `logs <slot>` vypíše osobnost, level, vojenský stav, poměr sil, odvetu.
8. **Config** — `ai_level` (`easy|medium|hard`, výchozí `medium`) v `doconfig`; server `addcpu [level]`.

## Chyby a okrajové případy

- Žádný nepřítel naživu / žádný aktivní hráč → automat zůstává v GATHER.
- Nulová síla nepřítele (neviditelný) → `enemy_known=false` větev (útok jen s 1,5× `rally_size`).
- Dělení nulou v poměrech → poměr vůči 0 = nekonečno (útok povolen), vůči vlastní 0 = 0.
- Mapa bez `initial_x/y` (−1) → shromaždiště = pozice první vlastní stavby / jednotky.
- Neznámá hodnota `ai_level` v configu → warning + medium.

## Testování

- **Unit (C++, `make test-ai`)**: RNG determinismus pro seed, rozsah `Uniform`, rozložení `PickWeighted`;
  osobnosti v mezích a různé pro různé seedy; `ArmyPower`; `ShouldAttack/Retreat` hranice;
  `DefenseCommitCount`; `TargetScore` pořadí; `EnemyPowerEstimate` rozpad; `RallyPoint` ořez;
  `TAI_RETALIATION` vypršení.
- **Integrace (headless server)**: `addcpu`×2 + `start` na `trial` a `orc_test`, ≥ 5 minut,
  `logs on` + nový log vojenských přechodů: bez pádu, obě AI dosáhnou ATTACK, aspoň jeden
  RETREAT nebo obrana, 3 běhy s různými osobnostmi / pozicemi staveb.
- Build klienta i serveru bez warningů v nových souborech.

## Mimo rozsah

Lobby UI pro výběr, sklady u surovin, útěk dělníků, nové typy jednotek.
