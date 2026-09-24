# Generování ras přes codex — playbook (zkušenosti z orků, 2026-09)

Souhrn toho, co při tvorbě orčí rasy (`races/orc-{red,blue,yellow}`) fungovalo, co ne a jak
postupovat příště — včetně úplně nové rasy, která nebude 1:1 kopií lidí.

Nástroje: `.cursor/skills/dark-oberon-dat/scripts/` (`race_pipeline.sh`, `run_codex_board_batch.py`,
`board_postprocess.py`, `attack_anim.py`, `recolor_race.py`, `validate_race.py`), testy
`tests/race_pipeline/` (`python3 -m pytest tests/race_pipeline`).

---

## 1. Postup, který se osvědčil

1. **Styl zamknout předem.** Všechno musí vypadat jako vymodelované z modelíny (matná hmota,
   zaoblené tvary, světlo zleva shora, žádné obrysy ani pixel-art). Pravidla jsou v
   `codex_prompts.py:STYLE_RULES` a jdou do každého promptu.
2. **Pilot: jedna jednotka + jedna budova**, až po schválení zbytek. Pilot odhalil většinu
   problémů (portrét, tvar budov, stavební fáze, bílé lemy, TGA patička).
3. **Design sheet na entitu → schválení uživatelem → teprve pak restyle** (`design`, `approve`,
   `restyle`). Schvalování po skupinách (jednotky / hlavní budovy / výroba / obrana).
4. **Restyle existujících boardů 1:1** (lidská grafika jako předloha): codex dostane lidský board
   + design sheet a přemaluje každý sprite na stejném místě, ve stejné póze a velikosti.
5. **Deterministický post-process:** alfa z lidského originálu (sprite sedí přesně do buňky,
   kotva a stín zůstanou), validace `coverage` / `spill` / `unchanged`, náhled `review/`.
6. **Neúspěšné boardy** přegenerovat `--force` (max 3×); když je výsledek vizuálně OK a jen
   neprojde prahem (typicky malé objekty se září), přijmout vědomě `board_postprocess.py --accept`
   (pamatuje se v `_accepted.json`).
7. **Týmová barva:** generovat jen červenou variantu (týmové části „čistě červeně“),
   modrou/žlutou dělat deterministicky `race_pipeline.sh variants` (kalibrované mapování
   `team_blue.json`, `team_yellow.json`). Zelená kůže / hnědé dřevo zůstanou.
8. **Kontrola v herní velikosti** (64 px sprity na trávě `(96,128,64)` vedle lidí) — v plném
   rozlišení vypadá vše dobře, problémy (lemy, díry, měřítko) jsou vidět až takhle.
9. **Validace a engine:** `validate_race.py races/<id> --reference races/human-red`, pak
   headless server (`make server` v dočasné kopii `src/`, `addcpu`×2, `start`) — musí dojít
   na `Update: Running` bez `Err:`.
10. **Testovací mapa pro novou rasu** (jednorázová, necommitovat): kopie `maps/trial.map` s `name "<rasa>"
    místo `"human-red"`; pro kontrolu animací boje postavit obě armády ~8 polí od sebe
    (`start_point_0/1` blízko, v `<Units>` jen vojáci) — boj začne hned po startu.

## 2. Co nefungovalo a jak se to řeší

| Problém | Příčina | Řešení (už v nástrojích) |
|---|---|---|
| Hra nenačetla rasu: `Error reading TGA data` | Pillow zapisuje TGA 2.0 s patičkou `TRUEVISION-XFILE`; engine čte `.dat` sekvenčně a `dsize` ignoruje | `do_dat_tool.py pack` ořezává data za pixely; `validate_race.py` je hlídá |
| Portrét = malá hlava v rohu | codex dostal celý board 1024², slot zabíral 400×320 | codexu posílat jen výřez slotů (`slots_bbox`) a vložit zpět (`embed_generated`) |
| Budova má jiný tvar než lidská | codex kopíroval tvar z design sheetu | v design i restyle promptu „zachovej tvar/půdorys lidské budovy“ (`LAYOUT_RULE`) |
| Stavební fáze nakreslené jako hotové budovy | codex nechápe význam animace | nápovědy podle animace (`ANIMATION_HINTS`: `picture`, `build`, `zombie`, `projectile`) |
| Bílé lemy kolem užší postavy | codex namaloval pozadí uvnitř staré siluety | bílou zprůhlednit, ale **jen spojenou s okolím** (flood-fill); jinak vzniknou díry (dvůr kasáren) |
| Projektil (32 px) neprošel validací | lidský kámen má rozmazanou záři | `--accept` po vizuální kontrole |
| Paralelní běhy si přepisovaly stav | každý proces zapisoval celý JSON | stav se před zápisem slučuje; přesto nespouštět zbytečně víc procesů nad stejným `W` |
| Útok vypadal jako „zvětšení“ | původní data mají 1 statický, větší snímek útoku | nová animace `attack_anim.py` (4 snímky, `atime` = `offensive_feed_time`) |
| Grunt v útoku otočený špatným směrem | reference byl jen design sheet (pohled zepředu) | reference = board se **všemi 8 směry** + `--views "5:back-left,6:back,7:back-right"` |
| Codex „selhal“ na všech voláních | došel limit ChatGPT plánu | `run_codex` teď vyhodí `CodexUsageLimit`; počkat na reset (hláška obsahuje čas) |
| Codex nenašel vstupní obrázky | relativní cesty, codex běží s `-C work` | všechny cesty pro codex absolutní |
| Kalibrace barev párovala špatně | lidské varianty mají jiné indexy skupin (`g020_…`) | párovat podle `skupina__id` |
| Světlá věž (Watchtower) působí lidsky | design zdědil světlou omítku | u design sheetu hlídat materiály (tmavé dřevo/kámen) — uživatel to schválil vědomě |

| Stavební fáze vypadala lidsky (šedý kámen) | codex převzal materiál z lidského boardu | `restyle --hint "…same orc materials as the finished building…"` |
| Zbytky lidských předmětů (modrá ruda, bílé kameny, tečkované obrysy) | alfa z lidského originálu drží i ostrůvky, které codex vyplní světle | post-process: stín jen šedý (ne barevný), odstranění světlých ostrůvků, osiřelých poloprůhledných obrysů a světlého lemu na obrysu |
| Modrý kámen na orčím katapultu | lidský předmět, nápověda nezabrala | deterministicky `W/_retint.json` (`{"catapult": {"from_hue": [190,260], "to_hue": 30, "sat": 0.15}}`) |

**Review hotové rasy:** metriky proti lidské rase (bílé pixely, lem, ztracená plocha, magenta,
týmová barva) + kontaktní listy všech textur na trávě; podezřelé kusy porovnat 1:1 s lidským
originálem. Po změně post-processu vždy zkontrolovat regrese (úbytek neprůhledné plochy > 2 %).
`finalize` validuje ještě před `attack_anim apply` → chyby `footman_attack` size jsou v tu chvíli
očekávané, po `apply` musí validace projít.

Provozní drobnosti: headless server po startu hry nereaguje na `quit` (ukončovat
`timeout -k`); server zapisuje logy do `logs/` repozitáře (nesplést s logy hráče); scratchpad
se po restartu session maže (binárky serveru stavět znovu); `finalize` přepíše `.dat` ze sheetů —
**animaci útoku pak znovu aplikovat** (`attack_anim.py apply`, snímky zůstávají v `ai-working/attack-*`)
a znovu spustit `variants`.

## 3. Úplně nová rasa (ne 1:1 kopie lidí) — doporučený postup

Rozdíl proti orkům: pro nové jednotky/budovy **neexistuje předloha**, ze které by šla převzít
alfa, kotva a počty snímků. Proto:

1. **Návrh obsahu:** seznam jednotek a budov, role, statistiky → nový `.rac` (id, `tg_*`
   skupiny, `can_build`, `products`, materiály). Kde to jde, držet **rozměry buněk a počty
   snímků** podle nejbližší lidské entity (jednotka 64×64, 8 směrů; budovy podle `width/height`),
   aby šly použít stejné kotvy (`pointx/pointy`) a stíny.
2. **Předloha tvaru:** pro každou novou entitu vybrat lidskou entitu podobné velikosti jako
   „siluetu a měřítko“ (kotva, velikost v buňce). Tam, kde má nová entita zůstat tvarově
   blízko, lze dál použít restyle 1:1 (nejpřesnější).
3. **Nové snímky bez předlohy** generovat stejně jako `attack_anim.py`: magentové pozadí,
   mřížka 2×2 se širokým okrajem, klíčování, jednotné měřítko na směr (z jednoho referenčního
   snímku), ukotvení nohou na zemní bod, stín převzít z předlohy (nebo syntetizovat).
   Reference pro codex = board se všemi 8 směry + nápověda pohledu pro zadní směry.
4. **Pořadí:** design sheety → schválení → 1 jednotka + 1 budova pilot (včetně herního
   testu) → zbytek → barevné varianty → validace → headless test → dočasná aréna (bod 10).
5. **Validátor** dnes porovnává s referenční rasou 1:1 (stejné skupiny a rozměry). Pro novou
   rasu bude potřeba režim „jen konzistence“ (každá `tg_*` skupina existuje, 8 textur pro
   směrové skupiny, `hcount*vcount` sedí s rozměrem, žádná data za pixely).
6. **AI** nové rasy funguje bez úprav, pokud `.rac` dodrží typy (`w`/`f`/`b`/`a`), `can_build`
   a továrny s `products` (AI čte `build_list`, nemá natvrdo jména).

## 4. Rychlá reference příkazů

```bash
S=.cursor/skills/dark-oberon-dat/scripts; W=$PWD/ai-working/race-pipeline-<id>
bash $S/race_pipeline.sh compose --source $PWD/races/human-red --work-dir $W
bash $S/race_pipeline.sh boards  --work-dir $W
python3 $S/run_codex_board_batch.py design  $W --entities <e1,e2> --refs <ref-dir> --parallel 4
python3 $S/run_codex_board_batch.py approve $W <e1> <e2>
python3 $S/run_codex_board_batch.py restyle $W --only <e1,e2> --parallel 5
bash $S/race_pipeline.sh codex-post --work-dir $W            # validace + alfa + unboards
python3 $S/board_postprocess.py $W --only <e> --accept <board_id>   # vědomé přijetí
bash $S/race_pipeline.sh finalize --work-dir $W --output races/<id>-red --race-id <id>-red \
  --race-name "<Name> - Red" --names-json <names.json>
# animace útoku (nové snímky)
python3 $S/attack_anim.py prepare  --unpacked <unpacked> --stay-group <unit>_stay --work ai-working/attack-<unit>
python3 $S/attack_anim.py generate --work ai-working/attack-<unit> --reference <board-8-dirs.png> \
  --subject "<popis>" --views "5:back-left,6:back,7:back-right"
python3 $S/attack_anim.py build    --unpacked <unpacked> --stay-group <unit>_stay --work ai-working/attack-<unit>
python3 $S/attack_anim.py apply    --unpacked <unpacked> --group <unit>_attack --work ai-working/attack-<unit>
bash $S/race_pipeline.sh variants --source races/<id>-red --race-prefix <id> --name-prefix "<Name>"
python3 $S/validate_race.py races/<id>-red --reference races/human-red
```
