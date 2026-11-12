#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
usage() {
    echo "Usage: $0 [--no-build] [--vtr] {simdojo|gem5-kmd} sample-name [build-directory]" >&2
    exit 2
}
build_first=true
gem5_args=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-build) build_first=false ;;
        --vtr) gem5_args=(--vtr) ;;
        --) shift; break ;;
        -*) usage ;;
        *) break ;;
    esac
    shift
done
if [[ $# -lt 2 || $# -gt 3 || ! $1 =~ ^(simdojo|gem5-kmd)$ || ! $2 =~ ^[a-zA-Z0-9][a-zA-Z0-9_-]*$ ]]; then
    usage
fi
backend=$1
sample=$2
if [[ ${#gem5_args[@]} -gt 0 && "$backend" != gem5-kmd ]]; then
    echo '--vtr requires the gem5-kmd backend.' >&2
    exit 2
fi
build=$(realpath -m "${3:-$root/build}")
if $build_first && [[ "$build" != "$root/build" ]]; then
    echo 'Use --no-build with a separately built directory.' >&2
    exit 2
fi
source "$root/scripts/images.sh"
if $build_first; then
    "$root/build.sh" "$sample"
fi
exec bash "$root/scripts/run-container.sh" "$backend" "$sdk" "$runner" "$platform" \
    "$build/bin/$sample" "$build/results/$backend/$sample" "${gem5_args[@]}"
