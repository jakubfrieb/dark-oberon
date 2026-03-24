# Dark Oberon 1.0.2 — co nainstalovat (Manjaro / Arch Linux)

Projekt je **C++** s **OpenGL** a očekává **GLFW 2.x** (staré API: `glfw.h`, `glfwOpenWindow`, vlákna z GLFW). Balíček `glfw` z oficiálních repozitářů je **GLFW 3**, ten **není** kompatibilní.

## Povinné nástroje

| Balíček / skupina | Účel |
|-------------------|------|
| `base-devel`      | `make`, `gcc`, … |
| `gcc`             | překladač C++ (`g++`) |

```bash
sudo pacman -S --needed base-devel gcc
```

## Povinné knihovny (vývoj)

| Balíček | Poznámka |
|---------|----------|
| `glu` | OpenGL Utility Library (`-lGLU`) |
| `libglvnd` nebo `mesa` | OpenGL (`-lGL`) — obvykle už máte s ovladačem GPU |
| `libx11` | X11 (`-lX11`) |
| `libxxf86vm` | `-lXxf86vm` |
| `libxext` | `-lXext` |
| `libxrandr` | `-lXrandr` |

```bash
sudo pacman -S --needed glu libx11 libxxf86vm libxext libxrandr mesa
```

## GLFW 2.x (kritické)

**Oficiální repo neobsahuje GLFW 2.** Doporučené řešení na Manjaro/Arch:

### Varianta A — AUR `glfw2` (doporučeno)

S `yay` nebo jiným AUR helperem:

```bash
yay -S glfw2
```

Tato sestava používá v `src/Makefile` linker flag **`-lglfw2`** (výchozí proměnná `GLFW_LIB=glfw2`).

### Varianta B — vlastní build z `glfw-legacy`

Zdroják: [glfw/glfw-legacy](https://github.com/glfw/glfw-legacy) (větev/tag 2.7.x), build podle jejich X11 návodu. Pokud nainstalujete knihovnu jako `libglfw.so`, přeložte hru například:

```bash
cd src && make GLFW_LIB=glfw
```

## Volitelně: zvuk (FMOD)

Výchozí build má `SOUND=0` (bez zvuku). Zapnutí zvuku podle `README` vyžaduje staré **FMOD 3.x** a ruční úpravy linkeru — na moderním Linuxu je to často nepraktické; pro začátek nechte `SOUND=0`.

## Spuštění

Po úspěšném linku vznikne binárka `dark-oberon` v **kořeni** projektu (nad `src/`). Spouštějte z kořene, aby se našly adresáře s daty:

```bash
./dark-oberon
```

**Poznámka:** V této kopii repozitáře může chybět adresář `dat/` s grafikou a zvuky z plného tarballu hry. Bez něj se hra typicky nespustí nebo bude hlásit chybějící soubory — použijte kompletní balík ze [SourceForge](http://dark-oberon.sourceforge.net/) nebo zkopírujte `dat/` z plné distribuce.

## Shrnutí problému s buildem

1. **Kód z roku ~2005** ukládal ukazatele do `int` a do polí událostí — na **64bit** to bez úprav nekompiluje (nebo by bylo rozbité za běhu). V této kopii jsou pro Linux opraveny klíče GUI (`intptr_t`) a pole `TEVENT::int1` / `int2` (ukládání ukazatelů).
2. **Linker `cannot find -lglfw`** znamená: nemáte nainstalovanou **GLFW 2**, nebo máte jen GLFW 3 pod jiným názvem. Nainstalujte AUR `glfw2` nebo použijte `GLFW_LIB=…` podle názvu vaší `.so` knihovny.
