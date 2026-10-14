#include "aamd_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const uint32_t n = 64u * 1024u;
    float *x = malloc((size_t)n * sizeof(float));
    float *y = malloc((size_t)n * sizeof(float));
    float *ref = malloc((size_t)n * sizeof(float));
    if (!x || !y || !ref) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    aamd_saxpy_init(x, y, n);
    memcpy(ref, y, (size_t)n * sizeof(float));
    aamd_saxpy_cpu(AAMD_SAXPY_A, x, ref, n, 1);
    aamd_saxpy_cpu(AAMD_SAXPY_A, x, y, n, 4);
    float err = 0.0f;
    uint32_t bad = aamd_saxpy_compare(y, ref, n, &err);
    printf("host saxpy: n=%u mismatches=%u max_abs_err=%g y[0]=%g y[n-1]=%g\n", n, bad,
           (double)err, (double)y[0], (double)y[n - 1]);
    free(x);
    free(y);
    free(ref);
    return bad == 0 ? 0 : 1;
}
