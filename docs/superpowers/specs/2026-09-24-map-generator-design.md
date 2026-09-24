# Generátor map pro 4 hráče — návrh

Datum: 2026-09-24
Stav: schváleno uživatelem („sepiš a pusť se do toho“)

## Cíl

Skript `generate_map.py`, který ze `--seed` vygeneruje hratelnou mapu `.map` pro schéma
`plastic`: **správně ukončená voda** (pobřeží), **správně ukončená vyvýšená místa** (skalní
obrysy plošin s nájezdy), rozumně rozmístěné lesy, zlato a uhlí, 4 startovní pozice.

### Co řekl uživatel
- 4 hráči, 4 spawn pointy; „funkční, smysluplně plné mapy“.
- Velikost 160×160; krajina „jezera + hřebeny“; **vyvážená, ne symetrická**; rasy lidé + orci.

### Předpoklady
- Generujeme jen segment 1 (zem) s obsahem; segment 0 (podzemí) zrcadlí vodu díly `ug_*`,
  segment 2 (vzduch) je celý fragment 0.
- Okrajové díly mapy (`grass_border_*`) nejsou nutné — originální mapy mají na okrajích i obyčejnou trávu.
- Skály nikdy nesousedí s vodou (mezi nimi vždy ≥ 1 buňka trávy) → nepotřebujeme díly `cliff_*`
  a `rocks_*_coast_*`.

## Model fragmentů (ověřeno na `sunnybay`, `virgin_editor`, `trial`)

Mapa = mřížka 32×32 buněk (fragment 5×5 polí). Souřadnice fragmentu = (x, y) v polích, násobky 5;
E = x+5, S = y+5.

**Voda** — oblast W (buňky vody). Okrajová buňka W (má souš mezi 8 sousedy) dostane díl podle
toho, kde je souš:

| souš | díl | | souš | díl |
|---|---|---|---|---|
| N | `coast_n` | | N+E | `coast_wn` |
| W | `coast_e` | | N+W | `coast_en` |
| S | `coast_s` | | S+W | `coast_es` |
| E | `coast_w` | | S+E | `coast_ws` |
| jen diag NW | `coast_ne` | | jen diag NE | `coast_nw` |
| jen diag SW | `coast_se` | | jen diag SE | `coast_sw` |

Vnitřek W = `sea`, souš = `grass`.

**Plošina** — oblast P. Okrajová buňka P dostane skalní díl podle strany, kde je okolí:

| okolí | díl | | okolí | díl |
|---|---|---|---|---|
| N | `rocks_s` | | N+W | `rocks_sw` |
| S | `rocks_n` | | N+E | `rocks_se` |
| W | `rocks_w` | | S+W | `rocks_nw` |
| E | `rocks_e` | | S+E | `rocks_ne` |
| jen diag NW | `rocks_ws` | | jen diag NE | `rocks_es` |
| jen diag SW | `rocks_wn` | | jen diag SE | `rocks_en` |

Vnitřek P = `grass`. **Nájezd** = dvojice sousedních rovných okrajových buněk nahrazená koncovými díly:
horní hrana `rocks_s_end_e` + `rocks_s_end_w` (zleva doprava), dolní `rocks_n_end_e` + `rocks_n_end_w`,
levá `rocks_w_end_n` + `rocks_w_end_s` (shora dolů), pravá `rocks_e_end_n` + `rocks_e_end_s`.

**Omezení tvaru:** oblast nesmí mít buňku s okolím na dvou protilehlých stranách ani „sedlo“
(diagonálně dotčené buňky) — oblasti se vyhladí morfologickým otevřením 3×3 a odstraní se
diagonální sedla.

## Architektura

`.cursor/skills/dark-oberon-map/scripts/`:
- `mapgen_tiles.py` — tabulky výše, `outline_fragment(region, x, y) -> name|None`, převod názvů na
  indexy fragmentů ze `.sch`, zrcadlení do `ug_*`.
- `mapgen_layout.py` — rozvržení: starty (kvadranty + jitter, ≥ 90 polí od sebe), jezera (noise
  blob + střední jezero), plošiny (obdélníková unie ≥ 3×3 buněk), volné zóny kolem startů,
  nájezdy; zajištění souvislosti (BFS po průchozím terénu, případně prokopání průchodu).
- `mapgen_resources.py` — zdroje: u každého startu zlato (10–15 polí), 2–3 lesní shluky, uhlí;
  2–3 sporné zlaté doly; bez překryvů, jen na trávě; bilance ±10 %.
- `generate_map.py` — CLI `--seed --size 160 --players 4 -o maps/<name>.map [--preview out.png]`;
  zapisuje hlavičku, `<Players>` (6 ras: human/orc × red/blue/yellow, sada: radnice, 4 dělníci,
  2 vojáci, suroviny 1500 1000 1000), `<SchemeRace>` se zdroji, 3 segmenty.
- `map_check.py` — kontroly: (1) každé sousedství fragmentů (E/S) je v množině dvojic z ručních map
  nebo mezi „grass/sea“ základem; (2) starty na průchozím terénu a vzájemně dosažitelné;
  (3) zdroje na trávě, bez překryvu, bilance; (4) `validate_map.py` projde.
- `render_map.py` — náhled ze skutečných textur `schemes/plastic.dat` (izometricky), zdroje a starty jako značky.

## Chyby a okrajové případy
- Nepodaří se rozmístit starty/zdroje → nový pokus s odvozeným seedem (max 20), jinak chyba.
- Oblast se po vyhlazení rozpadne na nic → vynechat.
- Nesouvislá mapa → prokopat 3 buňky široký průchod (odebrat plošinu/vodu v cestě).

## Testování
- pytest: tabulky obrysů (každý případ), vyhlazení (žádné protilehlé okolí, žádná sedla),
  nájezdy, zrcadlení `ug_*`, souvislost, zdroje bez překryvu, deterministický výstup pro seed,
  kontrola sousedství na ruční mapě `sunnybay` (musí projít = kalibrace kontroly).
- Integrace: 3 seedy → `map_check` OK, render k vizuální kontrole, headless server s 4× `addcpu`
  načte mapu a běží (AI těží/staví), nakonec test uživatele ve hře.

## Mimo rozsah
Útesy (skála u vody), řeky, ostrovy, dekorace (`Objects`), vrstvy (`Layers`), symetrické mapy.
