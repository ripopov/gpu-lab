#!/usr/bin/env bash
# Sourced by host entry points. Resolve local tags once, then use immutable IDs.
sdk=$(docker image inspect "${SDK_IMAGE:-gem5-gpu-hsakmt-sdk:local}" --format '{{.Id}}')
runner=$(docker image inspect "${RUNNER_IMAGE:-gem5-gpu-hsakmt-runner:local}" --format '{{.Id}}')
platform=$(docker image inspect "$sdk" --format '{{.Os}}/{{.Architecture}}')
runner_platform=$(docker image inspect "$runner" --format '{{.Os}}/{{.Architecture}}')
revision=$(docker image inspect "$sdk" --format '{{index .Config.Labels "org.gem5.rocjitsu.revision"}}')
runner_revision=$(docker image inspect "$runner" --format '{{index .Config.Labels "org.gem5.rocjitsu.revision"}}')
if [[ "$platform" != "$runner_platform" || -z "$revision" || "$revision" == '<no value>' || "$revision" != "$runner_revision" ]]; then
    echo 'SDK and runner must have matching platforms and nonempty rocjitsu revisions.' >&2
    exit 1
fi
export SDK_IMAGE="$sdk" RUNNER_IMAGE="$runner"
