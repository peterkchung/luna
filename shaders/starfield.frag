#version 450

layout(location = 0) in float fragBrightness;
layout(location = 0) out vec4 outColor;

void main() {
    // Slight warm tint for brighter stars
    vec3 starColor = mix(vec3(0.8, 0.85, 1.0), vec3(1.0, 0.95, 0.85), fragBrightness);
    outColor = vec4(starColor, 0.3 + 0.7 * fragBrightness);
}
