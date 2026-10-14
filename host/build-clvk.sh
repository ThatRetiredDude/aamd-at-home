#!/usr/bin/env bash
# Build clvk (OpenCL 3.0 on Vulkan) for the Linux host, with clspv compiled in
# (CLVK_CLSPV_ONLINE_COMPILER=1), the way the PS5 title will need it: no
# separate compiler process. Output: $DEPS/clvk/build/libOpenCL.so
#   host/build-clvk.sh            (DEPS defaults to ./.deps)
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/host/versions.env"
deps=${DEPS:-$root/.deps}
src=$deps/clvk
mkdir -p "$deps"
if [[ ! -d $src/.git ]]; then
    git clone --quiet "$CLVK_REPO" "$src"
fi
git -C "$src" fetch --quiet origin "$CLVK_COMMIT"
git -C "$src" checkout --quiet "$CLVK_COMMIT"
git -C "$src" submodule update --init --recursive --quiet
# clspv pulls its own LLVM, Clang and libclc sources.
python3 "$src/external/clspv/utils/fetch_sources.py" --shallow --deps llvm   # as clvk's own CI

vulkan=()
if [[ -n ${VULKAN_HEADERS_DIR:-} ]]; then   # newer headers than the distro ships, with the system loader
    vulkan=(-DCLVK_VULKAN_IMPLEMENTATION=custom -DVulkan_INCLUDE_DIRS="$VULKAN_HEADERS_DIR/include"
            -DVulkan_LIBRARIES="${VULKAN_LIBRARY:-/usr/lib/x86_64-linux-gnu/libvulkan.so.1}")
fi
cmake -S "$src" -B "$src/build" -G Ninja "${vulkan[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCLVK_CLSPV_ONLINE_COMPILER=1 \
    -DCLVK_BUILD_TESTS=OFF \
    -DCLVK_BUILD_STATIC_TESTS=OFF \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    ${CMAKE_EXTRA:-}
cmake --build "$src/build" --parallel "${JOBS:-$(nproc)}"
# One stable place for the library, wherever clvk's build tree puts it.
lib=$(find "$src/build" -name 'libOpenCL.so*' -type f | head -1)
[[ -n $lib ]] || { echo "libOpenCL.so not found in $src/build" >&2; exit 1; }
mkdir -p "$deps/clvk-lib"
cp -f "$lib" "$deps/clvk-lib/libOpenCL.so"
ln -sf libOpenCL.so "$deps/clvk-lib/libOpenCL.so.1"
echo "clvk: $deps/clvk-lib/libOpenCL.so (from $lib, $CLVK_COMMIT)"
