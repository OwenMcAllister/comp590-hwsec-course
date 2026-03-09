#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for _ in {1..10}; do
  python3 "$SCRIPT_DIR/run.py"
  python3 "$SCRIPT_DIR/graph.py"
  sleep 2
done
