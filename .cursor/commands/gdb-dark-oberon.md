# GDB: dark-oberon klient (segfault / backtrace)

Při pádu GUI klienta — spusť z **kořene repa** (kde leží `./dark-oberon` po `make` ve `src/`).

## Batch (run → konec hry → backtrace → konec GDB)

```bash
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)" && \
gdb -q --batch -ex 'set pagination off' -ex run -ex 'thread apply all bt' -ex quit \
  --args ./dark-oberon 2>&1 | tail -80
```

- Výstup jen do konzole (žádný log soubor).
- **Nepouštět na pozadí** — příkaz běží, dokud hra nekončí (normálně nebo pád).
- Po ukončení procesu hry GDB vypíše stack všech vláken a **sám se ukončí** (`quit`); nezůstává interaktivní `(gdb)`.

## Agent: how to invoke

Spusť výše uvedený příkaz **v popředí** (bez `block_until_ms: 0` / bez backgroundu). Po pádu nebo zavření hry se GDB sám ukončí a terminálový příkaz doběhne — výstup je v tom samém terminálu.

Timeout pro nástroj (`block_until_ms`) **nepovinný** — pokud prostředí vyžaduje horní mez, použij např. **600000** (10 minut); jinak ho klidně vynech.

Nepoužívej interaktivní `gdb -ex run` bez `--batch`, pokud chceš automatické ukončení po pádu.
