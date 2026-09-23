# Orčí rasa (orc-red / orc-blue / orc-yellow) — návrh

Datum: 2026-09-23
Stav: schváleno v konverzaci (část 1), čeká na review spec

## Cíl

Přidat do Dark Oberonu novou hratelnou rasu **orků** (zelené postavy), která je
**1:1 protějškem lidí**: každá lidská jednotka i budova má orčí ekvivalent se
stejnými animacemi, počty snímků, směry a statistikami. Mění se jen grafika a
zobrazovaná jména.

### Co řekl uživatel
- Rasa podobná orkům, zelené postavy, budovy i jednotky jako doplněk k lidem, včetně animací.
- Rozsah: 1:1 protějšek (ne nový roster, ne změna balancu).
- Grafika: restyle existujících lidských boardů přes codex.
- Barvy hráčů: 3 varianty red/blue/yellow.

### Předpoklady (moje)
- Zvuky se přebírají z lidí beze změny.
- Styl zůstává „plastic“ (hliněné/plastové 3D figurky, pohled shora šikmo), aby orci ve hře nevyčnívali.
- Interní `id` jednotek/budov v `.rac` zůstávají stejná jako u lidí (`footman`, `castle`, …) — mapy, AI a `can_build` odkazy fungují beze změn. Mění se jen `name`.

## Kritéria hotovosti

1. Existují `races/orc-red/`, `races/orc-blue/`, `races/orc-yellow/` (každá `<id>.rac` + `<id>.dat`).
2. Hra všechny tři rasy načte bez chyb v logu.
3. Všech 16 entit (5 jednotek + 11 budov) má kompletní orčí grafiku ve všech animacích, které mají lidé.
4. Na mapě lze nastavit hráči orčí rasu a odehrát hru proti lidem/AI (stavba, těžba, boj, smrt/zombie).
5. Vizuálně: postavy jsou zelení orci, týmová barva je rozpoznatelná, měřítko a ukotvení (pozice na políčku, stín) sedí s lidmi.

## Mapování entit

| id | Lidé | Orci (`name`) |
|----|------|---------------|
| peasant | Peasant | Peon |
| footman | Footman | Grunt |
| moleman | Moleman | Sapper (goblin kopáč) |
| catapult | Catapult | Bone Catapult |
| airship | Airship | Goblin Zeppelin |
| castle | Castle | Great Hall |
| townhall | Town Hall | Chieftain Hut |
| citadel | Citadel | Stronghold |
| fort | Fort | War Fort |
| barracks | Barracks | War Camp |
| farm | Farm | Pig Farm |
| shed | Shed | Lumber Hut |
| manufactory | Manufactory | Goblin Workshop |
| tower | Tower | Watchtower |
| cannontower | Cannon Tower | Skull Tower |
| wall | Wall | Palisade |

(Přesný seznam entit a animací se bere z výstupu `compose` nad `human-red.rac`; tabulka výše je jen návrh jmen.)

## Architektura / pipeline

Stavíme na existující pipeline v `.cursor/skills/dark-oberon-dat/scripts/`
(`race_pipeline.sh compose → boards → unboards → finalize`). Nový je jen krok
generování přes codex a krok barevných variant.

```
human-red.dat ──compose──► sheets ──boards──► boards/*.png (1024², lidé)
                                                   │
             design sheet (codex, schválí uživatel)┤
                                                   ▼
                                   codex restyle (1 volání / board)
                                                   ▼
                                    boards_orc/*.png ──post-process (alpha z originálu)
                                                   ▼
                                   unboards ──► finalize ──► races/orc-red/
                                                   ▼
                                   recolor týmové masky ──► orc-blue, orc-yellow
```

### 1. Čistý pracovní adresář
Stávající `ai-working/race-pipeline-human-red/` má v `_pipeline_state.json` absolutní cesty
z jiného umístění repa → spustit `compose` znovu do `ai-working/race-pipeline-orc/`.

### 2. Design sheet na entitu
Codex vygeneruje pro každou entitu jeden referenční obrázek (pohled zepředu/zezadu,
detail týmově zbarvených částí) ve stylu plastic, s referencemi `orcs-racs/*.png`
a lidským boardem entity. Uživatel design sheety schvaluje (u jednotek jednotlivě,
u budov po skupinách). Uloženo v `ai-working/race-pipeline-orc/design/<entita>.png`.

### 3. Restyle boardů
Nový skript `run_codex_board_batch.py` (vedle `run_openai_board_batch.py`):
- pro každý board sestaví prompt: vstupní lidský board + design sheet + pravidla
  („zachovej siluetu, pózu, pozici v buňce, stín a bílé pozadí; týmové části
  čistě červeně; nic nepřidávej mimo buňky“),
- spouští `codex exec` (vzor `~/.claude/skills/iso-sprite/tools/codex_run.py`:
  ověření, že výstupní PNG vznikl, retry),
- paralelně (`--parallel N`), idempotentně (přeskočí hotové boardy), stav v JSON,
- `--dry-run` a `--only <entita>`.

### 4. Post-processing (deterministický)
- zmenšení/zarovnání na rozměr boardu,
- alpha maska z původního lidského snímku (lehce rozšířená dilatací), aby zůstalo
  ukotvení, stopa a výběrová oblast,
- validace: rozměry, pokrytí masky, nenulový obsah v každé buňce; selhané boardy
  do reportu k přegenerování.

### 5. Finalize
`race_pipeline.sh finalize --race-id orc-red --race-name "Plastic Orcs - Red"`;
`generate_rac.py` převezme lidský `.rac` a přepíše `name` jednotek/budov podle
mapování (nová volba / mapovací JSON).

### 6. Barevné varianty
Týmová barva se v lidských rasách liší přímo v `.dat` (tři různé soubory).
Nový skript `recolor_race.py`: najde pixely týmové barvy (odstín červené, saturace
nad prahem) a přemapuje odstín na modrou/žlutou; výsledek zabalí jako
`orc-blue.dat` / `orc-yellow.dat` + `.rac` s upraveným `name`. Přesná metoda se
kalibruje v pilotu porovnáním `human-red` vs `human-blue`.

## Pilot (povinný před hromadným během)

Jen **Grunt (footman)**: design sheet + boardy `stay`, `move`, `attack`, `picture`,
`zombie` → finalize jako `orc-red` (ostatní entity dočasně lidská grafika) →
testovací mapa `orc-red` vs `human-blue` → kontrola měřítka, animace, ukotvení,
týmové barvy. Pokračuje se až po schválení uživatelem.

## Chyby a rizika

| Riziko | Opatření |
|--------|----------|
| Nekonzistence mezi snímky/boardy | design sheet jako společná reference; přegenerování jednotlivých boardů |
| Codex posune postavu v buňce | alpha z originálu + validace pokrytí |
| Týmová barva „rozlitá“ do zelené kůže | prompt vyžaduje čistou červenou; recolor jen nad prahem saturace a v rozsahu odstínu červené |
| Codex selže / nevytvoří soubor | retry, stav v JSON, pokračování od posledního hotového |
| Budovy vypadají „lidsky“ | vlastní design sheety pro budovy (dřevo, kůže, kosti, hroty) |

## Testování

- pytest pro deterministické části: post-process (alpha/rozměry), recolor (odstín týmové barvy se mění, zelená ne), mapování jmen v `generate_rac.py`.
- Round-trip: `unpack` → `pack` nového `.dat` → `unpack` dává stejné soubory.
- Manuální: hra načte rasu, pilotní mapa, kontrola logu.

## Mimo rozsah

Nové zvuky, změna balancu, nové jednotky/mechaniky, nové animace nad rámec lidských.
