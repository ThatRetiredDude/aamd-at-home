# aamd-at-home

AAMD@home: all-atom molecular dynamics on the PS5 GPU (homebrew).

Step 1 is a GPU compute feasibility probe. Nothing chemistry-related yet.

**GPU compute works on this console as a Mesa/RADV native title.** Porpoise
(PPSA99764) and RetroArch (PPSA99169) statically link RADV; the OpenGL
Showcase (PPSA99005) statically links Mesa GL 4.6 with compute shaders.
See [docs/FEASIBILITY.md](docs/FEASIBILITY.md).

## Artifacts

| File | How to run |
| --- | --- |
| [`artifacts/PPSA99021.zip`](artifacts/PPSA99021.zip) | Unzip and FTP the `PPSA99021/` folder to `/data/homebrew/PPSA99021/`, then launch **AAMD GPU Probe** from the home screen. Do not `nc` this to 9021. |
| `dist/aamd-probe.elf` | `nc -q0 <ps5> 9021 < dist/aamd-probe.elf` then FTP `/data/aamd/probe.log` (CPU-only; skip unless checking GPU results) |

The signed RADV title is in the zip on this branch. `param.json` matches
working GPU titles (`attribute3` 524352 plus `amm`/`kernel` page tables).
CI also uploads it as the `PPSA99021` Actions artifact. After launch, FTP
`/data/aamd/probe.log` or `/data/homebrew/PPSA99021/probe.log`. Rebuild
with `make vulkan` when `PS5_VULKAN` points at a built
[PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) tree.

## Build

Host check (no SDK):

```sh
make host-test
```

CPU ELF (payload SDK from [ps5-payload-dev](https://github.com/ps5-payload-dev/sdk); host needs clang/llvm 18 so `llvm-config-18` is on `PATH`):

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
make cpu
# or: docker build -t aamd-probe .
```

CI uploads the ELF as the `aamd-probe.elf` Actions artifact.

Vulkan title: follow the coordinator section in `docs/FEASIBILITY.md`.
