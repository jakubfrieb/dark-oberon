# Migrace Dark Oberon: GLFW 2 → GLFW 3

Postup po fázích; držet projekt po každé fázi zkompilovatelný. Detailní kontext: `need_install.md`, starší rozbor nekompatibility v konverzaci.

---

## Fáze 0 — Příprava

- [ ] Vytvořit git větev / záložní commit před migrací
- [ ] Zapsat současné chování: fullscreen vs okno, VSync, rozlišení, tok myši/kláves (GUI vs hra)
- [ ] Projít a vypsat všechna místa se `glfw` (callbacky, `glfwGetTime`, mutexy, vlákna)
- [ ] (Volitelně) Připravit build přes `pkg-config glfw3` pro pozdější fázi — do té doby lze nechat GLFW 2

**Dotčené oblasti (orientačně):** `doberon.cpp`, `doengine.cpp`, `doevents.*`, `doipc.*`, `dopool.h`, `dothreadpool.h`, `donet.h` + cpp, `glgui.*`, `glfont.h`, `dologs.h`, `domouse.cpp`, `dodraw.h`, jednotky s `glfwGetTime`

---

## Fáze 1 — Odstranit GLFW synchronizaci (mutex / vlákna / sleep)

GLFW 3 tyto API nemá; nahradit standardním C++11.

- [ ] Zavést jednotnou vrstvu (např. `sync.h`): `std::mutex`, `std::lock_guard`, případně `std::condition_variable` tam, kde byly `GLFWcond`
- [ ] `doevents.cpp`: fronta událostí — `glfwCreateMutex` / Lock / Unlock / Destroy → `std::mutex`
- [ ] `doipc.h` / `doipc.cpp`: mutex + cond → `std::mutex` + `std::condition_variable`
- [ ] `dopool.h`, `dothreadpool.h`, `donet.h` (+ implementace): `GLFWmutex` / `GLFWcond` / `GLFWthread` → std / `std::thread`
- [ ] `dologs.h`, `doalloc.cpp`, `doselection.cpp`, `doforces.cpp`, `dounits.h`, … — všechny `glfwLockMutex` / `glfwUnlockMutex`
- [ ] `doengine.cpp`: `glfwCreateThread` / `glfwWaitThread` / `glfwDestroyThread` (connecting thread) → `std::thread` + řízení životnosti (join / detach / flag)
- [ ] Nahradit `glfwSleep` → `std::this_thread::sleep_for` (`doengine.cpp`, `dodraw.h` a další)

**Kontrola:** build s GLFW 2, běh menu + krátká hra; žádné `GLFWmutex` / `glfwCreateThread` v kódu.

---

## Fáze 2 — Jednotný čas

- [ ] Přidat např. `AppTimeSeconds()` (interně zatím `glfwGetTime()`)
- [ ] Postupně nahradit přímá `glfwGetTime()` v herní logice voláním této funkce (lze dělit po souborech)

**Kontrola:** stejné chování časů událostí jako před úpravou (sanity check).

---

## Fáze 3 — GLFW 3: init, okno, kontext, smyčka

- [ ] Hlavičky: `#include <GLFW/glfw3.h>` (místo `glfw.h` z GLFW 2 / `libs/`)
- [ ] `glfwInit` → `glfwWindowHint` (verze GL, double buffer, resizable, … podle potřeby starého rendereru)
- [ ] `glfwOpenWindow` → `glfwCreateWindow` + uložit `GLFWwindow*`
- [ ] `glfwMakeContextCurrent(window)`; `glfwSwapInterval` až po kontextu
- [ ] Hlavní smyčka: `glfwPollEvents` nebo `glfwWaitEvents`; ukončení přes `glfwWindowShouldClose`
- [ ] `glfwDestroyWindow`, `glfwTerminate`

**Kontrola:** okno se otevře, kontext funguje (i prázdný/černý render je OK).

---

## Fáze 4 — Vstupy a callbacky

- [ ] `glfwSetWindowUserPointer` + v callbacku číst stav hry
- [ ] Zaregistrovat GLFW 3 callbacky: framebuffer size, key, mouse button, cursor pos, scroll (co hra používá)
- [ ] Přemapovat klávesy z konstant GLFW 2 na GLFW 3 (`glgui.cpp`, `doengine.cpp`, `domouse.cpp`, `KeyCallback`, `TranslateKey`, …)
- [ ] Kurzor: `glfwSetInputMode(window, GLFW_CURSOR, …)` místo `glfwDisable(GLFW_MOUSE_CURSOR)`
- [ ] Všechna `glfwGetKey(...)` předat správné `window` (globální držák okna jen dočasně)
- [ ] Ověřit souřadnice myši při HiDPI: okno vs framebuffer (`glfwGetWindowSize` vs `glfwGetFramebufferSize`)

**Kontrola:** menu, hra, výběr jednotek, chat/editace — stejné jako před migrací (nebo seznam známých rozdílů).

---

## Fáze 5 — Build a dokumentace

- [ ] `src/Makefile`: linkovat GLFW 3 (`-lglfw` / `pkg-config --libs glfw3`), upravit `INCLUDES`
- [ ] Aktualizovat `need_install.md` (balíček `glfw` z repa, ne AUR `glfw2`)
- [ ] Odstranit závislost na `libs/glfw.h`, pokud už není potřeba
- [ ] Regresní testy: fullscreen, změna rozlišení, VSync, síťové menu / vlákno připojení, zátěž CPU ve smyčce

---

## Rychlý grep pro průběh práce

```bash
rg 'glfw(Lock|Unlock|Create|Destroy|Wait|Sleep|OpenWindow|CreateThread|GetKey|SetWindow)' src/
rg 'GLFWmutex|GLFWthread|GLFWcond|GLFWCALL' src/
```

---

## Poznámka k obtížnosti

| Fáze | Náročnost |
|------|-----------|
| 0 | nízká |
| 1 | vysoká (hodně souborů) |
| 2 | nízká |
| 3 | střední |
| 4 | vysoká (ladění vstupů) |
| 5 | nízká |

Doporučené pořadí: **0 → 1 → (2 paralelně) → 3 → 4 → 5**.
