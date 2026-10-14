/* AAMD@home step 1: is there a working OpenCL device?
 *
 * Lists every OpenCL platform and device, prints the extensions OpenMM cares
 * about, then builds a saxpy kernel from source (OpenMM always compiles from
 * source at run time) and checks the result on the CPU. The same file runs on a
 * Linux host (clvk on a software Vulkan driver) and, later, in the PS5 title
 * (clvk on RADV). Exit code 0 means PASS.
 */
#define CL_TARGET_OPENCL_VERSION 120
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 65536

static const char *kSource =
    "__kernel void saxpy(const float a, __global const float *x, __global float *y) {\n"
    "    size_t i = get_global_id(0);\n"
    "    y[i] = a * x[i] + y[i];\n"
    "}\n";

static int check(cl_int err, const char *what) {
    if (err != CL_SUCCESS) {
        printf("FAIL: %s returned %d\n", what, err);
        return 1;
    }
    return 0;
}

static void print_info(cl_device_id dev) {
    char buf[8192];
    cl_uint cu = 0;
    cl_ulong mem = 0;
    size_t wg = 0;
    clGetDeviceInfo(dev, CL_DEVICE_NAME, sizeof buf, buf, NULL);
    printf("  device: %s\n", buf);
    clGetDeviceInfo(dev, CL_DEVICE_VERSION, sizeof buf, buf, NULL);
    printf("  version: %s\n", buf);
    clGetDeviceInfo(dev, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof cu, &cu, NULL);
    clGetDeviceInfo(dev, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof mem, &mem, NULL);
    clGetDeviceInfo(dev, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof wg, &wg, NULL);
    printf("  compute units: %u  memory: %.1f GiB  max work group: %zu\n", cu, mem / 1073741824.0, wg);
    clGetDeviceInfo(dev, CL_DEVICE_EXTENSIONS, sizeof buf, buf, NULL);
    /* OpenMM checks these; missing 64-bit atomics is fine (it falls back to force buffers). */
    const char *ext[] = {"cl_khr_int64_base_atomics", "cl_khr_fp64", "cl_khr_global_int32_base_atomics",
                         "cl_khr_local_int32_base_atomics"};
    for (size_t k = 0; k < sizeof ext / sizeof *ext; k++)
        printf("  %-34s %s\n", ext[k], strstr(buf, ext[k]) ? "yes" : "no");
}

int main(void) {
    cl_platform_id plats[8];
    cl_uint np = 0;
    if (check(clGetPlatformIDs(8, plats, &np), "clGetPlatformIDs") || np == 0) {
        printf("FAIL: no OpenCL platform\n");
        return 1;
    }
    cl_device_id dev = NULL;
    for (cl_uint p = 0; p < np; p++) {
        char name[256];
        clGetPlatformInfo(plats[p], CL_PLATFORM_NAME, sizeof name, name, NULL);
        printf("platform %u: %s\n", p, name);
        cl_device_id devs[8];
        cl_uint nd = 0;
        if (clGetDeviceIDs(plats[p], CL_DEVICE_TYPE_ALL, 8, devs, &nd) != CL_SUCCESS)
            continue;
        for (cl_uint d = 0; d < nd; d++) {
            print_info(devs[d]);
            if (!dev)
                dev = devs[d];
        }
    }
    if (!dev) {
        printf("FAIL: no OpenCL device\n");
        return 1;
    }

    cl_int err;
    cl_context ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, &err);
    if (check(err, "clCreateContext")) return 1;
    cl_command_queue q = clCreateCommandQueue(ctx, dev, 0, &err);
    if (check(err, "clCreateCommandQueue")) return 1;
    cl_program prog = clCreateProgramWithSource(ctx, 1, &kSource, NULL, &err);
    if (check(err, "clCreateProgramWithSource")) return 1;
    err = clBuildProgram(prog, 1, &dev, "-cl-mad-enable -cl-no-signed-zeros", NULL, NULL);
    if (err != CL_SUCCESS) {
        char log[16384] = "";
        clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, sizeof log, log, NULL);
        printf("FAIL: clBuildProgram %d\n%s\n", err, log);
        return 1;
    }
    cl_kernel k = clCreateKernel(prog, "saxpy", &err);
    if (check(err, "clCreateKernel")) return 1;

    float *x = malloc(N * sizeof(float)), *y = malloc(N * sizeof(float)), *ref = malloc(N * sizeof(float));
    const float a = 2.0f;
    for (int i = 0; i < N; i++) {
        x[i] = i * 0.001f;
        y[i] = i * 0.002f;
        ref[i] = a * x[i] + y[i];
    }
    cl_mem bx = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, N * sizeof(float), x, &err);
    if (check(err, "clCreateBuffer x")) return 1;
    cl_mem by = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, N * sizeof(float), y, &err);
    if (check(err, "clCreateBuffer y")) return 1;
    clSetKernelArg(k, 0, sizeof a, &a);
    clSetKernelArg(k, 1, sizeof bx, &bx);
    clSetKernelArg(k, 2, sizeof by, &by);
    size_t global = N, local = 64;
    if (check(clEnqueueNDRangeKernel(q, k, 1, NULL, &global, &local, 0, NULL, NULL), "clEnqueueNDRangeKernel"))
        return 1;
    if (check(clEnqueueReadBuffer(q, by, CL_TRUE, 0, N * sizeof(float), y, 0, NULL, NULL), "clEnqueueReadBuffer"))
        return 1;
    int bad = 0;
    for (int i = 0; i < N; i++)
        if (fabsf(y[i] - ref[i]) > 1e-4f * fabsf(ref[i]) + 1e-6f)
            bad++;
    printf("saxpy n=%d mismatches=%d\n", N, bad);
    printf("RESULT: %s (OpenCL saxpy)\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
