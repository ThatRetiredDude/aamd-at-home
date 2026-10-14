# Step 1: OpenCL on the PS5

`opencl_hello.c` lists every OpenCL platform and device, reports the
extensions OpenMM checks, builds a saxpy kernel from source (as OpenMM does)
and checks the result on the CPU. Exit code 0 and `RESULT: PASS` mean it
works.

It already runs on:

| Where | OpenCL | Result |
| --- | --- | --- |
| Mac (Apple M2) | Apple OpenCL 1.2 | PASS; no int64 atomics, no fp64 |
| Linux host | clvk on lavapipe | `host/run-tests.sh` (CI) |
| PS5 | clvk on RADV | not built yet (see [docs/ROADMAP.md](../../docs/ROADMAP.md), step 1) |

Mac:

```sh
cc -std=c11 -O2 -o opencl_hello opencl_hello.c -framework OpenCL && ./opencl_hello
```
