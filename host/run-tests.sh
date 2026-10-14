#!/usr/bin/env bash
# Go/no-go for the whole approach, on a Linux host with no GPU:
#   1. the step-1 probe (probes/01-opencl-hello) on clvk + lavapipe (Mesa's CPU Vulkan driver)
#   2. OpenMM's own OpenCL tests for every force and integrator the MSU simulations use
# Prints a summary table; exits non-zero if anything required fails.
set -uo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
deps=${DEPS:-$root/.deps}
clvk=$deps/clvk-lib
build=$deps/openmm/build
export LD_LIBRARY_PATH=$clvk${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
# Use lavapipe unless a Vulkan driver was chosen already.
if [[ -z ${VK_ICD_FILENAMES:-} ]]; then
    for f in /usr/share/vulkan/icd.d/lvp_icd*.json; do [[ -f $f ]] && export VK_ICD_FILENAMES=$f && break; done
fi
echo "Vulkan ICD: ${VK_ICD_FILENAMES:-system default}"

mkdir -p "$root/build/host"
cc -std=c11 -O2 -Wall -Wextra -Werror -I"$deps/clvk/external/OpenCL-Headers" \
    -o "$root/build/host/opencl_hello" "$root/probes/01-opencl-hello/opencl_hello.c" \
    -L"$clvk" -lOpenCL -lm
"$root/build/host/opencl_hello" || { echo "step 1 probe FAILED"; exit 1; }

# Forces and integrators used by the MSU runs (sim/run_surface.py, run_leg.py, run_crystal.py)
# plus the infrastructure they rely on.
required=(DeviceQuery Sort Random FFT HarmonicBondForce HarmonicAngleForce PeriodicTorsionForce
          NonbondedForce Ewald CustomNonbondedForce CustomBondForce CustomExternalForce Settle
          CMMotionRemover LangevinMiddleIntegrator MonteCarloBarostat MonteCarloAnisotropicBarostat
          MonteCarloFlexibleBarostat LocalEnergyMinimizer Checkpoints MultipleForces)
pattern=$(IFS='|'; echo "^TestOpenCL(${required[*]})Single$")
cd "$build"
ctest --output-on-failure --timeout "${TEST_TIMEOUT:-3600}" -R "$pattern" | tee "$root/build/host/ctest.log"
status=${PIPESTATUS[0]}
echo
grep -E "tests passed|Test +#" "$root/build/host/ctest.log" | sed 's/^ *//'
exit $status
