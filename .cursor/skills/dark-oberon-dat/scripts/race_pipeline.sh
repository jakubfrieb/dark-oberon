#!/bin/bash
# Dark Oberon — Race sprite-sheet pipeline
#
# Workflow:
#   1. compose    — unpack source .dat, compose per-entity sprite sheets
#   2. boards     — pack sheets into 1024x1024 AI boards (optional)
#   3. unboards   — unpack edited boards back into sheets (optional)
#   4. finalize   — slice edited sheets back, generate .rac, pack new .dat
#
# Steps 2–3 are for the AI board editing pipeline (GPT Image / Batch API).
# If editing sheets directly (manual paint, other tools), skip to finalize.

set -euo pipefail

SCRIPTS="$(cd "$(dirname "$0")" && pwd)"
# Repo root: .cursor/skills/dark-oberon-dat/scripts → four levels up
REPO_ROOT="$(cd "$SCRIPTS/../../../.." && pwd)"
AI_WORKING="${REPO_ROOT}/ai-working"

usage() {
    cat <<'EOF'
Usage:
  race_pipeline.sh compose  --source <race_dir> [--work-dir <dir>]
  race_pipeline.sh boards   --work-dir <dir>
  race_pipeline.sh unboards --work-dir <dir>
  race_pipeline.sh finalize --work-dir <dir> --output <race_dir> \
                            --race-id <id> --race-name <name> [--author <a>]
                            [--names-json <orc_entities.json>]
  race_pipeline.sh codex-post --work-dir <dir> [--only <ids>]
  race_pipeline.sh variants --source <race_dir> --race-prefix <p> --name-prefix <n>

Commands:
  compose    Unpack source .dat and compose per-entity sprite sheets.
  boards     Pack sheets into 1024x1024 AI boards for GPT Image editing.
  unboards   Unpack edited AI boards back into entity sheets.
  finalize   Slice edited sheets, generate .rac, and pack new .dat.
  codex-post Validate/fix codex boards (alpha from original) and unpack into sheets.
  variants   Recolour team colour of a finished race into <prefix>-blue / -yellow.

Example (manual edit):
  bash race_pipeline.sh compose --source races/human-red
  # edit PNGs …
  bash race_pipeline.sh finalize --work-dir … --output races/orc \
    --race-id orc --race-name "Orc Horde"

Example (AI board pipeline):
  bash race_pipeline.sh compose --source races/human-red
  bash race_pipeline.sh boards  --work-dir <work-dir>
  # run_openai_board_batch.py submit/poll/download …
  bash race_pipeline.sh unboards --work-dir <work-dir>
  bash race_pipeline.sh finalize --work-dir <work-dir> --output races/orc \
    --race-id orc --race-name "Orc Horde"
EOF
}

# ───────────────────── compose ─────────────────────
do_compose() {
    local SOURCE="" WORK=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --source)   SOURCE="$2"; shift 2;;
            --work-dir) WORK="$2";   shift 2;;
            *) echo "Unknown option: $1"; exit 1;;
        esac
    done
    [[ -z "$SOURCE" ]] && { echo "Error: --source required"; usage; exit 1; }
    SOURCE="$(cd "$SOURCE" && pwd)"

    local SRC_ID
    SRC_ID="$(basename "$SOURCE")"
    [[ -z "$WORK" ]] && WORK="${AI_WORKING}/race-pipeline-${SRC_ID}"
    mkdir -p "$WORK"

    local UNPACKED="$WORK/unpacked"
    local SHEETS="$WORK/sheets"

    echo "=== Dark Oberon — compose ==="
    echo "Source : $SOURCE ($SRC_ID)"
    echo "Work   : $WORK"

    echo ""
    echo ">>> Unpacking ${SRC_ID}.dat …"
    python3 "$SCRIPTS/do_dat_tool.py" unpack "$SOURCE/${SRC_ID}.dat" -o "$UNPACKED"

    echo ""
    echo ">>> Composing sprite sheets …"
    python3 "$SCRIPTS/compose_sheets.py" "$UNPACKED" "$SOURCE/${SRC_ID}.rac" "$SHEETS"

    echo ""
    echo "========================================================"
    echo "Sprite sheets ready at:"
    echo "  $SHEETS"
    echo ""
    echo "Edit the PNG files (send to AI model, manual paint, …)."
    echo "Do NOT change PNG dimensions!"
    echo ""
    echo "Then finalize:"
    echo "  bash $0 finalize \\"
    echo "    --work-dir $WORK \\"
    echo "    --output races/<new-id> \\"
    echo "    --race-id <new-id> \\"
    echo "    --race-name '<Display Name>'"
    echo "========================================================"
}

# ───────────────────── finalize ────────────────────
do_finalize() {
    local WORK="" OUTPUT="" RACE_ID="" RACE_NAME="" AUTHOR="AI Generated" NAMES_JSON=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --work-dir)   WORK="$2";      shift 2;;
            --output)     OUTPUT="$2";     shift 2;;
            --race-id)    RACE_ID="$2";    shift 2;;
            --race-name)  RACE_NAME="$2";  shift 2;;
            --author)     AUTHOR="$2";     shift 2;;
            --names-json) NAMES_JSON="$2"; shift 2;;
            *) echo "Unknown option: $1"; exit 1;;
        esac
    done
    [[ -z "$WORK"      ]] && { echo "Error: --work-dir required";  exit 1; }
    [[ -z "$OUTPUT"    ]] && { echo "Error: --output required";    exit 1; }
    [[ -z "$RACE_ID"   ]] && { echo "Error: --race-id required";   exit 1; }
    [[ -z "$RACE_NAME" ]] && { echo "Error: --race-name required"; exit 1; }

    local UNPACKED="$WORK/unpacked"
    local SHEETS="$WORK/sheets"
    local STATE="$SHEETS/_pipeline_state.json"

    [[ ! -f "$STATE" ]] && { echo "Error: $STATE not found — run compose first"; exit 1; }

    local SOURCE_RAC
    SOURCE_RAC=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['source_rac'])" "$STATE")

    echo "=== Dark Oberon — finalize ==="
    echo "Work dir  : $WORK"
    echo "Output    : $OUTPUT"
    echo "Race      : $RACE_ID ($RACE_NAME)"
    echo "Source rac: $SOURCE_RAC"

    echo ""
    echo ">>> Slicing sheets …"
    python3 "$SCRIPTS/slice_sheets.py" "$SHEETS" "$UNPACKED"

    echo ""
    echo ">>> Generating ${RACE_ID}.rac …"
    mkdir -p "$OUTPUT"
    local NAMES_ARGS=()
    [[ -n "$NAMES_JSON" ]] && NAMES_ARGS=(--names-json "$NAMES_JSON")
    python3 "$SCRIPTS/generate_rac.py" \
        "$SOURCE_RAC" "$OUTPUT/${RACE_ID}.rac" \
        --race-name "$RACE_NAME" --author "$AUTHOR" "${NAMES_ARGS[@]}"

    echo ""
    echo ">>> Packing ${RACE_ID}.dat …"
    python3 "$SCRIPTS/do_dat_tool.py" pack "$UNPACKED" -o "$OUTPUT/${RACE_ID}.dat"

    echo ""
    echo ">>> Validating against source race …"
    python3 "$SCRIPTS/validate_race.py" "$OUTPUT" --reference "$(dirname "$SOURCE_RAC")"

    echo ""
    echo "=== Done ==="
    echo "New race files:"
    echo "  $OUTPUT/${RACE_ID}.rac"
    echo "  $OUTPUT/${RACE_ID}.dat"
}

# ───────────────────── boards ──────────────────────
do_boards() {
    local WORK=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --work-dir) WORK="$2"; shift 2;;
            *) echo "Unknown option: $1"; exit 1;;
        esac
    done
    [[ -z "$WORK" ]] && { echo "Error: --work-dir required"; exit 1; }

    local SHEETS="$WORK/sheets"
    local BOARDS="$WORK/boards"

    echo "=== Dark Oberon — pack AI boards ==="
    echo "Sheets : $SHEETS"
    echo "Boards : $BOARDS"
    echo ""

    python3 "$SCRIPTS/pack_ai_boards.py" "$SHEETS" --out "$BOARDS"

    echo ""
    echo "========================================================"
    echo "AI boards ready at:"
    echo "  $BOARDS"
    echo ""
    echo "Next: submit boards to OpenAI for restyling:"
    echo "  python3 $SCRIPTS/run_openai_board_batch.py submit $BOARDS \\"
    echo "    --reference <identity.png> \\"
    echo "    --style '<description>'"
    echo "========================================================"
}

# ───────────────────── unboards ────────────────────
do_unboards() {
    local WORK=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --work-dir) WORK="$2"; shift 2;;
            *) echo "Unknown option: $1"; exit 1;;
        esac
    done
    [[ -z "$WORK" ]] && { echo "Error: --work-dir required"; exit 1; }

    local BOARDS="$WORK/boards"
    local EDITED="$BOARDS/edited"

    echo "=== Dark Oberon — unpack AI boards ==="
    echo "Boards  : $BOARDS"
    echo "Edited  : $EDITED"
    echo ""

    python3 "$SCRIPTS/unpack_ai_boards.py" "$BOARDS"

    echo ""
    echo "Entity sheets updated. Run finalize next."
}

# ───────────────────── codex-post ──────────────────
do_codex_post() {
    local WORK="" ONLY=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --work-dir) WORK="$2"; shift 2;;
            --only)     ONLY="$2"; shift 2;;
            *) echo "Unknown option: $1"; exit 1;;
        esac
    done
    [[ -z "$WORK" ]] && { echo "Error: --work-dir required"; exit 1; }
    local ONLY_ARGS=()
    [[ -n "$ONLY" ]] && ONLY_ARGS=(--only "$ONLY")
    python3 "$SCRIPTS/board_postprocess.py" "$WORK" "${ONLY_ARGS[@]}" || \
        echo "WARNING: some boards failed validation — see $WORK/_post_report.json"
    python3 "$SCRIPTS/unpack_ai_boards.py" "$WORK/boards" --sheets-dir "$WORK/sheets"
}

# ───────────────────── variants ────────────────────
do_variants() {
    local SOURCE="" PREFIX="" NAME_PREFIX=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --source)      SOURCE="$2";      shift 2;;
            --race-prefix) PREFIX="$2";      shift 2;;
            --name-prefix) NAME_PREFIX="$2"; shift 2;;
            *) echo "Unknown option: $1"; exit 1;;
        esac
    done
    [[ -z "$SOURCE" || -z "$PREFIX" || -z "$NAME_PREFIX" ]] && \
        { echo "Error: --source, --race-prefix, --name-prefix required"; exit 1; }
    local SRC_ID TMP
    SRC_ID="$(basename "$SOURCE")"
    TMP="$(mktemp -d)"
    python3 "$SCRIPTS/do_dat_tool.py" unpack "$SOURCE/${SRC_ID}.dat" -o "$TMP/src"
    for COLOR in blue yellow; do
        local ID="${PREFIX}-${COLOR}" OUT
        OUT="$(dirname "$SOURCE")/${ID}"
        mkdir -p "$OUT"
        python3 "$SCRIPTS/recolor_race.py" apply "$TMP/src" "$TMP/$COLOR" \
            --mapping "$SCRIPTS/team_${COLOR}.json"
        python3 "$SCRIPTS/do_dat_tool.py" pack "$TMP/$COLOR" -o "$OUT/${ID}.dat"
        python3 "$SCRIPTS/generate_rac.py" "$SOURCE/${SRC_ID}.rac" "$OUT/${ID}.rac" \
            --race-name "${NAME_PREFIX} - ${COLOR^}"
        echo "  $OUT"
    done
    rm -rf "$TMP"
}

# ───────────────────── main ────────────────────────
CMD="${1:-}"
shift || true

case "$CMD" in
    compose)  do_compose "$@";;
    boards)   do_boards "$@";;
    unboards) do_unboards "$@";;
    finalize) do_finalize "$@";;
    codex-post) do_codex_post "$@";;
    variants) do_variants "$@";;
    *)        usage; exit 1;;
esac
