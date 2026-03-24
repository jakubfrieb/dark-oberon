# Specifikace formátu rasy (Dark Oberon 1.0.x) — pro lidské autory a AI generátory

Tento dokument popisuje, co musí být konzistentní mezi `*.rac`, `*.dat` a schématem mapy. Primární zdroj pravdy je kód v `src/doraces.cpp`, `src/dodata.cpp`, `src/doforces.cpp`, `src/doworkers.cpp`, `src/dolayout.h`, `src/glgui.*`.

---

## 1. Soubory a cesty

| Položka | Pravidlo |
|--------|-----------|
| ID rasy | Řetězec bez mezer, např. `human`, `orc`. Složka = `races/<id>/`. |
| Soubory | `races/<id>/<id>.rac` a `races/<id>/<id>.dat` (stejné `<id>`). |
| Schéma | V hlavičce `.rac`: `schemes "plastic"` musí přesně odpovídat `scheme "plastic"` v `.map` (viz `doengine.cpp`). |
| Materiály | Počet čísel u `materials` / `max_amount` / … = `scheme.materials_count` (u `plastic` jsou 3: gold, wood, coal). |

---

## 2. Hlavička `*.rac` (globální)

Povinné / typické položky na začátku souboru (viz `LoadRace`):

```text
name "Zobrazované jméno rasy"
author "Autor"
schemes "plastic"

tg_food_id "food"           # skupina textur v .dat (ne soubor na disku)
tg_energy_id "energy"
tg_burning_id "building_burning"

# při SOUND=1 — ID zvuků v .dat; "none" = žádný
snd_error error
snd_placement placement
snd_construction construction
snd_burning burning
snd_dead none
snd_explosion explosion1 explosion2 ...
snd_building_selected building_selected
```

- Hodnoty `tg_*` jsou **jména skupin** uvnitř `<id>.dat`, ne cesty k TGA.
- `snd_*` může mít více ID za sebou — při přehrání se vybere náhodně jeden vzorek ze skupiny.

---

## 3. Typy entit (`item_type`)

| Znak | Typ | Struktura v kódu |
|------|-----|------------------|
| `f` | Bojová jednotka | `TFORCE_ITEM` |
| `w` | Dělník | `TWORKER_ITEM` |
| `b` | Budova (ne továrna) | `TBUILDING_ITEM` |
| `a` | Továrna (produkuje jednotky) | `TFACTORY_ITEM` + sekce `<Products>` |

---

## 4. Jednotky — sekce `<Units>` / `<Unit N>`

### 4.1 Pořadí načítání

1. Nejdřív se načtou **všechny** jednotky (průchod „first“).
2. Pak **buildings**, **sources** (schéma).
3. Poté závislosti: `can_build`, `can_repair`, `allowed_materials`, … (druhý průchod `LoadRacUnit(..., false)`).

AI musí generovat **stabilní `id`** (např. `footman`), na které odkazují budovy a dělníci.

### 4.2 `view` (důležité: **není** počet směrů sprite)

- `view` je **dosah vidění v mapelích** (používá se u mlhy války / viditelnosti), viz `LoadRacUnit` → `ReadSimpleRange(..., "view", 1, MAP_MAX_SIZE, 1)`.
- **Směry postavy** nejsou v `.rac` číslem „pohledů“, ale v **počtu textur ve skupině** v `.dat` (viz §6).

### 4.3 Segmenty a terén

- Hra má **3 segmenty** (`DAT_SEGMENTS_COUNT`): typicky podzemí / zem / vzduch (indexy 0, 1, 2).
- Pro každý segment se čtou trojice hodnot:
  - `move_terrain_id` — dvakrát za segment (min/max typu terénu pro pohyb),
  - `land_terrain_id` — totéž pro „přistání“,
  - `max_speed` — max. rychlost v segmentu,
  - `max_rotation_speed` — **ve stupních/s** (engine převádí na radiány).

Další: `min_exist_segment_id`, `max_exist_segment_id`, `min_max_visible_segment_id` (6× — pro každý segment min/max viditelnosti), `land_segment_id`, `energy`, `food`, `selection_height`, `burning_position x y`, `max_hided_units`, `features`, `heal_time`, bojové parametry pokud `is_offensive true`, atd. — přesný výčet polí kopíruj z `human.rac` a z `LoadRacUnit`.

### 4.4 Textury jednotky (`tg_*`)

Názvy skupin v `.dat`; nepovinné skupiny často ukonči jako `none` (engine volá `ReadTextureGroup` s `required=false`).

| Klíč | Účel (zjednodušeně) |
|------|---------------------|
| `tg_picture_id` | Ikona / portrét (povinná skupina v kontextu načtení) |
| `tg_stay_id` | Klidová animace |
| `tg_move_id` | Pohyb (fallback na `tg_stay_id` pokud chybí) |
| `tg_attack_id` | Útok |
| `tg_rotate_id` | Otáčení |
| `tg_land_id` / `tg_anchor_id` | Přistání / kotvení (vzduch) |
| `tg_dying_id` / `tg_zombie_id` / `tg_burning_id` | Smrt / zombie / hoření |
| `tg_projectile_id` | Pouze u útočné jednotky (`is_offensive true`) — projektil |

Dělník navíc: `tg_mine_id`, `tg_repair_id`.

### 4.5 Zvuky jednotky (`snd_*`)

Typicky: `snd_ready`, `snd_selected` (více variant), `snd_command`, `snd_dead`, `snd_burning`, u útočníka `snd_fireon`, `snd_fireoff`, `snd_hit`; dělník `snd_workcomplete`, `snd_mine_material0` … podle počtu materiálů schématu.

---

## 5. Budovy — `<Buildings>` / `<Building N>`

- `width`, `height` (obdélník, ne jen čtverec jako u jednotky).
- `view` — opět **dosah**, ne směry.
- `item_type` `a` nebo `b`; továrna má `<Products>` s `product "<unit_text_id>"` a `product_time`.
- Textury: `tg_picture_id`, `tg_stay_id`, `tg_build_id`, `tg_dying_id`, `tg_zombie_id`, `tg_burning_id`, volitelně `tg_projectile_id`.
- Závislosti v druhém průchodu: `allowed_materials`, `ancestor`, `can_hide`, …

---

## 6. Směry postav a textury (kritické pro AI umění)

### 6.1 Osm směrů v rovině

Definice v `dolayout.h` (index `look_direction` 0..7):

| Index | Konstanta | Směr (pojmenování v kódu) |
|------|-----------|---------------------------|
| 0 | `LAY_SOUTH` | Jih |
| 1 | `LAY_SOUTH_WEST` | Jihozápad |
| 2 | `LAY_WEST` | Západ |
| 3 | `LAY_NORTH_WEST` | Severozápad |
| 4 | `LAY_NORTH` | Sever |
| 5 | `LAY_NORTH_EAST` | Severovýchod |
| 6 | `LAY_EAST` | Východ |
| 7 | `LAY_SOUTH_EAST` | Jihovýchod |

Směry 8 (`LAY_UP`) a 9 (`LAY_DOWN`) jsou mezi segmenty — **sprite směr se bere jen z 0..7**.

### 6.2 Pravidlo „8 textur ve skupině“

V `TFORCE_UNIT::ChangeAnimation` a `TWORKER_UNIT::ChangeAnimation`:

```cpp
if (player->race->tex_table.groups[tg].count >= 8) tex = look_direction;
else tex = 0;
```

**Význam pro produkci grafiky:**

- Pokud má skupina (např. `footman_stay`) **≥ 8 textur**, pak textura s indexem **0** = směr `LAY_SOUTH`, index **1** = `LAY_SOUTH_WEST`, …, **7** = `LAY_SOUTH_EAST`.
- Pokud má skupina **méně než 8** textur, engine **vždy** použije jen index **0** → jeden „univerzální“ sprite bez otočení podle směru.

Častý vzor u lidí: **8 samostatných TGA** ve skupině, každé s `hcount=1`, `vcount=1` (nebo animace uvnitř každého směru).

### 6.3 Mřížka snímků v jednom TGA (`hcount`, `vcount`, `atime`)

V `TTEX_TABLE::Load` (`dodata.cpp`):

- `frames_count = hcount * vcount`
- `frame_width = original_width / hcount`, `frame_height = original_height / vcount`
- `frame_time = atime_ms / (1000 * frames_count)` — celkový `atime` v milisekundách se **rozdělí** mezi všechny snímky.

Vykreslení snímku (`TGUI_TEXTURE::DrawFrame` v `glgui.cpp`):

- `frame = 0..frames_count-1`, pozice v mřížce: řádek shora, sloupec zleva doprava:  
  `frame_x = frame % h_count`, `frame_y` odspoda textury.

**Doporučení pro AI / grafiky:** držet **mocniny 2** pro rozměry textury po načtení (engine umí TGA zvětšit na další 2^N — viz `TGA_RESCALE`).

### 6.4 Typ textury (`ttype` v .dat)

| Hodnota | Význam (`TGUI_TEX_TYPE`) |
|---------|---------------------------|
| 0 | `GUI_TT_NORMAL` — obdélníkový sprite |
| 1 | `GUI_TT_RHOMBUS` — kosočtverec (izometrický „diamond“) |

---

## 7. Binární `*.dat` a skupiny textur

- Formát: viz `tools/do_dat_tool.py` a komentář v hlavičce skriptu.
- Každá **skupina** má řetězcové `name` (shodné s tím, co je v uvozovkách u `tg_*` v `.rac`).
- Uvnitř skupiny: položky s vlastním `id` (technický řetězec), soubor TGA, `hcount`, `vcount`, `atime`, `pointx`, `pointy`, `ttype`.
- `pointx` / `pointy` v souboru jsou **kotva** (engine je ukládá se znaménkem jako `-point` při kreslení).

Workflow pro AI:

1. Vygenerovat **kanonický seznam skupin a ID** (z plánovaného `.rac`).
2. Pro každou potřebnou skupinu vytvořit správný počet TGA (např. 8× pro směrovou animaci).
3. Složit `manifest.json` + `pack` → `<id>.dat`.

---

## 8. Minimální příklad „nová jednotka“ (koncept)

V `.rac` (uvnitř `<Units>` zvýšit `count` a přidat):

```text
  <Unit N>
    id "grunt"
    name "Grunt"
    item_type f
    size 1
    materials 500 1 0
    max_life 80
    max_speed 4 4 4
    max_rotation_speed 720 720 720
    selection_height 40
    burning_position 0 0
    view 6
    energy 0
    food -15
    move_terrain_id 0 0 8 25 0 0
    land_terrain_id 0 0 0 0 0 0
    min_exist_segment_id 1
    max_exist_segment_id 1
    min_max_visible_segment_id 1 2 1 2 1 2
    land_segment_id 1
    max_hided_units 0
    can_hide

    tg_picture_id grunt_picture
    tg_stay_id grunt_stay
    tg_anchor_id none
    tg_move_id grunt_move
    tg_land_id none
    tg_rotate_id none
    tg_attack_id grunt_attack
    tg_dying_id none
    tg_zombie_id grunt_zombie
    tg_projectile_id none
    tg_burning_id none

    # ... snd_* a bojové parametry jako u footmana ...
  </Unit N>
```

V `.dat` musí existovat skupiny např. `grunt_picture`, `grunt_stay`, `grunt_move`, `grunt_attack`, `grunt_zombie` s odpovídajícími TGA. Pro směry u `grunt_stay`: buď **8 textur** ve skupině `grunt_stay`, nebo 1 textura (pak žádná rotace sprite).

---

## 9. Kontrolní seznam před spuštěním hry

- [ ] `schemes` v `.rac` = schéma mapy.
- [ ] Počty hodnot u `materials` / worker polí = počet materiálů ve schématu.
- [ ] Každé `tg_*` (kromě `none`) má existující **skupinu** v `.dat`.
- [ ] Každé `snd_*` (kromě `none`) má existující zvuk v `.dat` (build se zvukem).
- [ ] `can_build` / `can_repair` / `product` odkazují na existující `id` budov/jednotek.
- [ ] Směrové animace: buď **≥ 8** textur ve skupině, nebo přijmi neotočený sprite.

---

## 10. Odkazy na zdroj v repozitáři

- Načtení rasy: `src/doraces.cpp` — `LoadRace`, `LoadRacUnit`, `LoadRacBuilding`, `LoadRacSource`.
- Výběr textury podle směru: `src/doforces.cpp` / `src/doworkers.cpp` — `ChangeAnimation`.
- Směry: `src/dolayout.h` — `LAY_SOUTH` … `LAY_SOUTH_EAST`.
- Mřížka snímků: `src/glgui.cpp` — `TGUI_TEXTURE::DrawFrame`.
- Pack/unpack dat: `tools/do_dat_tool.py`.

---

*Verze dokumentu: odvozeno od stromu Dark Oberon 1.0.2-RC1. Při změně engine aktualizuj tento soubor podle `src/`.*
