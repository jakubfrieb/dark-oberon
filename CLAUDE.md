# Dark Oberon fork — project instructions

- Communicate with the user in Czech.
- Race and map workflows: `.cursor/skills/dark-oberon-race/PLAYBOOK.md`, `.cursor/skills/dark-oberon-map/`.
- Tests: `make test-ai` (C++ AI logic), `python -m pytest tests/race_pipeline tests/mapgen`.

## Versioning & Changelog (SemVer) — ZÁVAZNÉ
- Každá **hotová iterace** se verzuje podle Semantic Versioning **MAJOR.MINOR.PATCH**:
  - MAJOR = nekompatibilní změna, MINOR = nová funkce zpětně kompatibilní, PATCH = oprava.
  - Detailní pravidla (co je breaking pro `.dat`/`.rac`/mapy/síťový protokol, kategorie záznamů):
    `.cursor/skills/versioning-changelog/SKILL.md`.
- Vede se **CHANGELOG.md** ve formátu [Keep a Changelog](https://keepachangelog.com):
  novinky průběžně do sekce `## [Unreleased]` (Added/Changed/Fixed/Removed/Security); při dokončení iterace
  přesun Unreleased do `## [X.Y.Z] - RRRR-MM-DD`, aktualizuj odkazy na konci souboru a bumpni verzi.
- **Jediný zdroj verze je soubor `VERSION`** v kořeni (např. `0.2.0`). `src/Makefile` z něj generuje
  `build_info.h` (`DO_VERSION_STRING`, zobrazeno v menu). Verzi nikde jinde natvrdo nepiš.
- „Hotová iterace" = stav mergnutý do `master`, který se dá sestavit a hrát.
- Změna není hotová, dokud není CHANGELOG aktualizovaný a verze bumpnutá.
- Git tag `vX.Y.Z` a push jen na požádání uživatele.
