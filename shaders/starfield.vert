#version 450

layout(location = 0) in vec3 inDirection;
layout(location = 1) in float inBrightness;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pc;

layout(location = 0) out float fragBrightness;

void main() {
    // Place star at large distance along direction (camera-relative, no translation)
    gl_Position = pc.viewProj * vec4(inDirection * 1000.0, 1.0);
    gl_PointSize = mix(1.0, 3.0, inBrightness);
    fragBrightness = inBrightness;
}
