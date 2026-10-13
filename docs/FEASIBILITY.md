# GPU compute feasibility (step 1)

AAMD@home needs forces in GPU compute shaders on a jailbroken PS5
(firmware 13.60, OnionHEN + kstuff). This document is the evidence from
three working homebrew `eboot.bin` files pulled off the console, the
open-source stack they link, the route this probe copies, and how to run
it.

## Verdict

**GPU compute is possible today as a native homebrew title, not as a
port-9021 ELF.** Porpoise, RetroArch and the OpenGL Showcase all
statically link Mesa 26.2 into `eboot.bin` and talk to the GPU through a
PS5 winsys that submits command buffers with `sceAgcDriverSubmitDcb`.
Each title's `sce_module/` contains only `libc.prx`.

The smallest proven compute path is **Vulkan compute on mihawk-99's
RADV port** (`PS5_Mesa` + `PS5_Vulkan`). That is what this probe
copies. OpenGL 4.3 `glDispatchCompute` on `blackbearreloaded/ps5-opengl`
is the fallback (Showcase already ships compute particles).

A raw ELF sent to elfldr on TCP 9021 cannot use that stack: RADV titles
need the native CRT, FSELF-signed `eboot.bin`, `libc.prx`, and AGC
imports. The CPU saxpy ELF in this repo exists so the coordinator
pipeline is proven even before the RADV title is built.

## Console evidence (the three eboots)

Files from `/data/homebrew/<TITLEID>/eboot.bin`, strings-analyzed here:

| Title | ID | Size | GPU stack in `eboot.bin` |
| --- | --- | ---: | --- |
| OpenGL Showcase | PPSA99005 | 23 MiB | Mesa 26.2.0 GL, GLSL, `glDispatchCompute`, SSBOs, ACO, `libSceAgc` / `libSceAgcDriver` / `libSceVideoOut` |
| RetroArch | PPSA99169 | 38 MiB | `AMD RADV`, Mesa 26.2.0, ACO (`../radv-src/src/amd/compiler/aco_*.cpp`), `vkCreateComputePipelines`, `vkCmdDispatch`, `radv/ps5: sceAgcDriverSubmitDcb failed` |
| Porpoise / Dolphin | PPSA99764 | 31 MiB | Same RADV/ACO/Mesa 26.2.0, `PS5_Mesa` and `PS5_Vulkan` named in-binary, `/app0/radv-shader-cache`, `/app0/sce_module/libc.prx` |

Jelly5 (PPSA99505) was not in the zip; it is a native title as well, not
an ELF payload.

The smoking gun for the kernel GPU interface is the RADV winsys error
string in both RetroArch and Porpoise:

```text
radv/ps5: sceAgcDriverSubmitDcb failed: 0x%08x
```

That is Mesa RADV, with a PS5 winsys, submitting PM4 via the system
`libSceAgcDriver` module. It is not Piglet, not a raw ELF calling GNM,
and not a `libvulkan.so` loaded at runtime (PS5 titles cannot load
`.so` files; RetroArch's notes say `sce_module/libvulkan.so.1` was
removed when they moved to RADV).

## Source of the port

| Piece | Repo | Role |
| --- | --- | --- |
| RADV + ACO + NIR | [mihawk-99/PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa) | Mesa 26.2.0 with `-Dradv-winsys=ps5` |
| Winsys, link recipe, smoke test | [mihawk-99/PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) | Allocates direct memory, copies command streams into the ring as `sceAgcDriverSubmitDcb`, presents with VideoOut |
| Payload SDK fork | [mihawk-99/PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK) | `libps5platform.a` (heap, klog, libc shims RADV needs) |
| Public payload SDK | [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) | ELF payloads; v0.44 added `libSceAgc` stubs only |
| GL 4.6 / GLSL 4.60 | [blackbearreloaded/ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl) | Same Mesa 26.2, Gallium; compute + SSBO proven (Showcase, Khronos GL 4.6 CTS pass) |

PS5_Vulkan's own `radv/radv_smoke.c` already ran a compute dispatch on
the console (64×64 workgroups writing `i*3+7`, read back on the CPU).
`probe/vulkan/probe.c` is that program cut down to saxpy: same
`vk_icdGetInstanceProcAddr` (no loader), host-visible storage buffers,
`vkCmdDispatch`, fence wait, CPU compare.

Linking a title is `tools/radv-link.sh`: `--whole-archive` on
`libvulkan_radeon.ps5.a`, AGC import stubs, platform `--wrap` of malloc
and pthread, FSELF sign, copy `runtime/libc.prx`.

## Chosen route

1. **Primary:** Vulkan 1.3 compute on RADV, packaged as native title
   `PPSA99021`. Same driver Porpoise and RetroArch already run.
2. **Fallback:** OpenGL 4.3 compute (`glDispatchCompute` + SSBO) against
   a downloaded ps5-opengl SDK, same Mesa, also a native title.
3. **Pipeline proof:** multithreaded CPU saxpy ELF `aamd-probe.elf` for
   elfldr on port 9021. Always built in CI.

Rejected alternatives:

| Alternative | Why not |
| --- | --- |
| Piglet / OpenGL ES on PS5 | Piglet is the PS4 shell GLES stack. Showcase is Mesa GL 4.6, not Piglet. |
| Raw `libSceAgc` from an ELF | SDK v0.44 has import stubs, not headers, not a shader compiler, not a winsys. AGC Gears proves graphics, not a small compute sample we can copy. |
| `opengnm-psbc` GnmCsShader + `sceGnmSetCsShader` | PS4-era GNM. PS5 homebrew that works uses AGC + Mesa, not GNM. |
| ELF payload linking RADV | RADV titles are FSELF-signed folder apps with `libc.prx`. ps5-opengl's README: do not send GPU apps to an ELF loader. |
| Building Mesa inside this tiny repo | PS5_Mesa is ~650 MiB and a long meson/ninja build. We consume the port; we do not vendor it. |

## Risks

- **Native title vs ELF.** Coordinator must install `PPSA99021/` under
  `/data/homebrew/` (ShadowMountPlus or equivalent), not only `nc` to
  9021, for the GPU result.
- **RADV build cost.** First `tools/build-radv.sh release` is a long
  Mesa compile. CI here only builds the CPU ELF. The Vulkan title is
  `probe/vulkan/build.sh` once `PS5_VULKAN` points at a built tree.
- **Host-visible memory.** The probe uses `HOST_VISIBLE | HOST_COHERENT`
  the way `radv_smoke.c` does. If a future RADV build drops host-cached
  types, saxpy readback will need a staging copy.
- **Compute on the graphics ring.** PS5_Vulkan currently serves compute
  families from the graphics ring (no real ACE). Fine for saxpy; OpenMM
  force kernels later may want async compute that is not there yet.
- **GPL on the linked title.** RADV is MIT; PS5_Vulkan's platform/CRT
  is GPL-3.0-or-later. A distributed `eboot.bin` that links them is GPL.
  This repo's own sources stay MIT.
- **Title ID `PPSA99021`.** Local development id, not a Sony assignment.
  Change it if it collides on the console.
- **`param.json` GPU VA.** Working GPU titles (Showcase PPSA99005, Porpoise
  PPSA99764) set `attribute3` to 524352 (`0x80040`) and declare `amm` /
  `kernel` page-table sizes. A title with `attribute3: 0` and without those
  blocks dies at launch with "Something went wrong" and never reaches
  `main`. Keep `probe/vulkan/param.json` in that shape. Add `PPSA99021`
  to OnionHEN `exact_title_ids` on the console.

## Artifacts

| Path | What |
| --- | --- |
| `artifacts/PPSA99021.zip` (CI artifact `PPSA99021`) | Downloadable signed RADV title |
| `dist/PPSA99021/` | Unpacked GPU title after `make vulkan` |
| `dist/aamd-probe.elf` (CI artifact `aamd-probe.elf`) | CPU saxpy payload |
| `/data/aamd/probe.log` | Log on the console (FTP), if that directory is writable |
| `/data/homebrew/PPSA99021/probe.log` | Fallback log if `/data/aamd` is not writable |

## Coordinator: CPU ELF (always)

On the PC, with the console at `10.35.12.121` and elfldr on 9021:

```sh
nc -q0 10.35.12.121 9021 < dist/aamd-probe.elf
# or: nc 10.35.12.121 9021 < dist/aamd-probe.elf
```

A notification "AAMD probe PASS (CPU)" should appear. Then over FTP
(port 2121):

```text
/data/aamd/probe.log
```

Expect `RESULT: PASS (CPU saxpy)`, serial and N-thread timings, and
`gpu: skipped`. klog (port 3232) repeats the same lines.

## Coordinator: Vulkan title (the actual GPU probe)

A signed title is already on this branch: unzip
[`artifacts/PPSA99021.zip`](../artifacts/PPSA99021.zip). To rebuild on a
Linux host with clang 18, meson, ninja, glslang, `PS5_Mesa`, `PS5_Vulkan`,
`PS5_PayloadSDK`:

```sh
git clone https://github.com/mihawk-99/PS5_Vulkan.git
git clone https://github.com/mihawk-99/PS5_Mesa.git
git clone https://github.com/mihawk-99/PS5_PayloadSDK.git
# inside PS5_Vulkan, beside those clones:
tools/setup-native-dependencies.sh
tools/build-radv.sh release
# then in aamd-at-home:
export PS5_VULKAN=/path/to/PS5_Vulkan
make vulkan
```

`probe/vulkan/build.sh` passes `--no-dynamic-linker` so Mesa's weak
entry-point names stay out of `.dynsym` (the title converter otherwise
demands an SDK stub for each).

Install over FTP (binary mode):

```text
copy dist/PPSA99021/  ->  /data/homebrew/PPSA99021/
```

Launch **AAMD GPU Probe** from the home screen. The first line of `main`
is a `boot:` log written before any Vulkan init. Read, in order:

```text
/data/aamd/probe.log
/data/homebrew/PPSA99021/probe.log
```

Expect `device: ... RADV ...`, `RESULT: PASS (Vulkan saxpy on RADV)`,
CPU and GPU timings, `mismatches=0`.

## Coordinator: GL fallback

Download a ps5-opengl SDK release, set `PS5_OPENGL_PREFIX` to its `sdk/`
directory, `make gl`, then package with the native-app boilerplate the
same way Showcase is packaged. Prefer the Vulkan title; this exists if
RADV cannot be built.

## What to pull next if something fails

Useful FTP paths on the console:

```text
/data/aamd/probe.log
/data/homebrew/PPSA99021/eboot.bin
/data/homebrew/PPSA99021/sce_module/libc.prx
/data/homebrew/PPSA99764/eboot.bin          # Porpoise, already pulled
/data/homebrew/PPSA99169/radv-shader-cache/ # RetroArch cache
/data/homebrew/PPSA99005/                   # Showcase
```

klog during a GPU fault will contain RADV hang reports
(`========== RADV GPU hang report ==========`) and
`sceAgcDriverSubmitDcb failed`.
