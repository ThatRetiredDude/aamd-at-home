/*
 * AAMD@home OpenGL 4.3 compute fallback.
 *
 * Same saxpy as the Vulkan probe, using the Mesa GL 4.6 stack that the
 * OpenGL Showcase (PPSA99005) statically links. Requires the ps5-opengl SDK
 * and a native title, not an ELF payload.
 */

#include "aamd_probe.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *k_src =
    "#version 430\n"
    "layout(local_size_x = 64) in;\n"
    "layout(std430, binding = 0) readonly buffer X { float x[]; };\n"
    "layout(std430, binding = 1) buffer Y { float y[]; };\n"
    "uniform uint n;\n"
    "uniform float a;\n"
    "void main() {\n"
    "    uint i = gl_GlobalInvocationID.x;\n"
    "    if (i < n) y[i] = a * x[i] + y[i];\n"
    "}\n";

static GLuint compile_compute(void) {
    GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(sh, 1, &k_src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof log, NULL, log);
        aamd_log("glsl compile: %s", log);
        glDeleteShader(sh);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, sh);
    glLinkProgram(prog);
    glDeleteShader(sh);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof log, NULL, log);
        aamd_log("glsl link: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static int egl_setup(EGLDisplay *out_dpy, EGLContext *out_ctx) {
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) {
        aamd_log("eglGetDisplay failed");
        return 0;
    }
    if (!eglInitialize(dpy, NULL, NULL)) {
        aamd_log("eglInitialize failed");
        return 0;
    }
    eglBindAPI(EGL_OPENGL_API);
    const EGLint cfg_attrs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
    EGLConfig cfg;
    EGLint ncfg = 0;
    if (!eglChooseConfig(dpy, cfg_attrs, &cfg, 1, &ncfg) || ncfg < 1) {
        aamd_log("eglChooseConfig failed");
        return 0;
    }
    const EGLint ctx_attrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, (EGLint[]){EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE});
    if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx)) {
        aamd_log("eglCreateContext/MakeCurrent failed");
        return 0;
    }
    aamd_log("GL_VERSION=%s  GLSL=%s", glGetString(GL_VERSION),
             glGetString(GL_SHADING_LANGUAGE_VERSION));
    *out_dpy = dpy;
    *out_ctx = ctx;
    return 1;
}

int main(void) {
    const uint32_t n = AAMD_SAXPY_N;
    const unsigned threads = aamd_nthreads();
    int status = 1;
    EGLDisplay dpy = EGL_NO_DISPLAY;
    EGLContext ctx = EGL_NO_CONTEXT;

    aamd_log_open();
    aamd_log("AAMD@home GPU compute probe (OpenGL 4.3 compute fallback)");
    aamd_log("n=%u a=%.1f cpu_threads=%u", n, AAMD_SAXPY_A, threads);
    aamd_notify("AAMD probe: GL compute saxpy");

    float *x = (float *)malloc((size_t)n * sizeof(float));
    float *y_cpu = (float *)malloc((size_t)n * sizeof(float));
    float *y_gpu = (float *)malloc((size_t)n * sizeof(float));
    if (!x || !y_cpu || !y_gpu) {
        aamd_log("FAIL: malloc");
        goto done;
    }
    aamd_saxpy_init(x, y_cpu, n);
    memcpy(y_gpu, y_cpu, (size_t)n * sizeof(float));

    double t0 = aamd_now_s();
    aamd_saxpy_cpu(AAMD_SAXPY_A, x, y_cpu, n, threads);
    double t_cpu = aamd_now_s() - t0;
    aamd_log("cpu %u-thread: %.4f s", threads, t_cpu);

    if (!egl_setup(&dpy, &ctx)) {
        aamd_log("RESULT: FAIL (EGL/GL)");
        goto done;
    }
    GLuint prog = compile_compute();
    if (!prog) {
        aamd_log("RESULT: FAIL (shader)");
        goto done;
    }

    GLuint ssbo[2];
    glGenBuffers(2, ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)n * 4, x, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)n * 4, y_gpu, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo[1]);
    glUseProgram(prog);
    glUniform1ui(glGetUniformLocation(prog, "n"), n);
    glUniform1f(glGetUniformLocation(prog, "a"), AAMD_SAXPY_A);

    uint32_t groups = (n + AAMD_SAXPY_WG - 1u) / AAMD_SAXPY_WG;
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();

    memcpy(y_gpu, y_cpu, (size_t)n * sizeof(float));
    aamd_saxpy_init(x, y_gpu, n);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[1]);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)n * 4, y_gpu);

    t0 = aamd_now_s();
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();
    double t_gpu = aamd_now_s() - t0;

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)n * 4, y_gpu);

    float max_err = 0.0f;
    uint32_t mismatches = aamd_saxpy_compare(y_gpu, y_cpu, n, &max_err);
    double gb = (double)n * sizeof(float) * 3.0 / (1024.0 * 1024.0 * 1024.0);
    aamd_log("gpu saxpy: %.4f s  (%.2f GiB/s effective)", t_gpu, gb / t_gpu);
    aamd_log("gpu vs cpu: mismatches=%u max_abs_err=%g  speedup=%.2fx", mismatches,
             (double)max_err, t_cpu / t_gpu);

    if (mismatches == 0) {
        aamd_log("RESULT: PASS (GL 4.3 compute saxpy)");
        aamd_notify("AAMD probe PASS (GL)");
        status = 0;
    } else {
        aamd_log("RESULT: FAIL (GPU/CPU mismatch)");
        aamd_notify("AAMD probe FAIL (mismatch)");
    }
    glDeleteProgram(prog);
    glDeleteBuffers(2, ssbo);

done:
    if (dpy != EGL_NO_DISPLAY)
        eglTerminate(dpy);
    free(x);
    free(y_cpu);
    free(y_gpu);
    aamd_log("log: %s", aamd_log_path());
    aamd_log_close();
    (void)ctx;
    return status;
}
