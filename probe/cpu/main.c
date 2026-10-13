#include "aamd_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    const uint32_t n = AAMD_SAXPY_N;
    const unsigned threads = aamd_nthreads();
    int status = 1;

    aamd_log_open();
    aamd_log("AAMD@home GPU compute probe (CPU path)");
    aamd_log("build: ELF payload for elfldr TCP 9021");
    aamd_log("n=%u (%.1f MiB x 2 arrays) a=%.1f threads=%u", n,
             (double)n * sizeof(float) / (1024.0 * 1024.0), AAMD_SAXPY_A, threads);
    aamd_log("gpu: skipped (this ELF does not link RADV; install PPSA99021 for Vulkan)");
    aamd_notify("AAMD probe: CPU saxpy n=%u", n);

    float *x = (float *)malloc((size_t)n * sizeof(float));
    float *y = (float *)malloc((size_t)n * sizeof(float));
    float *y_ref = (float *)malloc((size_t)n * sizeof(float));
    if (!x || !y || !y_ref) {
        aamd_log("FAIL: malloc %u floats", n);
        goto done;
    }

    aamd_saxpy_init(x, y, n);
    memcpy(y_ref, y, (size_t)n * sizeof(float));

    double t0 = aamd_now_s();
    aamd_saxpy_cpu(AAMD_SAXPY_A, x, y, n, 1);
    double t_serial = aamd_now_s() - t0;

    memcpy(y, y_ref, (size_t)n * sizeof(float));
    t0 = aamd_now_s();
    aamd_saxpy_cpu(AAMD_SAXPY_A, x, y, n, threads);
    double t_mt = aamd_now_s() - t0;

    /* Serial result is the reference for the threaded run. Recompute serial
     * into y_ref so we can compare. */
    aamd_saxpy_init(x, y_ref, n);
    aamd_saxpy_cpu(AAMD_SAXPY_A, x, y_ref, n, 1);

    float max_err = 0.0f;
    uint32_t mismatches = aamd_saxpy_compare(y, y_ref, n, &max_err);
    double gb = (double)n * sizeof(float) * 3.0 / (1024.0 * 1024.0 * 1024.0);

    aamd_log("cpu serial: %.4f s  (%.2f GiB/s effective)", t_serial, gb / t_serial);
    aamd_log("cpu %u-thread: %.4f s  (%.2f GiB/s effective, %.2fx)", threads, t_mt,
             gb / t_mt, t_serial / t_mt);
    aamd_log("cpu check: mismatches=%u max_abs_err=%g", mismatches, (double)max_err);
    aamd_log("sample y[0]=%g y[1]=%g y[n-1]=%g", (double)y[0], (double)y[1],
             (double)y[n - 1]);

    if (mismatches == 0) {
        aamd_log("RESULT: PASS (CPU saxpy)");
        aamd_notify("AAMD probe PASS (CPU)");
        status = 0;
    } else {
        aamd_log("RESULT: FAIL (CPU saxpy mismatches)");
        aamd_notify("AAMD probe FAIL");
    }

done:
    free(x);
    free(y);
    free(y_ref);
    aamd_log("log: %s", aamd_log_path());
    aamd_log_close();
    return status;
}
