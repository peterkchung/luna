#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in float inHeight;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraOffset;
    float _pad0;
    vec4 sunDirection;
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out float fragHeight;
layout(location = 2) out vec3 fragSphereDir;

void main() {
    vec3 viewPos = inPosition + pc.cameraOffset;
    gl_Position = pc.viewProj * vec4(viewPos, 1.0);
    fragNormal = inNormal;
    fragHeight = inHeight;
    fragSphereDir = inNormal;  // sphere direction = normal on flat terrain
}
