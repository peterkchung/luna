#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat float fragInstrumentId;

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

layout(location = 0) out vec4 outColor;

// Seven-segment encoding: bits 6..0 = a,b,c,d,e,f,g
//  _a_
// |f  |b
//  _g_
// |e  |c
//  _d_
const int SEG[10] = int[10](
    0x7E, 0x30, 0x6D, 0x79, 0x33,
    0x5B, 0x5F, 0x70, 0x7F, 0x7B
);

float renderDigit(vec2 p, int d) {
    if (d < 0 || d > 9) return 0.0;
    int code = SEG[d];

    float t = 0.15;
    float m = 0.08;
    float r = 0.0;

    // a: top horizontal
    if ((code & 0x40) != 0 && p.y > (1.0 - t) && p.x > m && p.x < (1.0 - m)) r = 1.0;
    // b: top-right vertical
    if ((code & 0x20) != 0 && p.x > (1.0 - t) && p.y > 0.5 && p.y < (1.0 - m)) r = 1.0;
    // c: bottom-right vertical
    if ((code & 0x10) != 0 && p.x > (1.0 - t) && p.y > m && p.y < 0.5) r = 1.0;
    // d: bottom horizontal
    if ((code & 0x08) != 0 && p.y < t && p.x > m && p.x < (1.0 - m)) r = 1.0;
    // e: bottom-left vertical
    if ((code & 0x04) != 0 && p.x < t && p.y > m && p.y < 0.5) r = 1.0;
    // f: top-left vertical
    if ((code & 0x02) != 0 && p.x < t && p.y > 0.5 && p.y < (1.0 - m)) r = 1.0;
    // g: middle horizontal
    if ((code & 0x01) != 0 && p.y > (0.5 - t * 0.5) && p.y < (0.5 + t * 0.5) && p.x > m && p.x < (1.0 - m)) r = 1.0;

    return r;
}

// Render a minus sign (just the middle segment)
float renderMinus(vec2 p) {
    float t = 0.15;
    float m = 0.08;
    if (p.y > (0.5 - t * 0.5) && p.y < (0.5 + t * 0.5) && p.x > m && p.x < (1.0 - m))
        return 1.0;
    return 0.0;
}

// Render dim "ghost" segments (all 7 segments at low intensity)
float renderGhost(vec2 p) {
    return renderDigit(p, 8) * 0.1;
}

float renderNumber(vec2 uv, float value, int numDigits, bool showSign) {
    float digitW = 0.6;
    float gap = 0.1;
    float cellW = digitW + gap;
    float totalW = float(numDigits) * cellW;

    float cellX = uv.x * totalW;
    int charIndex = int(floor(cellX / cellW));
    if (charIndex < 0 || charIndex >= numDigits) return 0.0;

    float localX = (cellX - float(charIndex) * cellW) / digitW;
    if (localX > 1.0 || localX < 0.0) return 0.0;

    vec2 digitUV = vec2(localX, uv.y);

    int absVal = int(min(abs(value), 9999999.0));

    // Sign character
    if (showSign && charIndex == 0) {
        if (value < 0.0) return renderMinus(digitUV);
        return 0.0;
    }

    int digitOffset = showSign ? charIndex - 1 : charIndex;
    int digitCount = showSign ? numDigits - 1 : numDigits;
    int digitPos = digitCount - 1 - digitOffset;

    int digitVal = (absVal / int(pow(10.0, float(digitPos)))) % 10;

    // Leading zero suppression with ghost segments
    if (digitPos > 0 && absVal < int(pow(10.0, float(digitPos)))) {
        return renderGhost(digitUV);
    }

    return renderDigit(digitUV, digitVal);
}

float renderBar(vec2 uv, float fill) {
    float border = 0.06;
    float inner = 0.2;

    // Outline
    float outline = 0.0;
    if (uv.x < border || uv.x > (1.0 - border) ||
        uv.y < border || uv.y > (1.0 - border))
        outline = 0.4;

    // Fill region
    float barFill = 0.0;
    if (uv.x > inner && uv.x < (1.0 - inner) &&
        uv.y > border * 2.0 && uv.y < fill)
        barFill = 1.0;

    // Tick marks at 25%, 50%, 75%
    float ticks = 0.0;
    for (int i = 1; i <= 3; i++) {
        float tickY = float(i) * 0.25;
        if (abs(uv.y - tickY) < 0.008 && uv.x > border && uv.x < (1.0 - border))
            ticks = 0.3;
    }

    return max(max(outline, barFill), ticks);
}

void main() {
    int id = int(fragInstrumentId + 0.5);
    float bgAlpha = 0.3;
    vec4 color = vec4(0.0, 0.0, 0.0, bgAlpha);

    if (id == 0) {
        // Altitude: green
        float lit = renderNumber(fragUV, pc.altitude, 7, false);
        vec3 c = vec3(0.0, 1.0, 0.3);
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }
    else if (id == 1) {
        // Vertical speed: cyan
        float lit = renderNumber(fragUV, pc.verticalSpeed, 6, true);
        vec3 c = vec3(0.0, 0.9, 1.0);
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }
    else if (id == 2) {
        // Surface speed: yellow
        float lit = renderNumber(fragUV, pc.surfaceSpeed, 5, false);
        vec3 c = vec3(1.0, 0.9, 0.1);
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }
    else if (id == 3) {
        // Throttle bar: orange
        float lit = renderBar(fragUV, pc.throttle);
        vec3 c = vec3(1.0, 0.5, 0.0);
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }
    else if (id == 4) {
        // Fuel bar: green-to-red gradient
        float lit = renderBar(fragUV, pc.fuelFraction);
        vec3 fullColor = vec3(0.0, 1.0, 0.3);
        vec3 emptyColor = vec3(1.0, 0.2, 0.0);
        vec3 c = mix(emptyColor, fullColor, pc.fuelFraction);
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }

    outColor = color;
}
