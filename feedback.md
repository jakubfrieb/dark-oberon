# Feedback od hráčů

Zdroj: zpětná vazba hráče (2026-11-16), verze 0.6.0.

Platforma hráče: **macOS** (Apple). Oficiální mac balíček neexistuje → pravděpodobně vlastní build z Homebrew SDL2 (= sdl2-compat nad SDL3: asynchronní změna velikosti okna), pravděpodobně Retina (body ≠ pixely) a trackpad/Magic Mouse (pravé tlačítko jen dvěma prsty / Ctrl+klik). Ověření na Linuxu proto pokrývá jen část chování.

Vydáno v **0.7.0-rc.1** (2026-11-16), opravy z testu kompatibility v **0.7.0-rc.2** (2026-11-17). Relay přes server → **0.7.0-rc.3**. Po otestování → 0.7.0 stable.

Stav: `nové` → `investigace` → `návrh` → `oponentura` → `čeká na ověření ve hře` → `opraveno` / `odloženo`

| #  | Oblast   | Popis (původní znění)                                                                                                   | Typ      | Stav  |
|----|----------|-------------------------------------------------------------------------------------------------------------------------|----------|-------|
| F1 | Grafika  | Keď zväčším rozlíšenie, tak myš ostane obmedzená na pôvodné menšie rozlíšenie, takže ho nemôžem potvrdiť.               | bug      | v 0.7.0-rc.1 – čeká na test na Macu |
| F2 | Zvuk     | Zvuky jednotiek sa mi neprehrávali celé, prípadne vôbec.                                                                | bug      | v 0.7.0-rc.1 – čeká na test na Macu |
| F3 | Zvuk     | Po vypnutí hry v reproduktoroch dost nepekne praskne.                                                                    | bug      | v 0.7.0-rc.1 – čeká na test na Macu |
| F4 | Ovládání | Keď je označená budova, dá sa klikom nastaviť, kam má posielať nové jednotky. Mám pocit, že sa na tom mieste zobrazovala zapichnutá vlajka. | bug/UX   | v 0.7.0-rc.1 – čeká na test na Macu |
| F5 | Ovládání | Zoom mapy kolieskom myši by mal mať stred na polohe myši, nie v strede obrazovky.                                        | UX       | v 0.7.0-rc.1 – čeká na test na Macu |
| F6 | Ovládání | Možnosť zmeniť štýl ovládania podľa Command & Conquer (ľavé tlačidlo posiela označené jednotky, neodznačí ich).          | feature  | v 0.7.0-rc.1 – čeká na test na Macu |
| F7 | Síť      | Pri pokuse o pripojenie na Oberon Cloud mi to hodí len "Connection refused".                                             | bug      | v 0.7.0-rc.1 – čeká na test na Macu |
| F8 | Editor   | V editore je menu nejaké rozhodené.                                                                                      | bug      | v 0.7.0-rc.1 – čeká na test na Macu |

## Detail

Ke každé položce se doplní: příčina (soubor:řádek), návrh opravy, výsledek oponentury, větev/commit.

### F1 — myš omezená na původní rozlišení
- Větev: `fix/resolution-mouse`
- Změna rozlišení v menu Video (`src/doengine.cpp:2144-2199`) volá `glfwSetWindowSize` → `SDL_SetWindowSize` a hned `SDL_GetWindowSize` (`src/doglfw_sdl.cpp:397-404`). Při asynchronním WM (X11, SDL3/sdl2-compat) vrací ještě starou velikost → `config.scr_*`, GUI a ortho projekce se postaví pro staré rozlišení; pozdější `SIZE_CHANGED` přepíše jen config + viewport (`SizeCallback`, `doengine.cpp:4996`).
- Mouse grab (`set_window_mouse_grab`) se po změně velikosti neobnovuje.
- Zatím hypotéza — ověřuje se reprodukcí ve hře.
- Reprodukováno (sdl2-compat 2.32 + SDL3 3.4, stejný stack jako Homebrew): pozdě doručená velikost okna (async resize / WM ořízne okno větší než obrazovka) → menu postavené pro starou velikost, kurzor nedosáhne na ovládání. Screenshoty `scratchpad/shots/f1_*`.
- Oprava: SizeCallback označí menu k přestavění, když skutečná velikost dorazí později. Po oponentuře (`cd40018`): i Play menu; v podmenu se přestavění odloží do návratu na základní panel (nic rozepsaného se neztratí); po změně velikosti se znovu zachytí myš (riziko na macOS); okamžitý redraw.
### F2 — zvuky jednotek se nepřehrají celé / vůbec
- Větev: `fix/audio-channels`
- Příčina (potvrzeno z kódu): `TSAMPLE` si pamatuje jeden `snd_ch` (`src/dosound.h:100`), který se po dohrání nenuluje. SDL_mixer kanál znovu přidělí jinému zvuku → `TSAMPLE::Play()`→`Stop()` (`src/dosound.cpp:230,244`) utne cizí zvuk a `IsPlaying()` (`dosound.cpp:170`) vrací true pro cizí zvuk, takže zvuky s `force=0` (výběr/příkaz, `doselection.cpp`) se vůbec nepřehrají.
- Návrh: ověřovat, že kanál stále hraje *náš* chunk (`Mix_GetChunk(ch) == sample`) v IsPlaying/Stop/SetVolume.
- Commity `28614f6`, `b4b9676`. Oponentura: OK (zamítnuto: audio mutex — nepřiměřené, okénko v µs, existovalo i dřív). Sluchem neověřitelné na tomto stroji.
### F3 — prasknutí v reproduktorech po vypnutí hry
- Větev: `fix/audio-channels`
- Příčina: při ukončení (`DestroyAll`, `src/doberon.cpp:216-262`) se nejdřív uvolní hrající chunky/hudba (`DeleteData`), pak tvrdé `Mix_HaltChannel`/`Mix_CloseAudio` bez fade-outu → skok v signálu = lupnutí.
- Návrh: krátký fade-out kanálů a hudby před `DeleteData()`.
- Fade-out 150 ms před uvolněním zvuku i při odchodu ze hry (Quit). Ukončení ověřeno: bez pádu, +0.2 s.
### F4 — vlajka shromaždiště budovy
- Větev: `feat/rally-flag`
- Není regrese: shromaždiště (rally point) přidal fork v `b6866e1`, nikdy nemělo vizuální značku; jediná odezva je zelený kurzor. Žádný asset vlajky v `dat/` neexistuje.
- Návrh: u vybrané vlastní továrny s aktivním shromaždištěm vykreslit vlajku (GL primitivy, vzor `TFORCE_UNIT::DrawLine`, `src/doforces.cpp:1691`).
- Commity `6e8044c`, `7b334d7`. Oponentura: vlajka mizela s prázdnou frontou (opraveno), zrušena čára k továrně (duplikovala DrawLine). Ve hře ověřeno, Codex PASS (`scratchpad/shots/verify_f4_*`).
### F5 — zoom kolečkem ke kurzoru
- Větev: `feat/zoom-to-cursor`
- `MouseWheelCallback` (`src/doengine.cpp:5130`) volá jen `map.Zoom()` (`src/domap.cpp:1472`), které mění pouze faktor zoomu → zoom vždy ke středu.
- Návrh: po zoomu kolečkem posunout mapu o `s·(c2−c1)` (s = pozice kurzoru od středu, c = koeficient projekce), `TMAP::Move` řeší ořez okrajů. Klávesy +/− zůstávají ke středu.
- Commity `1ee235a`, `dfb80dc`. Navíc oprava: vodorovný swipe trackpadu dřív resetoval zoom (Zoom(0) = reset). Ve hře ověřeno, Codex PASS (`verify_f5_*`).
### F6 — ovládání ve stylu C&C (volitelné)
- Větev: `feat/cnc-controls`
- Dnes: levé tlačítko = výběr (na mouse-up `UnselectAll`, `src/doengine.cpp:2664`), pravé = příkaz (`doengine.cpp:2577`).
- Návrh: volitelný přepínač (default vypnuto) — při vybraných vlastních jednotkách levý klik vydá příkaz podle kurzoru (pohyb/útok/těžba/…); pravý klik zruší výběr. Tažení = výběr obdélníkem beze změny.
- Commity `ec870e3`, `1d418b6` (+ oprava z Codex kola 2). Volba "C&C mouse controls" v Options → Video, default vypnuto. Ve hře ověřeno (pohyb levým, pravý = zrušit výběr), Codex PASS (`verify_f6_*`).
- Mac bez pravého tlačítka: v C&C režimu příkazy levým; zrušení výběru kliknutím mimo mapu / Ctrl+klik / prázdný box.
### F7 — Oberon Cloud: "Connection refused"
- Větev: `fix/connect-error-hint`
- Není chyba nasazení: výchozí adresa `oberon-game.cloud.digitalmind.cz:17000` (`src/doconfig.h:63`) — porty 17000-17009 poslouchají jen když někdo hostuje hru z lobby. Sonda 2026-11-16: DNS OK, web/lobby 200, porty 17000-17009 RST (nikdo nehostuje).
- Klient zobrazí jen „Error connecting to host …“ (`src/doengine.cpp:632`), důvod (errno) se ztratí.
- Návrh: při ECONNREFUSED zobrazit nápovědu „nikdo tam nehostuje hru, otevři lobby …“.
- Commity `f33af4b`, `b3c1ce0`. Ve hře ověřeno, Codex PASS (`verify_f7_6.png`).
- Doporučení (neprovedeno): výchozí adresa `…:17000` je zavádějící, port přiděluje lobby dynamicky.
### F8 — rozházené menu v editoru
- Větev: `fix/editor-menu`
- Editor GUI je v `src/doengine.cpp` (vstupní menu 4506-4540, `CreateEditorGUI` 6561). Natvrdo zadané pozice; tlačítko „Open“ vpravo nahoře mimo seznam map; GUI se po změně rozlišení nepřestaví.
- Příčina nepotvrzena — ověřuje se reprodukcí (screenshoty v různých rozlišeních).
- Commit `373d10d`: tlačítko Open u seznamu map, mezery u nadpisů palety, popisek "pick category" pod Back. Oponentura OK.
- Nově nalezeno (i v 0.6.0): přes průsvitný dialog "Do you really want to disconnect?" prosvítalo tlačítko MAP EDITOR → dialogy menu jsou neprůhledné (`6e5ffa8`).

## Postup

- Investigace: 6 agentů (Sonnet), implementace v samostatných worktrees/větvích (Opus).
- Oponentura: 2 kola (5× Haiku – funkční / architektura / kód / regrese / minimalita, 1× Sonnet, 1× Codex), pak koncepční review (Opus + Codex) na duplicity a odchylky od standardů → konsolidační commit `8a94dca`.
- Ověření ve hře na Linuxu (sdl2-compat + SDL3), grafiku hodnotil Codex ze screenshotů.

## Test RC1 na Macu (checklist)

1. **F1** Options → Video: přepni na vyšší rozlišení (i větší než obrazovka) → kurzor dojede do všech rohů, jde kliknout Back. Totéž ve fullscreenu.
2. **F2** Rychle vybírej jednotky / dávej příkazy během boje → každý výběr/příkaz má zvuk, zvuky se navzájem neutínají.
3. **F3** Ukonči hru z menu i ze hry (a křížkem okna) → žádné lupnutí.
4. **F4** Vyber vlastní budovu, pravým (nebo levým v C&C režimu) klikni na zem → vlajka; zůstává i bez fronty.
5. **F5** Zoom kolečkem / trackpadem nad objektem u kraje → objekt zůstává pod kurzorem; vodorovný swipe nemění zoom.
6. **F6** Options → Video → „C&C mouse controls“: levý = příkaz (výběr zůstává), rychlé tažení = výběr, pravý (dva prsty) = zrušit výběr.
7. **F7** Connect na výchozí adresu, když nikdo nehostuje → hláška s odkazem na lobby.
8. **F8** Editor: vstupní menu, paleta objektů/hráčů v 800×600 a ve tvém rozlišení → nic se nepřekrývá.

## Nalezeno při testu kompatibility RC1 (2026-11-16)

Kompatibilita: 0.6.0 ↔ 0.7.0-rc.1 funguje oběma směry (server i klient), protokol se nezměnil (žádný handshake verze, jen hash mapy). Lobby z masteru lokálně OK; produkční lobby nasazeno na 0.7.0-rc.1.

Nové chyby — existují už v 0.6.0, nejsou regrese RC:

| #  | Popis | Závažnost |
|----|-------|-----------|
| N1 | Když hráč na dedikovaném serveru nechá rasu ve výběru na výchozí hodnotě, hru nejde spustit (`Error loading race ''`, `StartGame failed`); se dvěma lidmi klienti „běží“, ale server ne. | vysoká — **opraveno v rc.2** (`fix/race-default`): server přidělí volnou rasu při připojení, klient na starém serveru pošle volnou rasu, start odmítne neplatnou rasu se zprávou; navíc oprava „duchů“ v lobby dedikovaného serveru (odpojení se ignorovalo) |
| N2 | Příkazy hráče, který se připojil jako druhý, se nezobrazují u prvního (opačně funguje). | vysoká — **opraveno v rc.2** (`fix/remote-commands-sync`): klient přiřazoval spojení k hráčům jen podle IP, takže druhý hráč posílal příkazy prvního hráče serveru. Ověřeno na localhostu i přes Haiku VM (NAT). Bez změny protokolu; starý klient jako druhý hráč zůstává rozbitý (chyba je na jeho straně) |
| N3 | 0.6.0 server spadl (`stack smashing`) s dlouhou cestou v `--data`. | nízká — **opraveno v rc.2** (`fix/long-data-path`): buffery cest 4096 + `snprintf`; i id mapy ≥ 30 znaků se odmítne místo přetečení |
| N4 | Když server skončí uprostřed hry, klient ukáže „You WON!“. | nízká — **opraveno v rc.2** (`fix/server-quit-result`): „Connection to the server was lost!“ |
| N5 | Když se klient při startu hry nedovolá k jinému hráči (NAT), spadne na nezachycenou výjimku (Linux i Haiku). | vysoká — **opraveno v rc.2** (`fix/remote-commands-sync`): hláška místo pádu, start se zruší i při nedostupném hráči s nižším číslem |
| N7 | Robustnost síťového vstupu: poškozená zpráva od klienta mohla shodit server (od 0.6.0). | vysoká — **opraveno v rc.2** (`fix/net-input-hardening`): kontrola velikostí, ID hráčů, polí herních událostí, seznamu hráčů a názvů map/ras; omezené logování |
| N6 | Hra dvou lidí přes internet za NATem: klienti se spojují přímo mezi sebou (server příkazy nepřeposílá) → za NATem to nejde. Řešení = relay přes server = změna protokolu. | architektura — plánováno do **0.7.0-rc.3** (relay přes server, kontrola verze protokolu; nekompatibilní se staršími verzemi) |

Další nálezy během rc.2 (opraveno v rc.2): pád serveru při odchodu hráče, jehož jednotky server posunul; v lobby GUI hostitele OK na hlášce „Too many players…“ znovu založilo hru a odpojilo hráče; lokální změna rasy/mapy mohla přeplnit seznam hráčů.
Odloženo do rc.3 (přepis síťové vrstvy): pád připojujícího se klienta, když hostitel odejde do ~2,5 s; pád při SIGTERM uprostřed síťové hry; use-after-free v seznamu adres talkeru.
