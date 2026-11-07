#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_first=true
if [[ ${1:-} == --no-build ]]; then
    build_first=false
    shift
fi
if [[ $# -lt 2 || $# -gt 3 || ! $1 =~ ^(simdojo|gem5-kmd)$ || ! $2 =~ ^hip-(vector-add|memcopy)$ ]]; then
    echo "Usage: $0 [--no-build] {simdojo|gem5-kmd} {hip-vector-add|hip-memcopy} [build-directory]" >&2
    exit 2
fi
backend=$1
sample=$2
build=$(realpath -m "${3:-$root/build/cli}")
if $build_first && [[ "$build" != "$root/build/cli" ]]; then
    echo 'Use --no-build with a separately built directory.' >&2
    exit 2
fi
source "$root/scripts/images.sh"
if $build_first; then
    "$root/build.sh" --target "$sample"
fi
exec bash "$root/scripts/run-container.sh" "$backend" "$sdk" "$runner" "$platform" \
    "$build/bin/$sample" "$build/results/$backend/$sample"
