#include "aamd_probe.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__PROSPERO__)
typedef struct {
    char unused[45];
    char message[3075];
} aamd_notify_req;

int sceKernelSendNotificationRequest(int, aamd_notify_req *, size_t, int);
int sceKernelDebugOutText(int channel, const char *text);
#endif

static FILE *g_log;

static void klog_line(const char *line) {
#if defined(__PROSPERO__)
    char buf[1024];
    snprintf(buf, sizeof buf, "%s\n", line);
    sceKernelDebugOutText(0, buf);
#else
    (void)line;
#endif
}

int aamd_log_open(void) {
    mkdir(AAMD_LOG_DIR, 0777);

    g_log = fopen(AAMD_LOG_PATH, "w");
    if (!g_log)
        g_log = fopen(AAMD_TITLE_LOG_PATH, "w");
    if (!g_log)
        g_log = fopen("probe.log", "w");
    if (g_log)
        setvbuf(g_log, NULL, _IOLBF, 0);
    return g_log ? 0 : -1;
}

void aamd_log_close(void) {
    if (g_log) {
        fclose(g_log);
        g_log = NULL;
    }
}

void aamd_log(const char *fmt, ...) {
    char line[768];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);

    fprintf(stdout, "%s\n", line);
    fflush(stdout);
    fprintf(stderr, "[aamd-probe] %s\n", line);
    fflush(stderr);
    klog_line(line);
    if (g_log) {
        fprintf(g_log, "%s\n", line);
        fflush(g_log);
    }
}

void aamd_notify(const char *fmt, ...) {
    char message[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof message, fmt, ap);
    va_end(ap);
#if defined(__PROSPERO__)
    aamd_notify_req req;
    memset(&req, 0, sizeof req);
    strncpy(req.message, message, sizeof req.message - 1);
    sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
#endif
    aamd_log("notify: %s", message);
}
