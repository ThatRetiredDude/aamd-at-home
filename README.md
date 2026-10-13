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
| `dist/aamd-probe.elf` | `nc -q0 <ps5> 9021 < dist/aamd-probe.elf` then FTP `/data/aamd/probe.log` |
| `dist/PPSA99021/` | Copy to `/data/homebrew/PPSA99021/` and launch (Vulkan saxpy on RADV) |

CI uploads `aamd-probe.elf`. The RADV title is built with `make vulkan`
when `PS5_VULKAN` points at a built [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) tree.

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
