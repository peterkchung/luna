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

    // Lat/lon gridlines from sphere direction
    vec3 dir = normalize(fragSphereDir);
    float lat = asin(clamp(dir.y, -1.0, 1.0));
    float lon = atan(dir.z, dir.x);

    // Grid every 10 degrees
    float spacing = radians(10.0);

    // Use fwidth on the raw dir components for stable derivatives
    float latDeriv = fwidth(lat);
    float lonDeriv = fwidth(lon);

    float latDist = abs(fract(lat / spacing + 0.5) - 0.5) * spacing;
    float lonDist = abs(fract(lon / spacing + 0.5) - 0.5) * spacing;

    float latLine = 1.0 - smoothstep(0.0, latDeriv * 2.0, latDist);
    float lonLine = 1.0 - smoothstep(0.0, lonDeriv * 2.0, lonDist);
    float grid = max(latLine, lonLine);

    vec3 gridColor = mix(baseColor, vec3(0.85, 0.85, 0.9), grid);

    vec3 color = gridColor * (ambient + diffuse);
    outColor = vec4(color, 1.0);
}
