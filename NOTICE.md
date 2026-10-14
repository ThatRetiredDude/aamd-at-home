# Third-party notices

AAMD@home probe sources in this repository are MIT (see LICENSE).

A **linked native title** that uses `probe/vulkan/build.sh` pulls in:

- Mesa RADV / ACO / NIR from [mihawk-99/PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa)
  (Mesa MIT / Khronos / other licenses in that tree).
- PS5 winsys, CRT and `libc.prx` from
  [mihawk-99/PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan)
  (GPL-3.0-or-later). A distributed `eboot.bin` that links that tree is
  GPL-3.0-or-later.
- Platform shims from [mihawk-99/PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK).

The OpenGL fallback links [blackbearreloaded/ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl)
(GPL-3.0-or-later).

The CPU ELF uses only [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk).

`probe/vulkan/probe.c` follows the ICD / compute pattern of
`PS5_Vulkan/radv/radv_smoke.c` (Copyright Mihawk-99, GPL-3.0-or-later).
The saxpy kernel and CPU check in this repo are new MIT code.
