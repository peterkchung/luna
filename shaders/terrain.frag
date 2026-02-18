#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in float fragHeight;
layout(location = 2) in vec3 fragSphereDir;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraOffset;
    float _pad0;
    vec4 sunDirection;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 sunDir = normalize(pc.sunDirection.xyz);

    float diffuse = max(dot(normal, sunDir), 0.0);
    float ambient = 0.12;

    vec3 baseColor = vec3(0.6, 0.6, 0.58);

    // Topographic contour lines from elevation data.
    // Wide smoothstep transition softens contour lines at LOD boundaries where
    // adjacent patches interpolate heights at different resolutions.
    float contourSpacing = 500.0;
    float hDeriv = fwidth(fragHeight);
    float contourWidth = max(hDeriv * 3.0, contourSpacing * 0.008);
    float hDist = abs(fract(fragHeight / contourSpacing + 0.5) - 0.5) * contourSpacing;
    float contour = 1.0 - smoothstep(0.0, contourWidth, hDist);
    baseColor = mix(baseColor, vec3(0.35, 0.30, 0.25), contour);

    vec3 color = baseColor * (ambient + diffuse);
    outColor = vec4(color, 1.0);
}
