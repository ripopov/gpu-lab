#!/usr/bin/env bash
# Sourced by host entry points. Resolve local tags once, then use immutable IDs.
image_info() {
    docker image inspect "$1" --format '{{.Id}} {{.Os}}/{{.Architecture}} {{index .Config.Labels "org.gem5.rocjitsu.revision"}}'
}
sdk_info=$(image_info "${SDK_IMAGE:-gem5-gpu-hsakmt-sdk:local}")
runner_info=$(image_info "${RUNNER_IMAGE:-gem5-gpu-hsakmt-runner:local}")
read -r sdk platform revision <<< "$sdk_info"
read -r runner runner_platform runner_revision <<< "$runner_info"
if [[ "$platform" != "$runner_platform" || -z "$revision" || "$revision" == '<no value>' || "$revision" != "$runner_revision" ]]; then
    echo 'SDK and runner must have matching platforms and nonempty rocjitsu revisions.' >&2
    exit 1
fi
export SDK_IMAGE="$sdk" RUNNER_IMAGE="$runner"
