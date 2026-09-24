#!/usr/bin/env bash
# Single-board restyle via local `codex exec` CLI.
#
# Usage:
#   run_codex_board.sh <board.png> <reference.png> <output.png> "<style description>"
#
# The codex CLI must be installed and either:
#   - logged in via ChatGPT subscription (`codex login`), OR
#   - able to read OPENAI_API_KEY from .env at the repo root.
#
# This is the per-board fallback to the OpenAI Batch API path
# (run_openai_board_batch.py) — handy for re-rolling individual boards.

set -euo pipefail

BOARD="${1:?board PNG required}"
REF="${2:?reference PNG required}"
OUT="${3:?output PNG required}"
STYLE="${4:?style description required}"

# Load .env if present so codex inherits OPENAI_API_KEY.
REPO_ROOT="$(git -C "$(dirname "$0")" rev-parse --show-toplevel 2>/dev/null || pwd)"
if [[ -f "$REPO_ROOT/.env" ]]; then
  set -a; source "$REPO_ROOT/.env"; set +a
fi

if ! command -v codex >/dev/null 2>&1; then
  echo "codex CLI not found. Install: https://github.com/openai/codex" >&2
  exit 1
fi

codex exec --sandbox workspace-write <<PROMPT
Restyle the isometric Dark Oberon sprite board at:
  ${BOARD}

Use the character/style from the reference image at:
  ${REF}

Style description: ${STYLE}

Rules:
- Preserve exact pose, silhouette, and facing direction of every sprite.
- Keep each sprite within its current grid cell — do not move or swap them.
- Maintain transparent background between and around sprites.
- Output 1024x1024 PNG with transparent background.

Save the resulting PNG to: ${OUT}
Print only the absolute output path on success.
PROMPT
