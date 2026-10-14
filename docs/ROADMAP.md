# Roadmap: OpenMM on the PS5 GPU

Each step is a gate. A step starts only when the one before it passed, and
each has a fallback if it fails.

## Why port OpenMM instead of writing an engine

- The MSU simulations already run in OpenMM 8.6.1 (CUDA on rented RTX 5090s,
  OpenCL/Metal on a Mac). Running the same OpenMM on the PS5 gives identical
  physics, so results can be mixed between machines.
- OpenMM ships a test suite for every force and integrator. Passing it on the
  PS5 is the validation; a new engine would need all of that written and
  checked from scratch (long-range electrostatics, constraints, barostats,
  checkpoints).
- OpenMM's GPU code needs CUDA, OpenCL or HIP. The PS5 has Vulkan. clvk
  implements OpenCL on Vulkan, so OpenMM's OpenCL platform can run unchanged.

## Gate H: OpenMM on clvk, on a Linux host (no PS5)

Build clvk and OpenMM's OpenCL platform on Linux, and run OpenMM's tests on
lavapipe (Mesa's software Vulkan driver). This answers "can OpenMM run on
clvk at all?" before any console work. `host/`, CI
`host-openmm-clvk.yml`.

Pass: `probes/01-opencl-hello` prints PASS, and these OpenMM tests pass in
single precision: DeviceQuery, Sort, Random, FFT, HarmonicBondForce,
HarmonicAngleForce, PeriodicTorsionForce, NonbondedForce, Ewald,
CustomNonbondedForce, CustomBondForce, CustomExternalForce, Settle,
CMMotionRemover, LangevinMiddleIntegrator, MonteCarloBarostat,
MonteCarloAnisotropicBarostat, MonteCarloFlexibleBarostat,
LocalEnergyMinimizer, Checkpoints, MultipleForces.

Known in our favour: clvk has no 64-bit atomics and no double precision; the
Mac's Apple OpenCL lacks both too, and OpenMM already runs on it (it falls
back to per-block force buffers). `probes/01-opencl-hello` on the Mac shows
the same profile.

If a test fails: find whether it is clvk/clspv (report upstream, patch locally)
or the software driver (retry on a real GPU via RADV on a Linux PC). Fallback
route: Mesa's own OpenCL (rusticl) on the PS5 Gallium stack.

## Step 1: OpenCL on the PS5

Build clvk for the PS5 as part of a native title (same packaging as step 0),
with clspv compiled in (no separate compiler process, no dynamic loading),
and run `probes/01-opencl-hello`.

Work:

- Cross-compile clvk + clspv + its LLVM for the PS5 with the PS5_Vulkan
  toolchain (`prospero-clang18`). clvk already cross-compiles for Android
  with a separately built libclc; the same recipe applies.
- A tiny Vulkan "loader" shim that hands clvk RADV's
  `vk_icdGetInstanceProcAddr` (PS5 titles cannot load `libvulkan.so`).
- Package as a new title id with the step-0 `param.json` shape.

Pass: `RESULT: PASS (OpenCL saxpy)` on the console, device reported by clvk
on RADV.

## Step 2: OpenMM's tests on the PS5

Build OpenMM as a static library (no plugin loading on the PS5) with the
OpenCL platform registered at start-up, link it with step 1's clvk, and run
gate H's test list on the console, writing a log to `/data/aamd/`.

Pass: the same tests as gate H pass on the PS5.

## Step 3: an MSU simulation on the PS5

A title that loads a system and state saved by OpenMM on the Mac
(`XmlSerializer`: System, State, integrator), runs N ns with checkpoints,
and writes the trajectory and energies to `/data/aamd/`. The Mac fetches the
results over FTP. The MSU repository keeps its data private; this repository
only gets the generic runner.

Pass: 1 ns of the MSU collagen control (about 29,500 atoms) agrees with the
same run on CUDA within statistical error (energies, density, urate on
collagen), and a measured ns/day.

## Making the console steps fast

Each console test needs a title installed and launched. PS5_Vulkan's
`ps5vkctl` agent can launch and stop titles remotely; set up once, it lets
builds be tested without someone at the console each time.

## Expected speed

The PS5 GPU (RDNA2, 36 CUs, about 10 TFLOPS FP32) is roughly a quarter to a
sixth of an RTX 5090 for this kind of work. Measured numbers replace this at
step 3.
