#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPosition;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 sunDirection;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 sunDir = normalize(pc.sunDirection.xyz);

    // Diffuse lighting
    float diffuse = max(dot(normal, sunDir), 0.0);
    float ambient = 0.05;

    // Height-based color: darker lowlands, lighter highlands
    // Use magnitude from origin as proxy for elevation
    float height = length(fragPosition);
    float heightFactor = clamp((height - 1737000.0) / 1000.0, 0.0, 1.0);
    vec3 baseColor = mix(vec3(0.25, 0.25, 0.25), vec3(0.55, 0.55, 0.52), heightFactor);

    vec3 color = baseColor * (ambient + diffuse);
    outColor = vec4(color, 1.0);
}
