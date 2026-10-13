#!/usr/bin/env bash
# Reproducible probe build. Always produces the CPU ELF. Optionally the RADV
# native title when PS5_VULKAN points at a built PS5_Vulkan tree.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

make host-test

if [[ -z ${PS5_PAYLOAD_SDK:-} ]]; then
    echo "PS5_PAYLOAD_SDK is unset; CPU ELF skipped. Host saxpy test passed."
    exit 0
fi

make cpu
mkdir -p dist
cp -f probe/cpu/aamd-probe.elf dist/aamd-probe.elf
echo "CPU ELF: dist/aamd-probe.elf"

if [[ -n ${PS5_VULKAN:-} ]]; then
    make vulkan
    echo "RADV title: dist/PPSA99021/"
fi
