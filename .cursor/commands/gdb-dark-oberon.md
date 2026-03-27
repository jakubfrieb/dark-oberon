# GDB: dark-oberon klient (segfault / backtrace)

Při pádu GUI klienta — spusť z **kořene repa** (kde leží `./dark-oberon` po `make` ve `src/`).

## Pust → (batch: až do SIGSEGV, pak stack všech vláken)

```bash
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)" && \
gdb -q --batch -ex 'set pagination off' -ex run -ex 'thread apply all bt' -ex quit \
  --args ./dark-oberon
```

Volitelně uložit výstup: přidej na konec `2>&1 | tee gdb-client-crash.log`.

## Pust → (interaktivní gdb — ruční `run` / `bt`)

```bash
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)" && \
gdb -q -ex 'set pagination off' --args ./dark-oberon
```

V sandboxu Cursor často neběží GUI — použij lokální terminál.
