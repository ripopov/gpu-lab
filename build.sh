#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$root/scripts/images.sh"
build="$root/build/cli"
mkdir -p "$build"
docker run --rm --pull never --network none --platform "$platform" \
    --user "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$root:$root:ro" -v "$build:$build" -w "$root" \
    "$sdk" bash -c '
        set -euo pipefail
        build=$1; sdk=$2; runner=$3; shift 3
        cmake --preset default -B "$build" -DSDK_IMAGE="$sdk" -DRUNNER_IMAGE="$runner"
        cmake --build "$build" --parallel "$@"
    ' bash "$build" "$sdk" "$runner" "$@"
