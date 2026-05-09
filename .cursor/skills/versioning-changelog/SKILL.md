---
name: versioning-changelog
description: >-
  Maintain SemVer 2.0 versioning and a Keep-a-Changelog `CHANGELOG.md` for the
  Dark Oberon fork. Use whenever the user asks to update the changelog, cut a
  release, bump a version, classify the severity of recent changes, or review
  uncommitted/recent commits and write them into `[Unreleased]`. Keeps entries
  short, grouped by Added/Changed/Fixed/Deprecated/Removed/Security, and maps
  change severity to MAJOR/MINOR/PATCH bumps.
---

# Versioning & Changelog skill

Single source of truth for **how this fork numbers releases** and **how
changes get recorded**. Reach for this skill any time the user says:
*"zapiš to do changelogu"*, *"co bumpneme — minor nebo patch?"*, *"udělej
release"*, *"zrevidovat změny od posledního tagu"*.

## Versioning policy — SemVer 2.0

Format: `vMAJOR.MINOR.PATCH` (e.g. `v0.2.0`).

| Bump | Trigger |
|------|---------|
| **MAJOR** | Breaking change to save-game format, network protocol, `.dat` / `.rac` / map format, or CLI flags. |
| **MINOR** | New feature, new gameplay mechanic, new asset pipeline, new tool, backward-compatible engine change. |
| **PATCH** | Bugfix, doc/skill update, build script tweak, gitignore/repo hygiene, no behavioural change for players. |

Pre-1.0 caveat: while we're on `0.x.y`, a MINOR may carry breaking changes —
call them out explicitly in the changelog entry.

The fork starts at **0.1.0** (independent line; upstream Dark Oberon is 1.1.0
and we don't claim continuity). Tag releases with `git tag v0.1.0` and push
tags with `git push --tags`.

## Severity classification (used in chat before writing)

When reviewing changes, label each one:

| Label | Meaning | Goes into |
|-------|---------|-----------|
| `BREAKING` | Forces players/operators to migrate (config, saves, protocol) | **Changed** + bold `**BREAKING:**` prefix; forces MAJOR bump |
| `feat` | New user-visible capability | **Added** |
| `change` | Behaviour change, no break | **Changed** |
| `fix` | Bugfix | **Fixed** |
| `deprecate` | Marked for removal | **Deprecated** |
| `remove` | Removed feature/file | **Removed** |
| `security` | CVE-like, secret leak, hardening | **Security** |
| `chore` | Internal only — repo hygiene, skill docs, CI | **Changed** (one-liner) or skip if truly invisible |

Engine/gameplay/protocol changes always make it in. Pure dev-tooling
(skills, scripts that never touch the game binary) gets one bullet, not five.

## CHANGELOG.md format — Keep a Changelog 1.1

Top of file is always `[Unreleased]`. On release, rename it to
`[X.Y.Z] - YYYY-MM-DD`, add a new empty `[Unreleased]`, update the link
references at the bottom, and tag.

```markdown
# Changelog

All notable changes to this project are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versioning: [SemVer 2.0](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- short bullet, present tense, user-facing wording

### Changed
- short bullet

### Fixed
- short bullet

### Security
- short bullet

## [0.1.0] - 2026-05-09
...

[Unreleased]: https://github.com/jakubfrieb/dark-oberon/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/jakubfrieb/dark-oberon/releases/tag/v0.1.0
```

## Workflow — "zapiš to do changelogu"

1. **Collect changes.** `git status --short` (uncommitted) **and**
   `git log --oneline <last-tag>..HEAD` (since last release; if no tag yet,
   pick a sensible cutoff like the last 5–10 commits or ask the user).
2. **Classify each change** with the severity table above. Group repeated
   tiny commits into one bullet.
3. **Decide bump** by the highest severity in the list (BREAKING → MAJOR,
   feat → MINOR, fix/chore → PATCH).
4. **Edit `CHANGELOG.md`**: add bullets under the right `[Unreleased]`
   subsection. Keep each bullet to one line, present tense, **what** changed,
   not **how**. Czech in chat, English in the changelog (consistent with the
   rest of the repo's docs).
5. **Show the diff** to the user before committing. Don't auto-commit unless
   the user explicitly asked.
6. **On release** (only when user says "udělej release X.Y.Z"):
   - Rename `[Unreleased]` → `[X.Y.Z] - <today>`.
   - Add empty `[Unreleased]` with the standard subsection skeleton.
   - Update link refs at the bottom.
   - Commit `chore(release): vX.Y.Z`.
   - `git tag vX.Y.Z` (signed if user has GPG configured, otherwise plain).
   - Remind user to `git push && git push --tags`.

## Bullet style rules

- **One line per bullet** — if it doesn't fit, split it.
- **Lead with the noun**: "Codex pipeline for…" not "Add Codex pipeline for…".
- **No commit hashes inside bullets** — link them only when a fix has a
  related GitHub issue (`(#42)`).
- **No internal jargon** — *"tile reorder editor"* beats *"WT_TILE_RG2 fix"*.
- **Anti-pattern**: dumping `git log --oneline` raw. Always recompose.

## Where the version lives in code

We currently don't embed the version in the binary. When that becomes
necessary, add `#define DO_FORK_VERSION "0.1.0"` to `src/build_info.h`
(already gitignored — generate it from a Makefile target so source bumps
auto-propagate).

## Anti-patterns (refuse)

- Bumping MAJOR for a bugfix to "look more mature".
- Editing or deleting an already-released section (history is immutable —
  add a follow-up release instead).
- Mixing two unrelated releases in one section.
- Auto-generating from commit messages without classification — commit
  messages here are terse Czech notes, not Conventional Commits.
