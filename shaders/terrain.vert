#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in ivec2 inNormalOct;  // snorm16x2 octahedron-encoded
layout(location = 2) in float inHeight;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraOffset;
    float _pad0;
    vec4 sunDirection;
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out float fragHeight;

vec3 octDecode(vec2 e) {
    vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return normalize(n);
}

void main() {
    vec3 viewPos = inPosition + pc.cameraOffset;
    gl_Position = pc.viewProj * vec4(viewPos, 1.0);
    fragNormal = octDecode(vec2(inNormalOct) / 32767.0);
    fragHeight = inHeight;
}
