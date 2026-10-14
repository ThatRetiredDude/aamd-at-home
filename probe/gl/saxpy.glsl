#version 430
/* AAMD@home GL 4.3 compute fallback: y = a*x + y. */
layout(local_size_x = 64) in;

layout(std430, binding = 0) readonly buffer X { float x[]; };
layout(std430, binding = 1) buffer Y { float y[]; };

uniform uint n;
uniform float a;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < n)
        y[i] = a * x[i] + y[i];
}
