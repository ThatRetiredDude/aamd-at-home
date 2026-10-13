#!/usr/bin/env bash
# Link the AAMD saxpy probe as a native title against PS5_Vulkan's RADV archive.
# Requires a built PS5_Vulkan tree (tools/setup-native-dependencies.sh and
# tools/build-radv.sh release) and PS5_Mesa at the pin that tree uses.
#
# Usage:
#   PS5_VULKAN=/path/to/PS5_Vulkan ./probe/vulkan/build.sh
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
vk=${PS5_VULKAN:?set PS5_VULKAN to a cloned, built mihawk-99/PS5_Vulkan tree}
mesa=${PS5_MESA_FORK:-$vk/../PS5_Mesa}
sdk_root="$vk/.deps/native/ps5-payload-sdk"
archive=${RADV_ARCHIVE:-$vk/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a}
if [[ ! -f $archive ]]; then
    archive=${RADV_ARCHIVE:-$mesa/build-ps5/src/amd/vulkan/libvulkan_radeon.a}
fi
native="$vk/tooling/native"
tool="$vk/build/host/ps5-native-tool"
work="$root/build/vulkan"
param="$root/probe/vulkan/param.json"
shader="$root/probe/vulkan/shaders/saxpy.comp"

for file in "$archive" "$tool" "$param" "$shader" "$sdk_root/bin/prospero-lld" \
            "$vk/tools/radv-link.sh"; do
    [[ -e $file ]] || { echo "missing $file" >&2; exit 2; }
done
command -v glslangValidator >/dev/null || { echo "glslangValidator was not found" >&2; exit 2; }

title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' "$param")
module_sdk=0x02000009
companion_sdk=0x08050001
fself_magic=0x1D3D154F

mkdir -p "$work/obj" "$work/stubs" "$work/gen"
cc() { PS5_PAYLOAD_SDK="$sdk_root" sh "$vk/tooling/prospero-clang18" "$@"; }

glslangValidator -V --target-env vulkan1.0 --vn saxpy_comp_spv "$shader" \
    -o "$work/gen/saxpy_spv.h" >/dev/null

cc -std=c11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections -D__PROSPERO__ \
    -I "$mesa/include" -I "$vk/.deps/native/radv-release/include" \
    -I "$root/probe/include" -I "$work/gen" \
    -c "$root/probe/vulkan/probe.c" -o "$work/obj/probe.o"
cc -std=c11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections -D__PROSPERO__ \
    -I "$root/probe/include" -c "$root/probe/src/aamd_log.c" -o "$work/obj/aamd_log.o"
cc -std=c11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections -D__PROSPERO__ \
    -I "$root/probe/include" -c "$root/probe/src/aamd_saxpy.c" -o "$work/obj/aamd_saxpy.o"
cc -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
    -c "$native/app_crt.cpp" -o "$work/obj/app_crt.o"
cc -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
    -c "$native/app_cpp_runtime.cpp" -o "$work/obj/app_cpp_runtime.o"

stub() {
    local library=$1 source=$2
    cc -std=c11 -O2 -fPIC -c "$vk/$source" -o "$work/obj/${library}_stub.o"
    "$sdk_root/bin/prospero-lld" --shared -soname "${library}.prx" \
        -o "$work/stubs/${library}.so" "$work/obj/${library}_stub.o"
}
stub libSceAgc vendor/ps5/sdk/stubs/agc_canary_link_stub.c
stub libSceAgcDriver vendor/ps5/sdk/stubs/agc_driver_canary_link_stub.c

# shellcheck source=/dev/null
source "$vk/tools/radv-link.sh"
radv_link_recipe "$vk" "$sdk_root" "$archive" || exit 2
"$sdk_root/bin/prospero-lld" "${radv_linker_script[@]}" --eh-frame-hdr "${radv_link_flags[@]}" \
    --version-script "$native/app-symbols.map" --exclude-libs=ALL \
    -e _start -o "$work/llvm-pie.elf" \
    "$work/obj/app_crt.o" "$work/obj/app_cpp_runtime.o" \
    "$work/obj/probe.o" "$work/obj/aamd_log.o" "$work/obj/aamd_saxpy.o" \
    "$work/stubs/libSceAgc.so" "$work/stubs/libSceAgcDriver.so" \
    "${radv_link_inputs[@]}" \
    --as-needed "$sdk_root"/target/lib/*.so
"$tool" link --in "$work/llvm-pie.elf" --out "$work/eboot.elf" \
    --stub-dir "$sdk_root/target/lib" --stub "$work/stubs/libSceAgc.so" \
    --stub "$work/stubs/libSceAgcDriver.so" --module-sdk "$module_sdk" \
    --companion-sdk "$companion_sdk" --file-name eboot.elf

app="$root/dist/$title_id"
rm -rf -- "$app"
mkdir -p "$app/sce_sys" "$app/sce_module"
"$tool" self --sign --in "$work/eboot.elf" --out "$app/eboot.bin" --magic "$fself_magic"
cp "$param" "$app/sce_sys/param.json"
for asset in icon0.png pic0.dds pic1.dds snd0.at9; do
    [[ -f $root/probe/title/sce_sys/$asset ]] && cp "$root/probe/title/sce_sys/$asset" "$app/sce_sys/$asset"
done
[[ -f $vk/runtime/libc.prx ]] || bash "$vk/tools/rebuild-libc.sh"
cp "$vk/runtime/libc.prx" "$app/sce_module/libc.prx"
"$tool" self --inspect --file "$app/eboot.bin" >/dev/null
printf 'AAMD RADV title: %s (%s bytes)\n' "$app" "$(stat -c %s "$app/eboot.bin")"
