#!/usr/bin/env bash
# Classic Dark Oberon client: make in src/ → dark-oberon binary in the project root.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT/src"

want_j=true
for a in "$@"; do
	case $a in
	-j* | --jobs | --jobs=*) want_j=false; break ;;
	esac
done

if $want_j; then
	exec make -j"$(nproc)" "$@"
else
	exec make "$@"
fi
