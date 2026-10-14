# Step 0: Vulkan compute on the PS5 GPU (saxpy)

**Result: PASS** on fw 13.60 (OnionHEN + kstuff): `RESULT: PASS (Vulkan saxpy on RADV)`,
device `PlayStation 5 GPU (RADV NAVI21)`, Mesa 26.2.0, 0 mismatches, 6.57x vs 16 CPU threads.
Evidence and the route chosen: [docs/FEASIBILITY.md](../../docs/FEASIBILITY.md).

| Path | What |
| --- | --- |
| `vulkan/` | the native title `PPSA99021` (Vulkan compute on RADV) |
| `cpu/` | CPU-only saxpy ELF for elfldr (port 9021) |
| `gl/` | OpenGL 4.3 compute fallback title |
| `src/`, `include/`, `host/` | shared saxpy + logging code, host test |
| `title/` | title assets (icon) |

## Artifacts

| File | How to run |
| --- | --- |
| [`artifacts/PPSA99021.zip`](../../artifacts/PPSA99021.zip) (GPL-3.0-or-later, see NOTICE.md) | Unzip and FTP the `PPSA99021/` folder to `/data/homebrew/PPSA99021/`, then launch **AAMD GPU Probe** from the home screen. Do not `nc` this to 9021. |
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

