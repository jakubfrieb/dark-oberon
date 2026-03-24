# Migrace Dark Oberon — stav (SDL2)

Shrnutí: GLFW 2 a FMOD jsou z buildu pryč. Okno/vstup jde přes **SDL2** (`doglfw_sdl.*` kompatibilní API), čas přes `dotime.h`, vlákna přes SDL / C++, zvuk přes **SDL2_mixer** (`SOUND=1`).

Starý plán níže je **historický**; jednotlivé body byly řešeny větví vývoje (fáze 1–5).

---

## Historický plán (GLFW 2 → 3) — již neaktuální

Původní záměr byl GLFW 3; repozitář zvolil SDL2 místo přímého GLFW 3. Pro detailní kroky viz git historie nebo zálohy.

---

## Aktuální build

- Bez zvuku: `cd src && make` nebo `make SOUND=0`
- Se zvukem: `make SOUND=1` (+ balíček `sdl2_mixer`)
- Závislosti: `need_install.md`, `README` (sekce Requirements / compile)
