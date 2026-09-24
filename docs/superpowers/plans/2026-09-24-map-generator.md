# Generátor map pro 4 hráče — implementační plán

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `generate_map.py --seed N` vytvoří hratelnou mapu 160×160 pro 4 hráče se správně ukončenou vodou a plošinami, zdroji a 6 rasami.

**Architecture:** Buňková mřížka 32×32 (fragment 5×5 polí). Rozvržení pracuje s maskami oblastí (voda W, plošina P) na buňkách; autotiling převede okraje oblastí na fragmenty podle tabulek ze spec. Zdroje a starty v polích. Kontroly: sousedství fragmentů proti ručním mapám, průchodnost, bilance, `validate_map.py`.

**Tech Stack:** Python 3 (numpy, Pillow, pytest), existující `dark_oberon_maplib.py`, `do_dat_tool.py` (náhled z textur), headless server.

**Spec:** `docs/superpowers/specs/2026-09-24-map-generator-design.md`

## Global Constraints

- Mapa 160×160, 32×32 buněk, fragment 5×5; souřadnice fragmentu = pole (násobky 5); E = x+5, S = y+5.
- Tabulky okrajových dílů přesně podle spec (voda: `coast_*`, plošina: `rocks_*`, nájezdy `rocks_*_end_*`).
- Skála nikdy nesousedí s vodou (≥ 1 buňka trávy mezi nimi, i diagonálně).
- Průchozí terén pro pozemní jednotky: vrstvy 10–25 (tráva 10, bažina 20, skalnatá tráva 25).
- Zdroje: `goldmine` 4×4, `coal` 4×4, `forest` 3×3; jen na průchozím terénu, bez překryvu, mimo radnice.
- Rasy: `human-red`, `human-blue`, `human-yellow`, `orc-red`, `orc-blue`, `orc-yellow`; `max_count 4`; 4 starty.
- Výstup deterministický pro daný seed (`random.Random(seed)`, `numpy.random.default_rng(seed)`).
- Skripty v `.cursor/skills/dark-oberon-map/scripts/`, testy v `tests/mapgen/`.
- Commity končí `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.

## Review Focus

1. **Oblast s buňkou, která má okolí na protilehlých stranách** (úzký krček 1 buňka) → po vyhlazení nesmí existovat. Test: `test_smooth_removes_thin_necks` (Task 1).
2. **Diagonální sedlo** (dvě oblasti se dotýkají rohem) → odstraněno. Test: `test_smooth_removes_saddles` (Task 1).
3. **Plošina u vody** → vynucený odstup. Test: `test_plateau_keeps_distance_from_water` (Task 3).
4. **Start obklíčený plošinou / vodou** → souvislost vynucena. Test: `test_all_starts_connected` (Task 3).
5. **Ruční mapa `sunnybay` musí projít kontrolou sousedství** (jinak je kontrola chybná). Test: `test_adjacency_check_accepts_handmade_map` (Task 2).

---

### Task 1: `mapgen_tiles.py` — autotiling a vyhlazení
**Produces:** `WATER_TABLE`, `PLATEAU_TABLE` (dict: frozenset stran okolí → název), `outline_fragment(mask, cx, cy, table) -> str|None` (None = vnitřek), `smooth_region(mask) -> mask` (otevření 3×3, odstranění krčků a sedel, iterovat do ustálení), `place_ramps(mask, rng, n) -> dict[(cx,cy)] -> name`, `fragment_ids(sch_path, segment) -> dict[name] -> index`, `ug_name(name) -> str` (`coast_*`→`ug_coast_*`, `sea`→`ug_sea`, jinak `ug_grass`).
Testy (`tests/mapgen/test_tiles.py`): každý řádek obou tabulek na syntetické masce (čtverec 6×6 buněk + výřez pro vnitřní rohy), `test_smooth_removes_thin_necks`, `test_smooth_removes_saddles`, nájezdy dávají správnou dvojici a jen na rovné hraně délky ≥ 4, `fragment_ids` obsahuje všech 80 dílů segmentu 1 a `ug_*` v segmentu 0.

### Task 2: `map_check.py` — kontroly
**Produces:** `learn_adjacency(map_paths, sch) -> set[(a, dir, b)]`, `check_adjacency(frag_grid, allowed) -> list[str]`, `walkable_grid(frag_grid, sch) -> bool[160,160]` (+ zdroje/budovy jako překážky), `check_connectivity(walk, starts) -> list[str]`, `check_resources(sources, starts) -> list[str]`, CLI `map_check.py maps/x.map` (exit 1 při chybách).
Pravidlo sousedství: dvojice je OK, pokud je v naučené množině z `sunnybay`, `virgin_editor`, `trial`, nebo jsou oba díly z {`grass`, `sea`} a stejné.
Testy: `test_adjacency_check_accepts_handmade_map` (sunnybay, naučeno z ostatních dvou + sebe), `test_adjacency_check_flags_bad_pair` (`coast_n` vedle `rocks_e`), souvislost na syntetické mřížce, bilance zdrojů.

### Task 3: `mapgen_layout.py` — rozvržení
**Produces:** `Layout(water, plateau, ramps, starts)`, `make_layout(seed, cells=32) -> Layout`.
Kroky: starty (kvadranty, jitter ±3 buňky, min. 18 buněk od sebe, 2 buňky od okraje), volné zóny 5×5 buněk kolem startů; střední jezero (noise blob 6–9 buněk) + 2–3 menší (3–5 buněk) mimo volné zóny; vyhladit; plošiny 3–5 (obdélníky 4–7 buněk, případně unie dvou), vyhladit, odstup od vody ≥ 1 (8-okolí); nájezdy 1–2 na plošinu; souvislost startů (BFS po buňkách mimo W a obrys P + nájezdy), při nesouvislosti odebrat plošinu nebo zmenšit jezero a opakovat.
Testy: deterministika pro seed, `test_plateau_keeps_distance_from_water`, `test_all_starts_connected`, starty mimo W/P, min. vzdálenosti.

### Task 4: `mapgen_resources.py` — zdroje
**Produces:** `place_resources(layout, walk, rng) -> list[(id, x, y, life, amount)]`.
U každého startu: 1× goldmine 10–15 polí od radnice (35 000), 1× coal 12–18 polí (8 000), 2–3 lesní shluky (8–14 lesů po 3 polích) 8–20 polí; střed/plošiny: 2–3 sporné goldmine (40 000); rozptýlené lesy do celkem ~350. Bilance: počet zlata/uhlí/lesa v okruhu 25 polí od každého startu ±10 %.
Testy: bez překryvu, jen průchozí, bilance, nic v okruhu 6 polí od startu.

### Task 5: `generate_map.py` + `render_map.py` + integrace
Zápis `.map` (hlavička, Players se 6 rasami a sadou radnice + 4 dělníci + 2 vojáci, SchemeRace se zdroji, 3 segmenty: seg0 zrcadlo `ug_*`, seg1 obsah, seg2 fragment 0; Layers/Objects count 0), `--preview`. Integrace: seedy 1–3 → `map_check` + `validate_map.py` OK, render, headless server `addcpu`×4 na nejlepší mapě 180 s bez `Err:` a AI s dělníky těží; commit nejlepší mapy jako `maps/four_lakes.map`.
