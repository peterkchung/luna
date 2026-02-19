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
    float pitch;
    float roll;
    float heading;
    float timeToSurface;
    float progradeX;
    float progradeY;
    float flightPhase;
    float missionTime;
    float warningFlags;
    float progradeVisible;
    float tiltAngle;
    float _pad0;
    float _pad1;
    float _pad2;
} pc;

layout(location = 0) out vec4 outColor;

// ============================================================
// Seven-segment display
// ============================================================

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

    if ((code & 0x40) != 0 && p.y > (1.0 - t) && p.x > m && p.x < (1.0 - m)) r = 1.0;
    if ((code & 0x20) != 0 && p.x > (1.0 - t) && p.y > 0.5 && p.y < (1.0 - m)) r = 1.0;
    if ((code & 0x10) != 0 && p.x > (1.0 - t) && p.y > m && p.y < 0.5) r = 1.0;
    if ((code & 0x08) != 0 && p.y < t && p.x > m && p.x < (1.0 - m)) r = 1.0;
    if ((code & 0x04) != 0 && p.x < t && p.y > m && p.y < 0.5) r = 1.0;
    if ((code & 0x02) != 0 && p.x < t && p.y > 0.5 && p.y < (1.0 - m)) r = 1.0;
    if ((code & 0x01) != 0 && p.y > (0.5 - t * 0.5) && p.y < (0.5 + t * 0.5) && p.x > m && p.x < (1.0 - m)) r = 1.0;

    return r;
}

float renderMinus(vec2 p) {
    float t = 0.15;
    float m = 0.08;
    if (p.y > (0.5 - t * 0.5) && p.y < (0.5 + t * 0.5) && p.x > m && p.x < (1.0 - m))
        return 1.0;
    return 0.0;
}

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

    if (showSign && charIndex == 0) {
        if (value < 0.0) return renderMinus(digitUV);
        return 0.0;
    }

    int digitOffset = showSign ? charIndex - 1 : charIndex;
    int digitCount = showSign ? numDigits - 1 : numDigits;
    int digitPos = digitCount - 1 - digitOffset;

    int digitVal = (absVal / int(pow(10.0, float(digitPos)))) % 10;

    if (digitPos > 0 && absVal < int(pow(10.0, float(digitPos)))) {
        return renderGhost(digitUV);
    }

    return renderDigit(digitUV, digitVal);
}

float renderDashes(vec2 uv, int numDashes) {
    float digitW = 0.6;
    float gap = 0.1;
    float cellW = digitW + gap;
    float totalW = float(numDashes) * cellW;

    float cellX = uv.x * totalW;
    int charIndex = int(floor(cellX / cellW));
    if (charIndex < 0 || charIndex >= numDashes) return 0.0;

    float localX = (cellX - float(charIndex) * cellW) / digitW;
    if (localX > 1.0 || localX < 0.0) return 0.0;

    return renderMinus(vec2(localX, uv.y));
}

// ============================================================
// Bar gauge
// ============================================================

float renderBar(vec2 uv, float fill) {
    float border = 0.06;
    float inner = 0.2;

    float outline = 0.0;
    if (uv.x < border || uv.x > (1.0 - border) ||
        uv.y < border || uv.y > (1.0 - border))
        outline = 0.4;

    float barFill = 0.0;
    if (uv.x > inner && uv.x < (1.0 - inner) &&
        uv.y > border * 2.0 && uv.y < fill)
        barFill = 1.0;

    float ticks = 0.0;
    for (int i = 1; i <= 3; i++) {
        float tickY = float(i) * 0.25;
        if (abs(uv.y - tickY) < 0.008 && uv.x > border && uv.x < (1.0 - border))
            ticks = 0.3;
    }

    return max(max(outline, barFill), ticks);
}

// ============================================================
// 3x5 bitmap font
// ============================================================

const int CHAR_A = 0x2BED;
const int CHAR_B = 0x6BAE;
const int CHAR_C = 0x3923;
const int CHAR_D = 0x6B6E;
const int CHAR_E = 0x79A7;
const int CHAR_F = 0x79A4;
const int CHAR_G = 0x396B;
const int CHAR_H = 0x5BED;
const int CHAR_I = 0x7497;
const int CHAR_L = 0x4927;
const int CHAR_M = 0x5F6D;
const int CHAR_N = 0x5DDD;
const int CHAR_O = 0x2B6A;
const int CHAR_P = 0x6BA4;
const int CHAR_R = 0x6BAD;
const int CHAR_S = 0x388E;
const int CHAR_T = 0x7492;
const int CHAR_U = 0x5B6F;
const int CHAR_V = 0x5B52;
const int CHAR_W = 0x5B7A;

float renderBitmapChar(vec2 p, int bitmap) {
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 0.0;

    int col = int(p.x * 3.0);
    int row = int((1.0 - p.y) * 5.0);
    col = clamp(col, 0, 2);
    row = clamp(row, 0, 4);

    int bitIndex = (4 - row) * 3 + (2 - col);
    return ((bitmap >> bitIndex) & 1) != 0 ? 1.0 : 0.0;
}

float renderLabel(vec2 uv, int chars[4], int numChars) {
    float charW = 0.8;
    float gap = 0.3;
    float cellW = charW + gap;
    float totalW = float(numChars) * cellW - gap;

    float startX = (1.0 - totalW / (totalW + 0.5)) * 0.5;
    float scaledX = (uv.x - startX) / (1.0 - 2.0 * startX);

    float cellX = scaledX * float(numChars);
    int idx = int(floor(cellX));
    if (idx < 0 || idx >= numChars) return 0.0;

    float localX = fract(cellX) * (cellW / charW);
    if (localX > 1.0) return 0.0;

    return renderBitmapChar(vec2(localX, uv.y), chars[idx]);
}

float renderText(vec2 uv, int chars[4], int numChars) {
    float charW = 0.55;
    float gap = 0.2;
    float cellW = charW + gap;
    float totalW = float(numChars) * cellW - gap;

    float startX = 0.5 - totalW * 0.5;
    float x = uv.x - startX;
    if (x < 0.0 || x > totalW) return 0.0;

    float cellX = x / cellW;
    int idx = int(floor(cellX));
    if (idx < 0 || idx >= numChars) return 0.0;

    float cx = fract(cellX) * (cellW / charW);
    if (cx > 1.0) return 0.0;

    return renderBitmapChar(vec2(cx, uv.y), chars[idx]);
}

// ============================================================
// Attitude indicator
// ============================================================

vec4 renderAttitude(vec2 uv) {
    // Panel is 0.12w x 0.20h — compute pixel aspect for circular correction
    float panelAspect = 0.12 * pc.aspectRatio / 0.20;

    vec2 center = vec2(0.5);
    vec2 p = uv - center;
    p.x *= panelAspect;
    float dist = length(p);

    if (dist > 0.48) {
        if (dist > 0.48 && dist < 0.50) return vec4(0.7, 0.7, 0.7, 0.6);
        return vec4(0.0);
    }

    // Rotate by roll angle
    float cr = cos(-pc.roll);
    float sr = sin(-pc.roll);
    vec2 rotP = vec2(p.x * cr - p.y * sr, p.x * sr + p.y * cr);

    // Pitch shifts horizon (90deg = full radius)
    float pitchOffset = pc.pitch / radians(90.0) * 0.4;
    float horizonY = rotP.y + pitchOffset;

    // Sky/ground split
    vec3 skyColor = vec3(0.05, 0.08, 0.25);
    vec3 groundColor = vec3(0.35, 0.20, 0.10);
    vec3 bgColor = horizonY > 0.0 ? skyColor : groundColor;

    // Horizon line
    float horizLine = smoothstep(0.008, 0.003, abs(horizonY));
    bgColor = mix(bgColor, vec3(0.9), horizLine);

    // Pitch ladder at 10deg intervals
    for (int i = -8; i <= 8; i++) {
        if (i == 0) continue;
        float tickY = rotP.y + pitchOffset - float(i) * (10.0 / 90.0) * 0.4;
        float tickLen = (i % 3 == 0) ? 0.12 : 0.06;
        if (abs(tickY) < 0.003 && abs(rotP.x) < tickLen) {
            bgColor = mix(bgColor, vec3(0.8), 0.7);
        }
    }

    // Fixed aircraft symbol (T-shape at center)
    float sym = 0.0;
    if (abs(p.y) < 0.008 && abs(p.x) < 0.12 && abs(p.x) > 0.02) sym = 1.0;
    if (dist < 0.015) sym = 1.0;
    if (abs(p.x) < 0.005 && p.y > 0.0 && p.y < 0.06) sym = 1.0;
    bgColor = mix(bgColor, vec3(1.0, 0.8, 0.0), sym);

    // Roll indicator ticks
    float angle = atan(p.y, p.x);
    float rollTick = 0.0;
    for (int i = -3; i <= 3; i++) {
        float tickAngle = radians(90.0) + float(i) * radians(30.0);
        if (abs(angle - tickAngle) < 0.03 && dist > 0.42 && dist < 0.48) rollTick = 0.6;
    }

    float rollPointerAngle = radians(90.0) + pc.roll;
    if (abs(angle - rollPointerAngle) < 0.05 && dist > 0.44 && dist < 0.48) rollTick = 1.0;

    bgColor = mix(bgColor, vec3(1.0), rollTick);

    return vec4(bgColor, 0.85);
}

// ============================================================
// Heading compass
// ============================================================

vec4 renderCompass(vec2 uv) {
    // Panel is 0.60w x 0.04h — very wide, need aspect for character sizing
    float panelAspect = 0.60 * pc.aspectRatio / 0.04;

    vec3 color = vec3(0.0);
    float alpha = 0.3;

    float headingRange = 60.0;
    float hdg = pc.heading;

    float lit = 0.0;
    for (int i = -6; i <= 6; i++) {
        float deg = floor(hdg / 10.0) * 10.0 + float(i) * 10.0;
        float offset = (deg - hdg) / headingRange + 0.5;
        if (offset < 0.0 || offset > 1.0) continue;

        float tickDist = abs(uv.x - offset);
        bool major = (mod(deg, 30.0) < 0.5 || mod(deg, 30.0) > 29.5);
        float tickH = major ? 0.5 : 0.25;
        if (tickDist < 0.003 && uv.y < tickH) {
            lit = major ? 0.8 : 0.4;
        }
    }

    // Cardinal points — size characters to maintain ~3:5 pixel aspect
    for (int c = 0; c < 4; c++) {
        float cardinalDeg = float(c) * 90.0;
        float diff = cardinalDeg - hdg;
        if (diff > 180.0) diff -= 360.0;
        if (diff < -180.0) diff += 360.0;
        float offset = diff / headingRange + 0.5;
        if (offset < 0.05 || offset > 0.95) continue;

        float charUvH = 0.8;
        float charUvW = charUvH * 0.6 / panelAspect;

        float charX = (uv.x - offset + charUvW * 0.5) / charUvW;
        float charY = (uv.y - 0.35) / charUvH;
        if (charX >= 0.0 && charX <= 1.0 && charY >= 0.0 && charY <= 1.0) {
            int bitmap = (c == 0) ? CHAR_N : (c == 1) ? CHAR_E : (c == 2) ? CHAR_S : CHAR_W;
            float ch = renderBitmapChar(vec2(charX, charY), bitmap);
            if (ch > 0.0) lit = max(lit, ch);
        }
    }

    // Center marker triangle
    float triDist = abs(uv.x - 0.5);
    float triY = 1.0 - (triDist * 4.0);
    if (uv.y > triY && uv.y > 0.7 && triDist < 0.06) lit = max(lit, 1.0);

    if (uv.y < 0.06 || uv.y > 0.94) lit = max(lit, 0.3);

    color = vec3(0.0, 0.9, 0.5) * lit;
    return vec4(color, alpha + lit * 0.7);
}

// ============================================================
// Flight phase display
// ============================================================

vec4 renderPhaseDisplay(vec2 uv) {
    int phase = int(pc.flightPhase + 0.5);

    int chars[4];
    int numChars = 3;
    vec3 color = vec3(0.0, 1.0, 0.4);

    if (phase == 0)      { chars = int[4](CHAR_O, CHAR_R, CHAR_B, 0); numChars = 3; }
    else if (phase == 1) { chars = int[4](CHAR_D, CHAR_S, CHAR_C, 0); numChars = 3; }
    else if (phase == 2) { chars = int[4](CHAR_P, CHAR_W, CHAR_R, 0); numChars = 3; }
    else if (phase == 3) { chars = int[4](CHAR_T, CHAR_R, CHAR_M, 0); numChars = 3; }
    else if (phase == 4) { chars = int[4](CHAR_L, CHAR_N, CHAR_D, 0); numChars = 3;
                           color = vec3(1.0, 0.8, 0.0); }
    else                 { chars = int[4](CHAR_F, CHAR_A, CHAR_I, CHAR_L); numChars = 4;
                           color = vec3(1.0, 0.15, 0.0); }

    float lit = renderText(uv, chars, numChars);
    return vec4(color * lit, 0.3 + lit * 0.7);
}

// ============================================================
// Mission elapsed time (MM:SS)
// ============================================================

vec4 renderMET(vec2 uv) {
    vec3 c = vec3(0.7, 0.7, 0.7);
    float bgAlpha = 0.3;

    float labelSplit = 0.70;
    bool inLabel = uv.y > labelSplit;

    if (inLabel) {
        vec2 labelUV = vec2(uv.x, (uv.y - labelSplit) / (1.0 - labelSplit));
        int chars[4] = int[4](CHAR_M, CHAR_E, CHAR_T, 0);
        float lit = renderLabel(labelUV, chars, 3) * 0.6;
        return vec4(c * lit, bgAlpha + lit * 0.7);
    }

    vec2 instrUV = vec2(uv.x, uv.y / labelSplit);

    int totalSec = int(pc.missionTime);
    int minutes = totalSec / 60;
    int seconds = totalSec % 60;

    // Layout: MM:SS
    float digitW = 0.5;
    float gap = 0.08;
    float colonW = 0.2;
    float cellW = digitW + gap;
    float totalW = 4.0 * cellW + colonW;
    float baseX = instrUV.x * totalW;

    float lit = 0.0;
    int positions[4] = int[4](minutes / 10, minutes % 10, seconds / 10, seconds % 10);

    for (int i = 0; i < 4; i++) {
        float posX;
        if (i < 2) posX = float(i) * cellW;
        else posX = 2.0 * cellW + colonW + float(i - 2) * cellW;

        float localX = (baseX - posX) / digitW;
        if (localX >= 0.0 && localX <= 1.0) {
            lit = renderDigit(vec2(localX, instrUV.y), positions[i]);
        }
    }

    // Colon
    float colonX = (baseX - 2.0 * cellW) / colonW;
    if (colonX > 0.2 && colonX < 0.8) {
        if (abs(instrUV.y - 0.65) < 0.06) lit = max(lit, 0.8);
        if (abs(instrUV.y - 0.35) < 0.06) lit = max(lit, 0.8);
    }

    return vec4(c * lit, bgAlpha + lit * 0.7);
}

// ============================================================
// Time to surface
// ============================================================

vec4 renderTTS(vec2 uv) {
    vec3 c = vec3(0.8, 0.5, 1.0);
    float bgAlpha = 0.3;

    float labelSplit = 0.75;
    bool inLabel = uv.y > labelSplit;

    if (inLabel) {
        vec2 labelUV = vec2(uv.x, (uv.y - labelSplit) / (1.0 - labelSplit));
        int chars[4] = int[4](CHAR_T, CHAR_T, CHAR_S, 0);
        float lit = renderLabel(labelUV, chars, 3) * 0.6;
        return vec4(c * lit, bgAlpha + lit * 0.7);
    }

    vec2 instrUV = vec2(uv.x, uv.y / labelSplit);

    float lit;
    if (pc.timeToSurface < 0.0) {
        lit = renderDashes(instrUV, 5);
    } else {
        lit = renderNumber(instrUV, min(pc.timeToSurface, 99999.0), 5, false);
    }

    return vec4(c * lit, bgAlpha + lit * 0.7);
}

// ============================================================
// Full-screen overlay (cockpit frame, crosshair, prograde, warnings)
// ============================================================

vec4 renderOverlay(vec2 uv) {
    float lit = 0.0;
    vec3 color = vec3(0.0);

    // Aspect-corrected UV for circular shapes
    vec2 auv = vec2((uv.x - 0.5) * pc.aspectRatio, uv.y - 0.5);

    // --- Cockpit frame: corner brackets ---
    float bracketLen = 0.06;
    float bracketThick = 0.002;
    float margin = 0.02;

    if ((uv.x < margin + bracketLen && abs(uv.y - (1.0 - margin)) < bracketThick) ||
        (uv.y > 1.0 - margin - bracketLen && abs(uv.x - margin) < bracketThick)) {
        color = vec3(0.4, 0.5, 0.4); lit = 1.0;
    }
    if ((uv.x > 1.0 - margin - bracketLen && abs(uv.y - (1.0 - margin)) < bracketThick) ||
        (uv.y > 1.0 - margin - bracketLen && abs(uv.x - (1.0 - margin)) < bracketThick)) {
        color = vec3(0.4, 0.5, 0.4); lit = 1.0;
    }
    if ((uv.x < margin + bracketLen && abs(uv.y - margin) < bracketThick) ||
        (uv.y < margin + bracketLen && abs(uv.x - margin) < bracketThick)) {
        color = vec3(0.4, 0.5, 0.4); lit = 1.0;
    }
    if ((uv.x > 1.0 - margin - bracketLen && abs(uv.y - margin) < bracketThick) ||
        (uv.y < margin + bracketLen && abs(uv.x - (1.0 - margin)) < bracketThick)) {
        color = vec3(0.4, 0.5, 0.4); lit = 1.0;
    }

    // --- Center crosshair ---
    float chSize = 0.015;
    float chGap = 0.005;
    float chThick = 0.0015;
    float adx = abs(auv.x);
    float ady = abs(auv.y);
    if (ady < chThick && adx > chGap && adx < chSize) {
        color = vec3(0.0, 1.0, 0.3); lit = 1.0;
    }
    if (adx < chThick && ady > chGap && ady < chSize) {
        color = vec3(0.0, 1.0, 0.3); lit = 1.0;
    }

    // --- Prograde marker ---
    if (pc.progradeVisible > 0.5) {
        vec2 progPos = vec2(pc.progradeX * 0.5, -pc.progradeY * 0.5);
        float progDist = length(auv - progPos);
        float progRadius = 0.012;

        if (abs(progDist - progRadius) < 0.0015) {
            color = vec3(0.0, 1.0, 0.3); lit = 1.0;
        }
        if (progDist < 0.003) {
            color = vec3(0.0, 1.0, 0.3); lit = 1.0;
        }
        float lineLen = 0.008;
        if (abs(auv.x - progPos.x) < 0.001 && auv.y - progPos.y > progRadius && auv.y - progPos.y < progRadius + lineLen) {
            color = vec3(0.0, 1.0, 0.3); lit = 1.0;
        }
        if (abs(auv.y - progPos.y) < 0.001 && progPos.x - auv.x > progRadius && progPos.x - auv.x < progRadius + lineLen) {
            color = vec3(0.0, 1.0, 0.3); lit = 1.0;
        }
        if (abs(auv.y - progPos.y) < 0.001 && auv.x - progPos.x > progRadius && auv.x - progPos.x < progRadius + lineLen) {
            color = vec3(0.0, 1.0, 0.3); lit = 1.0;
        }
    }

    // --- Warning indicators ---
    int warnings = int(pc.warningFlags + 0.5);
    float flashRate = 3.0;
    bool flashOn = fract(pc.missionTime * flashRate) > 0.35;

    if (warnings > 0 && flashOn) {
        float warnY = 0.18;
        float warnH = 0.035;

        int warnCount = 0;
        for (int b = 0; b < 3; b++) {
            if ((warnings & (1 << b)) != 0) warnCount++;
        }

        float warnW = 0.08;
        float warnGap = 0.02;
        float totalWarnW = float(warnCount) * (warnW + warnGap) - warnGap;
        float warnStartX = 0.5 - totalWarnW * 0.5;

        int warnIdx = 0;
        for (int b = 0; b < 3; b++) {
            if ((warnings & (1 << b)) == 0) continue;

            float wx = warnStartX + float(warnIdx) * (warnW + warnGap);
            if (uv.x >= wx && uv.x <= wx + warnW && uv.y >= warnY && uv.y <= warnY + warnH) {
                vec2 wuv = vec2((uv.x - wx) / warnW, (uv.y - warnY) / warnH);
                int wchars[4];
                int wnum;
                vec3 wcolor;

                if (b == 0) { wchars = int[4](CHAR_F, CHAR_U, CHAR_E, CHAR_L); wnum = 4; wcolor = vec3(1.0, 0.1, 0.0); }
                else if (b == 1) { wchars = int[4](CHAR_R, CHAR_A, CHAR_T, CHAR_E); wnum = 4; wcolor = vec3(1.0, 0.6, 0.0); }
                else { wchars = int[4](CHAR_T, CHAR_I, CHAR_L, CHAR_T); wnum = 4; wcolor = vec3(1.0, 0.6, 0.0); }

                float wlit = renderText(wuv, wchars, wnum);
                if (wlit > 0.0) {
                    color = wcolor;
                    lit = wlit;
                }
            }
            warnIdx++;
        }
    }

    if (lit > 0.0) return vec4(color, lit * 0.9);
    return vec4(0.0);
}

// ============================================================
// Main
// ============================================================

void main() {
    int id = int(fragInstrumentId + 0.5);
    float bgAlpha = 0.3;
    vec4 color = vec4(0.0, 0.0, 0.0, bgAlpha);

    // Labeled panels: top 25% = label, bottom 75% = instrument
    float labelSplit = 0.75;
    bool inLabel = fragUV.y > labelSplit;
    vec2 instrUV = vec2(fragUV.x, fragUV.y / labelSplit);
    vec2 labelUV = vec2(fragUV.x, (fragUV.y - labelSplit) / (1.0 - labelSplit));

    if (id == 0) {
        vec3 c = vec3(0.0, 1.0, 0.3);
        float lit;
        if (inLabel) {
            int chars[4] = int[4](CHAR_A, CHAR_L, CHAR_T, 0);
            lit = renderLabel(labelUV, chars, 3) * 0.6;
        } else {
            lit = renderNumber(instrUV, pc.altitude, 7, false);
        }
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }
    else if (id == 1) {
        vec3 c = vec3(0.0, 0.9, 1.0);
        float lit;
        if (inLabel) {
            int chars[4] = int[4](CHAR_V, CHAR_S, CHAR_P, CHAR_D);
            lit = renderLabel(labelUV, chars, 4) * 0.6;
        } else {
            lit = renderNumber(instrUV, pc.verticalSpeed, 6, true);
        }
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }
    else if (id == 2) {
        vec3 c = vec3(1.0, 0.9, 0.1);
        float lit;
        if (inLabel) {
            int chars[4] = int[4](CHAR_H, CHAR_S, CHAR_P, CHAR_D);
            lit = renderLabel(labelUV, chars, 4) * 0.6;
        } else {
            lit = renderNumber(instrUV, pc.surfaceSpeed, 5, false);
        }
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }
    else if (id == 3) {
        vec3 c = vec3(1.0, 0.5, 0.0);
        float lit;
        if (inLabel) {
            int chars[4] = int[4](CHAR_T, CHAR_H, CHAR_R, 0);
            lit = renderLabel(labelUV, chars, 3) * 0.6;
        } else {
            lit = renderBar(instrUV, pc.throttle);
        }
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }
    else if (id == 4) {
        vec3 fullColor = vec3(0.0, 1.0, 0.3);
        vec3 emptyColor = vec3(1.0, 0.2, 0.0);
        vec3 c = mix(emptyColor, fullColor, pc.fuelFraction);
        float lit;
        if (inLabel) {
            int chars[4] = int[4](CHAR_F, CHAR_U, CHAR_E, CHAR_L);
            lit = renderLabel(labelUV, chars, 4) * 0.6;
        } else {
            lit = renderBar(instrUV, pc.fuelFraction);
        }
        color = vec4(c * lit, bgAlpha + lit * 0.7);
    }
    else if (id == 5) {
        color = renderAttitude(fragUV);
    }
    else if (id == 6) {
        color = renderCompass(fragUV);
    }
    else if (id == 7) {
        color = renderPhaseDisplay(fragUV);
    }
    else if (id == 8) {
        color = renderMET(fragUV);
    }
    else if (id == 9) {
        color = renderTTS(fragUV);
    }
    else if (id == 10) {
        color = renderOverlay(fragUV);
    }

    outColor = color;
}
