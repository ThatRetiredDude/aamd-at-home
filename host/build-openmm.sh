#!/usr/bin/env bash
# Build OpenMM (C++ only, OpenCL platform) against clvk, with OpenMM's own
# tests, for the Linux host. Run host/build-clvk.sh first.
#   host/build-openmm.sh
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/host/versions.env"
deps=${DEPS:-$root/.deps}
clvk=$deps/clvk
src=$deps/openmm
[[ -f $deps/clvk-lib/libOpenCL.so ]] || { echo "build clvk first (host/build-clvk.sh)" >&2; exit 2; }
if [[ ! -d $src/.git ]]; then
    git clone --quiet --depth 1 --branch "$OPENMM_TAG" "$OPENMM_REPO" "$src"
fi

cmake -S "$src" -B "$src/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$deps/openmm-install" \
    -DOPENMM_BUILD_OPENCL_LIB=ON \
    -DOPENMM_BUILD_CUDA_LIB=OFF \
    -DOPENMM_BUILD_HIP_LIB=OFF \
    -DOPENMM_BUILD_PYTHON_WRAPPERS=OFF \
    -DOPENMM_BUILD_C_AND_FORTRAN_WRAPPERS=OFF \
    -DOPENMM_BUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=ON \
    -DOPENMM_BUILD_OPENCL_DOUBLE_PRECISION_TESTS=OFF \
    -DOPENCL_INCLUDE_DIR="$clvk/external/OpenCL-Headers" \
    -DOPENCL_LIBRARY="$deps/clvk-lib/libOpenCL.so" \
    ${CMAKE_EXTRA:-}
cmake --build "$src/build" --parallel "${JOBS:-$(nproc)}"
echo "OpenMM $OPENMM_TAG built in $src/build against clvk"
