#!/usr/bin/env bash
# Klasický klient Dark Oberon: make v src/ → binárka dark-oberon v koreni projektu.
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
