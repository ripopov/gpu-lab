#!/usr/bin/env bash
set -euo pipefail

backend=$1
sdk=$2
runner=$3
platform=$4
app=$5
output=$6
shift 6 # Remaining arguments are gem5 options, not configuration-script options.
app_name=${app##*/}
client="gpu-lab-samples-$$-sdk"
server="gpu-lab-samples-$$-runner"
uid=$(id -u)
gid=$(id -g)

[[ -x "$app" ]] || { echo "Build $app before testing." >&2; exit 1; }
mkdir -p "$output/runtime"

cleanup() {
    status=$?
    trap - EXIT INT TERM
    if [[ "$backend" == gem5-kmd ]]; then
        docker logs "$server" > "$output/gem5.log" 2>&1 || true
    fi
    docker rm -f "$client" "$server" > /dev/null 2>&1 || true
    rm -rf "$output/runtime"
    if (( status != 0 )); then
        echo "Test failed; logs: $output" >&2
    fi
    exit "$status"
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Docker's init reaps detached rocjitsu memory keepers in both backends.
attach=()
client_namespace=()
if [[ "$backend" == gem5-kmd ]]; then
    runner_env=()
    if (( $# > 0 )); then
        # The runner inserts these before its configuration script. NUL-delimited
        # arguments preserve quoting; the existing runtime mount carries the file.
        printf '%s\0' "$@" > "$output/runtime/gem5-args.bin"
        runner_env=(-e HSAKMT_GEM5_ARGS_FILE=/run/rocjitsu/gem5-args.bin)
    fi
    docker run --detach --init --pull never --network none --shm-size 2g \
        --platform "$platform" --name "$server" \
        -e HSAKMT_RUN_UID="$uid" -e HSAKMT_RUN_GID="$gid" \
        -e HSAKMT_OUTPUT_DIR=/outputs "${runner_env[@]}" \
        -v "$output:/outputs" -v "$output/runtime:/run/rocjitsu" \
        "$runner" run > /dev/null
    ready=false
    for ((attempt = 0; attempt < 60; ++attempt)); do
        if docker exec "$server" test -S /run/rocjitsu/daemon.sock; then
            ready=true
            break
        fi
        [[ $(docker inspect --format '{{.State.Running}}' "$server") == true ]] || break
        sleep 1
    done
    [[ "$ready" == true ]] || { echo "gem5 did not become ready." >&2; exit 1; }
    attach=(--attach)
    # Socket peer credentials and process_vm_* require mutually visible PIDs.
    client_namespace=(--pid="container:$server")
fi

app_timeout=300
if [[ "$backend" == gem5-kmd && "$app_name" == hip-bf16-gemm-1024x512x1024 ]]; then
    app_timeout=0
fi
timeout "$app_timeout" docker run --rm --init --pull never --network none \
    --platform "$platform" --name "$client" --user "$uid:$gid" \
    "${client_namespace[@]}" \
    -e HOME=/tmp -e ROCJITSU_RUNTIME_DIR=/run/rocjitsu \
    -v "$(dirname "$app"):/apps:ro" -v "$output:/tmp" \
    -v "$output/runtime:/run/rocjitsu" -w /tmp \
    "$sdk" rocjitsu-run "${attach[@]}" "/apps/$app_name" \
    2>&1 | tee "$output/app.log"

if [[ "$backend" == gem5-kmd ]]; then
    status=$(timeout 60 docker wait "$server")
    [[ "$status" == 0 ]] || { echo "gem5 exited with status $status." >&2; exit 1; }
fi
