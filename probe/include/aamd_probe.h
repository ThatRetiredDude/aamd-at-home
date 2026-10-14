#ifndef AAMD_PROBE_H
#define AAMD_PROBE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 8 Mi floats: two 32 MiB arrays. Large enough to swamp caches, small enough
 * for a first console run. Must be a multiple of the compute workgroup. */
#define AAMD_SAXPY_N (8u * 1024u * 1024u)
#define AAMD_SAXPY_WG 64u
#define AAMD_SAXPY_A 2.0f

#define AAMD_LOG_DIR "/data/aamd"
#define AAMD_LOG_PATH "/data/aamd/probe.log"
#define AAMD_HOMEBREW_LOG_PATH "/data/homebrew/PPSA99021/probe.log"
#define AAMD_TITLE_LOG_PATH "/app0/probe.log"

int aamd_log_open(void);
const char *aamd_log_path(void);
void aamd_log_close(void);
void aamd_log(const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
void aamd_notify(const char *fmt, ...);

void aamd_saxpy_init(float *x, float *y, uint32_t n);
void aamd_saxpy_cpu(float a, const float *x, float *y, uint32_t n, unsigned threads);
uint32_t aamd_saxpy_compare(const float *got, const float *ref, uint32_t n, float *max_abs_err);
double aamd_now_s(void);
unsigned aamd_nthreads(void);

#ifdef __cplusplus
}
#endif

#endif
