#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for tool in make cc c++ python3; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Required tool is missing: $tool" >&2
    exit 1
  fi
done
echo "Building PagePet desktop simulator…" >&2
make --no-print-directory -f tools/simulator/Makefile -j2
exec python3 tools/simulator/app.py "$@"
