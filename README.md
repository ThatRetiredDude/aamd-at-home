# aamd-at-home

AAMD@home: all-atom molecular dynamics on the PS5 GPU (homebrew).

**Goal:** run [OpenMM](https://openmm.org), unchanged, on a jailbroken PS5's
GPU, so the console can run the same simulations as a rented RTX 5090 (first
target: the monosodium urate / collagen runs of the MSU project).

**Route:** OpenMM's existing OpenCL platform, on
[clvk](https://github.com/kpet/clvk) (OpenCL implemented on Vulkan), on the
PS5's Vulkan driver (Mesa RADV, already proven on this console in step 0).
No OpenMM GPU code is rewritten. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Status

| Step | What | Where it runs | Status |
| --- | --- | --- | --- |
| 0 | Vulkan compute on the PS5 GPU (saxpy) | PS5 | **PASS** (6.6x vs 16 CPU threads, 0 mismatches) |
| H | OpenMM's own tests on clvk, software Vulkan | Linux host / CI | in progress |
| 1 | OpenCL on the PS5 (clvk on RADV), saxpy | PS5 | not started |
| 2 | OpenMM's tests on the PS5 | PS5 | not started |
| 3 | An MSU simulation on the PS5, checked against the 5090 | PS5 | not started |

Each step is a go/no-go gate; details and fallbacks in
[docs/ROADMAP.md](docs/ROADMAP.md).

## Layout

| Path | What |
| --- | --- |
| `probes/00-vulkan-saxpy/` | Step 0: Vulkan saxpy title `PPSA99021` (plus CPU ELF and GL fallback) |
| `probes/01-opencl-hello/` | Step 1: OpenCL device check and saxpy; runs on the host now, PS5 title next |
| `host/` | Linux host builds of clvk and OpenMM, and the test run (gate H) |
| `docs/` | Roadmap, architecture, step-0 feasibility evidence |
| `artifacts/` | Prebuilt signed titles (GPL, see `NOTICE.md`) |

## Quick start

Step 0 host check (no SDK):

```sh
make host-test
```

Gate H on a Linux x86-64 machine (Ubuntu 24.04; needs cmake, ninja, python3,
`mesa-vulkan-drivers`, `libvulkan-dev`; the clvk build includes LLVM and takes a
while):

```sh
host/build-clvk.sh      # clvk with the clspv compiler built in
host/build-openmm.sh    # OpenMM 8.6.1, OpenCL platform, linked to clvk
host/run-tests.sh       # step-1 probe + OpenMM tests on lavapipe
```

CI runs the same thing (`.github/workflows/host-openmm-clvk.yml`).

Step 0 on the console: see [probes/00-vulkan-saxpy/README.md](probes/00-vulkan-saxpy/README.md).

## License

The source code here is MIT (`LICENSE`), except
`probes/00-vulkan-saxpy/vulkan/probe.c` (GPL-3.0-or-later, derived from PS5_Vulkan). The prebuilt title
`artifacts/PPSA99021.zip` links GPL-3.0-or-later code (PS5_Vulkan,
PS5_PayloadSDK), so that binary is distributed under GPL-3.0-or-later
(`LICENSES/GPL-3.0-or-later.txt`). `NOTICE.md` lists every third-party
part and the exact source commits it was built from. clvk is Apache-2.0 and
OpenMM is MIT/LGPL; future titles that link them will list them in `NOTICE.md`.
