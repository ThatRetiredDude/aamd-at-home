#include "aamd_probe.h"

#include <math.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    float a;
    const float *x;
    float *y;
    uint32_t begin;
    uint32_t end;
} saxpy_job;

double aamd_now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

unsigned aamd_nthreads(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1)
        n = 1;
    if (n > 16)
        n = 16;
    return (unsigned)n;
}

void aamd_saxpy_init(float *x, float *y, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        x[i] = (float)i * 0.001f;
        y[i] = (float)i * 0.002f;
    }
}

static void *saxpy_worker(void *arg) {
    saxpy_job *job = arg;
    const float a = job->a;
    const float *x = job->x;
    float *y = job->y;
    for (uint32_t i = job->begin; i < job->end; i++)
        y[i] = a * x[i] + y[i];
    return NULL;
}

void aamd_saxpy_cpu(float a, const float *x, float *y, uint32_t n, unsigned threads) {
    if (threads < 1)
        threads = 1;
    if (threads == 1) {
        for (uint32_t i = 0; i < n; i++)
            y[i] = a * x[i] + y[i];
        return;
    }

    pthread_t tids[16];
    saxpy_job jobs[16];
    if (threads > 16)
        threads = 16;

    uint32_t chunk = (n + threads - 1u) / threads;
    unsigned started = 0;
    for (unsigned t = 0; t < threads; t++) {
        uint32_t begin = t * chunk;
        if (begin >= n)
            break;
        uint32_t end = begin + chunk;
        if (end > n)
            end = n;
        jobs[t].a = a;
        jobs[t].x = x;
        jobs[t].y = y;
        jobs[t].begin = begin;
        jobs[t].end = end;
        if (pthread_create(&tids[t], NULL, saxpy_worker, &jobs[t]) != 0) {
            saxpy_worker(&jobs[t]);
            continue;
        }
        started++;
    }
    for (unsigned t = 0; t < started; t++)
        pthread_join(tids[t], NULL);
}

uint32_t aamd_saxpy_compare(const float *got, const float *ref, uint32_t n, float *max_abs_err) {
    uint32_t mismatches = 0;
    float worst = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float err = fabsf(got[i] - ref[i]);
        if (err > worst)
            worst = err;
        /* Exact match is expected for this input; keep a tiny slack for GPU
         * flush-to-zero / rtz differences. */
        if (err > 1.0e-4f)
            mismatches++;
    }
    if (max_abs_err)
        *max_abs_err = worst;
    return mismatches;
}
