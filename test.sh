#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [[ $# -gt 1 || ( $# -eq 1 && ! $1 =~ ^(simdojo|gem5-kmd)$ ) ]]; then
    echo "Usage: $0 [simdojo|gem5-kmd]" >&2
    exit 2
fi
"$root/build.sh"
args=()
if [[ $# -eq 1 ]]; then args=(-L "^$1$"); fi
exec ctest --test-dir "$root/build/cli" --output-on-failure "${args[@]}"
