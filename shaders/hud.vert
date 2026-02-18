#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in float inInstrumentId;

layout(push_constant) uniform PushConstants {
    float altitude;
    float verticalSpeed;
    float surfaceSpeed;
    float throttle;
    float fuelFraction;
    float aspectRatio;
    float _pad0;
    float _pad1;
} pc;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out flat float fragInstrumentId;

void main() {
    // Screen UV (0-1, origin bottom-left) to Vulkan NDC (-1 to +1, Y-down)
    vec2 ndc = inPosition * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, 0.0, 1.0);
    fragUV = inUV;
    fragInstrumentId = inInstrumentId;
}
