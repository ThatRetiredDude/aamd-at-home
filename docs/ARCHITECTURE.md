# Architecture

```text
 Mac (setup and analysis)              PS5 native title (FSELF eboot.bin + libc.prx)
 ------------------------              ---------------------------------------------
 OpenMM + Python builds the system     aamd runner (C++)                 step 3
   (force field, crystal, water)         |  loads System/State XML, runs, checkpoints
 XmlSerializer -> system.xml  --FTP-->   v
                                       OpenMM 8.6.1, static, OpenCL platform  step 2
                                         |  kernels as OpenCL C source, built at run time
                                         v
                                       clvk (OpenCL 3.0 on Vulkan)            step 1
                                         |  clspv in-process: OpenCL C -> SPIR-V
                                         v
                                       Vulkan loader shim -> RADV (Mesa 26.2, PS5 winsys)  step 0
                                         |  SPIR-V -> NIR -> ACO -> AGC, sceAgcDriverSubmitDcb
                                         v
 results <--FTP-- /data/aamd/            PS5 GPU (RDNA2, "RADV NAVI21")
```

## Pieces and sources

| Piece | Source | License | Notes |
| --- | --- | --- | --- |
| OpenMM 8.6.1 | openmm/openmm | MIT (API), LGPL (some platform code) | static build, OpenCL platform only |
| clvk | kpet/clvk | Apache-2.0 | `CLVK_CLSPV_ONLINE_COMPILER=1` |
| clspv + LLVM | google/clspv, llvm/llvm-project | Apache-2.0 with LLVM exception | the biggest part of the title |
| RADV, ACO, NIR | mihawk-99/PS5_Mesa | MIT | proven in step 0 |
| winsys, CRT, `libc.prx` | mihawk-99/PS5_Vulkan | GPL-3.0-or-later | makes the linked title GPL |

## Precision and atomics

clvk reports no `cl_khr_fp64` and no `cl_khr_int64_base_atomics` on most
drivers. OpenMM then:

- uses single precision, which is also OpenMM's default on CUDA and the
  precision the MSU runs use (they do not set a precision), and
- accumulates forces in per-block float buffers instead of 64-bit atomics,
  the same path it uses on Apple's OpenCL.

## Host twin

Everything above the Vulkan driver is built and tested on Linux first
(`host/`), with lavapipe standing in for RADV. A failure there is about
OpenMM/clvk, not the console.
