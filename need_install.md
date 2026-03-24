# Dark Oberon 1.0.2 — co nainstalovat (Manjaro / Arch Linux)

Build používá **SDL2** (okno, vstup, čas, vlákna), **OpenGL** (`-lGL -lGLU`) a volitelně **SDL2_mixer** pro zvuk (`make SOUND=1`). Staré závislosti **GLFW 2** a **FMOD** už nejsou potřeba.

## Povinné nástroje

| Balíček / skupina | Účel |
|-------------------|------|
| `base-devel`      | `make`, `gcc`, … |
| `gcc`             | překladač C++ (`g++`) |
| `pkg-config`      | zjištění flagů pro `sdl2` (doporučeno) |

```bash
sudo pacman -S --needed base-devel gcc pkgconf
```

## Povinné knihovny (vývoj)

| Balíček | Poznámka |
|---------|----------|
| `sdl2` | okno, vstup, časery |
| `glu` | `-lGLU` |
| `mesa` nebo `libglvnd` | OpenGL `-lGL` |

```bash
sudo pacman -S --needed sdl2 glu mesa
```

## Volitelně: zvuk (`SOUND=1`)

| Balíček | Poznámka |
|---------|----------|
| `sdl2_mixer` | MP3/OGG/mod závisí na buildu balíčku (často `libvorbis`, `libmpg123`, `libmodplug`) |

```bash
sudo pacman -S --needed sdl2_mixer
```

Překlad:

```bash
cd src && make SOUND=1
```

Pokud `pkg-config SDL2_mixer` nic nevrátí, Makefile doplní `-lSDL2_mixer`; v tom případě musí být knihovna v defaultních cestách linkeru.

## Spuštění

Binárka je v **kořeni** projektu (`./dark-oberon`). Spouštěj z kořene, aby se našly `dat/`, `maps/`, …

**Poznámka:** V neúplné kopii repa může chybět `dat/` — použij plný tarball ze [SourceForge](http://dark-oberon.sourceforge.net/).

## Časté problémy

1. **`pkg-config: sdl2 not found`** — nainstaluj `sdl2` a `pkgconf`, případně ručně doplníš `SDL2_CFLAGS` / `SDL2_LIBS` do `Makefile` (jako dřív `sdl2-config`).
2. **Zvuk: hudba se nenačte** — zkontroluj, že `sdl2_mixer` má podporu pro formáty v `gui.dat` (OGG/MP3); v logu uvidíš varování z `Mix_LoadMUS_RW`.
