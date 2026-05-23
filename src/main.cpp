#include <M5Unified.h>
#include <Preferences.h>
#include <math.h>

// ============================================================
// LockCracker - a pretend safe-cracking game for kids
// ============================================================

static constexpr uint32_t CRACK_DURATION_MS = 10000;   // 10 s
static constexpr float    MOTION_THRESHOLD  = 0.10f;   // g, sensitive
static constexpr int      RING_COUNT        = 4;
static constexpr int      SCR_W             = 320;
static constexpr int      SCR_H             = 240;

// Palette (RGB565)
static constexpr uint16_t COLOR_BG      = 0x0841;   // very dark blue
static constexpr uint16_t COLOR_TEXT    = 0xFFFF;
static constexpr uint16_t COLOR_OK      = 0x07E0;   // green
static constexpr uint16_t COLOR_FAIL    = 0xF800;   // red
static constexpr uint16_t COLOR_LOCKED  = 0xFFFF;
static constexpr uint16_t COLOR_ACCENT  = 0x9BDF;   // light purple
static constexpr uint16_t COLOR_DIM     = 0x52B6;   // dim grey-blue
static constexpr uint16_t COLOR_RING[RING_COUNT] = {
    0x5C9F,  // light blue
    0x4ABD,  // medium blue
    0xA275,  // light purple
    0x6018,  // dark purple
};

enum State {
    STATE_IDLE,
    STATE_CRACKING,
    STATE_RESULT,
    STATE_SETTINGS,
};

static State    state           = STATE_IDLE;
static uint32_t stateStartMs    = 0;
static bool     resultSuccess   = false;

// ====== Persistent settings ======
static Preferences prefs;
static uint8_t volumeLevel    = 160;   // 0..255
static uint8_t brightnessIndex = 3;    // 0..3 -> 25/50/75/100%
static const uint8_t kBrightTable[4] = { 64, 128, 192, 255 };

static void applySettings() {
    M5.Speaker.setVolume(volumeLevel);
    M5.Display.setBrightness(kBrightTable[brightnessIndex]);
}

static void loadSettings() {
    prefs.begin("mcc", true);
    volumeLevel     = prefs.getUChar("vol", 160);
    brightnessIndex = prefs.getUChar("bri", 3);
    if (brightnessIndex > 3) brightnessIndex = 3;
    prefs.end();
}

static void saveSettings() {
    prefs.begin("mcc", false);
    prefs.putUChar("vol", volumeLevel);
    prefs.putUChar("bri", brightnessIndex);
    prefs.end();
}

// ====== Canvas (off-screen sprite for flicker-free drawing) ======
static LGFX_Sprite canvas(&M5.Display);

// ====== Small UI primitives ======
static void drawBatteryIcon(int x, int y) {
    int level = M5.Power.getBatteryLevel();
    if (level < 0)   level = 0;
    if (level > 100) level = 100;

    uint16_t col = COLOR_TEXT;
    if (level < 20) col = 0xFC00; // orange
    if (level < 10) col = COLOR_FAIL;

    const int w = 26, h = 12;
    canvas.drawRect(x, y, w, h, col);
    canvas.fillRect(x + w, y + 3, 2, h - 6, col);
    int fill = (w - 4) * level / 100;
    canvas.fillRect(x + 2, y + 2, fill, h - 4, col);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", level);
    canvas.setTextColor(col, COLOR_BG);
    canvas.setTextDatum(textdatum_t::top_right);
    canvas.setFont(&fonts::Font2);
    canvas.drawString(buf, x - 4, y - 1);
    canvas.setTextDatum(textdatum_t::top_left);
}

static void drawGearIcon(int cx, int cy, uint16_t col) {
    // 8 teeth
    for (int i = 0; i < 8; ++i) {
        float a = i * (M_PI / 4.0f);
        int tx = cx + (int)(cosf(a) * 11);
        int ty = cy + (int)(sinf(a) * 11);
        canvas.fillRect(tx - 2, ty - 2, 4, 4, col);
    }
    canvas.fillCircle(cx, cy, 9, col);
    canvas.fillCircle(cx, cy, 4, COLOR_BG);
}

static void drawPowerIcon(int cx, int cy, uint16_t col) {
    // ring with gap at top + vertical bar
    canvas.drawCircle(cx, cy, 9, col);
    canvas.drawCircle(cx, cy, 8, col);
    canvas.drawCircle(cx, cy, 7, col);
    // gap at top
    canvas.fillRect(cx - 3, cy - 11, 7, 5, COLOR_BG);
    // vertical bar
    canvas.fillRect(cx - 1, cy - 11, 3, 9, col);
}

// ============================================================
// IDLE SCREEN
// ============================================================
static void drawIdle() {
    canvas.fillSprite(COLOR_BG);

    // Top bar: battery (right)
    drawBatteryIcon(290, 6);

    // Title
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(COLOR_ACCENT);
    canvas.setFont(&fonts::Font4);
    canvas.drawString("LOCK", SCR_W / 2, 70);
    canvas.setTextColor(COLOR_TEXT);
    canvas.drawString("CRACKER", SCR_W / 2, 105);

    // Decorative circle
    int cx = SCR_W / 2, cy = 150;
    canvas.drawCircle(cx, cy, 18, COLOR_ACCENT);
    canvas.drawCircle(cx, cy, 14, COLOR_RING[2]);
    canvas.fillCircle(cx, cy, 6, COLOR_RING[0]);

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(COLOR_DIM);
    canvas.drawString("Tap screen to crack", SCR_W / 2, 188);

    // Button hints with icons (above physical A and C buttons)
    drawGearIcon(64, 218, COLOR_ACCENT);
    drawPowerIcon(256, 218, COLOR_ACCENT);

    canvas.pushSprite(0, 0);
}

// ============================================================
// CRACKING ANIMATION
// ============================================================
struct Ring {
    int   outerR;
    int   innerR;
    float startAngle;
    float speed;        // rad/s (sign = direction)
    bool  locked;
    float lockAngle;
    int   segments;     // number of arc gaps
    float gapWidth;     // gap arc in rad
};

static Ring rings[RING_COUNT];
static bool  motionDetected     = false;
static bool  baselineSet        = false;
static float accelBaseline[3]   = { 0, 0, 0 };
static uint32_t lastImuMs       = 0;
static int   nextLockIdx        = 0;

static void initRings() {
    const int rOut = 110;
    for (int i = 0; i < RING_COUNT; ++i) {
        rings[i].outerR     = rOut - i * 18;
        rings[i].innerR     = rings[i].outerR - 14;
        rings[i].startAngle = i * 0.6f;
        rings[i].speed      = (i % 2 == 0 ? 2.4f : -1.9f) - i * 0.15f;
        rings[i].locked     = false;
        rings[i].lockAngle  = 0.0f;
        rings[i].segments   = 3;
        rings[i].gapWidth   = (float)(M_PI / 7.5f);
    }
    nextLockIdx = 0;
}

static void drawCrackFrame(uint32_t elapsedMs) {
    canvas.fillSprite(COLOR_BG);

    drawBatteryIcon(290, 6);

    // Title hint
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setTextColor(COLOR_DIM, COLOR_BG);
    canvas.setFont(&fonts::Font2);
    canvas.drawString("cracking...", 12, 6);

    const int cx = SCR_W / 2;
    const int cy = SCR_H / 2;

    // Determine which rings should be locked by now.
    // Locks at progressive points: 0.25, 0.50, 0.75, 1.00 of the duration.
    float progress = elapsedMs / (float)CRACK_DURATION_MS;
    if (progress > 1.0f) progress = 1.0f;

    while (nextLockIdx < RING_COUNT) {
        float lockAt = (nextLockIdx + 1) / (float)RING_COUNT;
        if (progress < lockAt) break;
        // Snap to a notch.
        Ring& r = rings[nextLockIdx];
        float current = r.startAngle + r.speed * (elapsedMs / 1000.0f);
        float notch = (float)(2.0 * M_PI) / r.segments;
        float snapped = roundf(current / notch) * notch;
        r.locked    = true;
        r.lockAngle = snapped;
        // Click feedback
        M5.Speaker.tone(1400 + nextLockIdx * 220, 70);
        ++nextLockIdx;
    }

    // Draw all rings
    for (int i = 0; i < RING_COUNT; ++i) {
        Ring& r = rings[i];
        float base = r.locked
                     ? r.lockAngle
                     : r.startAngle + r.speed * (elapsedMs / 1000.0f);
        uint16_t col = r.locked ? COLOR_LOCKED : COLOR_RING[i];

        float segArc = (float)(2.0 * M_PI) / r.segments - r.gapWidth;
        for (int s = 0; s < r.segments; ++s) {
            float a0 = base + s * (float)(2.0 * M_PI) / r.segments;
            float a1 = a0 + segArc;
            // fillArc takes degrees, 0 = right (3 o'clock), CW positive.
            float deg0 = a0 * 180.0f / (float)M_PI;
            float deg1 = a1 * 180.0f / (float)M_PI;
            canvas.fillArc(cx, cy, r.outerR, r.innerR, deg0, deg1, col);
        }
    }

    // Center indicator - red pulse if movement detected, otherwise calm pulse
    int pulseR = 9 + (int)(3 * sinf(elapsedMs * 0.009f));
    uint16_t centerCol = motionDetected ? COLOR_FAIL : COLOR_RING[2];
    canvas.fillCircle(cx, cy, pulseR, centerCol);
    canvas.drawCircle(cx, cy, pulseR + 2, COLOR_ACCENT);

    // Progress hint at bottom
    int barW = 220;
    int barX = (SCR_W - barW) / 2;
    int barY = 220;
    canvas.drawRect(barX, barY, barW, 6, COLOR_DIM);
    canvas.fillRect(barX + 1, barY + 1, (barW - 2) * progress, 4,
                    motionDetected ? COLOR_FAIL : COLOR_RING[0]);

    canvas.pushSprite(0, 0);
}

// ============================================================
// RESULT (OPEN / FAIL)
// ============================================================
// ----- Non-blocking tone sequencer -----
struct ToneStep { uint16_t freq; uint16_t durMs; };

static const ToneStep kOpenSeq[] = {
    { 523, 120 }, { 659, 120 }, { 784, 120 }, { 1046, 120 }, { 1318, 320 }
};
static const ToneStep kAlarmSeq[] = {
    { 880, 140 }, { 440, 140 }, { 880, 140 }, { 440, 140 },
    { 880, 140 }, { 440, 140 }, { 880, 140 }, { 440, 140 },
    { 880, 140 }, { 440, 140 }, { 880, 140 }, { 440, 140 },
};

static const ToneStep* seqSteps    = nullptr;
static int             seqLen      = 0;
static int             seqIdx      = -1;
static uint32_t        seqStepStart = 0;

static void startToneSequence(const ToneStep* steps, int len) {
    seqSteps     = steps;
    seqLen       = len;
    seqIdx       = -1;
    seqStepStart = 0;
}

static void stopToneSequence() {
    seqSteps = nullptr;
    seqLen   = 0;
    seqIdx   = -1;
    M5.Speaker.stop();
}

static void tickToneSequence() {
    if (!seqSteps) return;
    uint32_t now = millis();
    uint16_t curDur = (seqIdx >= 0) ? seqSteps[seqIdx].durMs : 0;
    // Small inter-tone gap so consecutive notes are audible.
    if (now - seqStepStart < (uint32_t)(curDur + 25)) return;

    int next = seqIdx + 1;
    if (next >= seqLen) {
        seqSteps = nullptr;
        return;
    }
    seqIdx       = next;
    seqStepStart = now;
    M5.Speaker.tone(seqSteps[seqIdx].freq, seqSteps[seqIdx].durMs);
}

static void drawResultStatic(bool success) {
    uint16_t col = success ? COLOR_OK : COLOR_FAIL;
    canvas.fillSprite(col);
    canvas.setTextColor(COLOR_BG);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setFont(&fonts::Font4);
    canvas.setTextSize(2);
    canvas.drawString(success ? "OPEN" : "ALARM", SCR_W / 2, 110);
    canvas.setTextSize(1);
    canvas.setFont(&fonts::Font2);
    canvas.drawString(success ? "tap to continue" : "movement detected",
                      SCR_W / 2, 180);
    drawBatteryIcon(290, 6);
    canvas.pushSprite(0, 0);
}

static void playResultAnimation(bool success) {
    uint16_t col = success ? COLOR_OK : COLOR_FAIL;
    for (int f = 0; f < 4; ++f) {
        canvas.fillSprite(f % 2 == 0 ? col : COLOR_BG);
        canvas.setTextColor(f % 2 == 0 ? COLOR_BG : col);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setFont(&fonts::Font4);
        canvas.setTextSize(2);
        canvas.drawString(success ? "OPEN" : "ALARM", SCR_W / 2, 110);
        canvas.setTextSize(1);
        canvas.pushSprite(0, 0);
        delay(140);
    }
    drawResultStatic(success);
}

// ============================================================
// MOTION DETECTION
// ============================================================
static void resetMotion() {
    motionDetected = false;
    baselineSet    = false;
}

static void sampleMotion() {
    float ax = 0, ay = 0, az = 0;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) return;
    if (!baselineSet) {
        accelBaseline[0] = ax;
        accelBaseline[1] = ay;
        accelBaseline[2] = az;
        baselineSet = true;
        return;
    }
    float dx = ax - accelBaseline[0];
    float dy = ay - accelBaseline[1];
    float dz = az - accelBaseline[2];
    float mag = sqrtf(dx * dx + dy * dy + dz * dz);
    // Slow follow so slow tilt doesn't trigger.
    accelBaseline[0] = accelBaseline[0] * 0.94f + ax * 0.06f;
    accelBaseline[1] = accelBaseline[1] * 0.94f + ay * 0.06f;
    accelBaseline[2] = accelBaseline[2] * 0.94f + az * 0.06f;
    if (mag > MOTION_THRESHOLD) motionDetected = true;
}

// ============================================================
// SETTINGS SCREEN
// ============================================================
struct SettingsLayout {
    static constexpr int volBarX = 20;
    static constexpr int volBarY = 80;
    static constexpr int volBarW = 280;
    static constexpr int volBarH = 22;

    static constexpr int briRowY = 150;
    static constexpr int briRowH = 38;
    static constexpr int briColW = 65;
    static constexpr int briColGap = 5;
    static constexpr int briStartX = 20;
};

static void drawSettings() {
    canvas.fillSprite(COLOR_BG);
    drawBatteryIcon(290, 6);

    canvas.setTextColor(COLOR_TEXT, COLOR_BG);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setFont(&fonts::Font4);
    canvas.drawString("Settings", 16, 8);

    // ---- Volume ----
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(COLOR_ACCENT, COLOR_BG);
    canvas.drawString("Volume", SettingsLayout::volBarX, 58);

    int volPct = (volumeLevel * 100) / 255;
    char vbuf[8];
    snprintf(vbuf, sizeof(vbuf), "%d%%", volPct);
    canvas.setTextColor(COLOR_TEXT, COLOR_BG);
    canvas.setTextDatum(textdatum_t::top_right);
    canvas.drawString(vbuf, 300, 58);
    canvas.setTextDatum(textdatum_t::top_left);

    canvas.drawRect(SettingsLayout::volBarX, SettingsLayout::volBarY,
                    SettingsLayout::volBarW, SettingsLayout::volBarH, COLOR_TEXT);
    int fillW = (SettingsLayout::volBarW - 4) * volumeLevel / 255;
    canvas.fillRect(SettingsLayout::volBarX + 2, SettingsLayout::volBarY + 2,
                    fillW, SettingsLayout::volBarH - 4, COLOR_RING[2]);

    // ---- Brightness ----
    canvas.setTextColor(COLOR_ACCENT, COLOR_BG);
    canvas.drawString("Brightness", SettingsLayout::briStartX, 128);
    static const char* labels[4] = { "25%", "50%", "75%", "100%" };
    for (int i = 0; i < 4; ++i) {
        int x = SettingsLayout::briStartX +
                i * (SettingsLayout::briColW + SettingsLayout::briColGap);
        bool sel = (brightnessIndex == i);
        uint16_t boxBG = sel ? COLOR_RING[0] : COLOR_BG;
        uint16_t boxFG = sel ? COLOR_BG     : COLOR_TEXT;
        canvas.fillRect(x, SettingsLayout::briRowY,
                        SettingsLayout::briColW, SettingsLayout::briRowH, boxBG);
        canvas.drawRect(x, SettingsLayout::briRowY,
                        SettingsLayout::briColW, SettingsLayout::briRowH, COLOR_TEXT);
        canvas.setTextColor(boxFG, boxBG);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setFont(&fonts::Font2);
        canvas.drawString(labels[i],
                          x + SettingsLayout::briColW / 2,
                          SettingsLayout::briRowY + SettingsLayout::briRowH / 2);
    }
    canvas.setTextColor(COLOR_TEXT, COLOR_BG);
    canvas.setTextDatum(textdatum_t::top_left);

    // Bottom hints
    drawGearIcon(64, 218, COLOR_ACCENT);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(COLOR_DIM, COLOR_BG);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.drawString("Back", 64, 232);
    drawPowerIcon(256, 218, COLOR_ACCENT);
    canvas.drawString("Off", 256, 232);
    canvas.setTextDatum(textdatum_t::top_left);

    canvas.pushSprite(0, 0);
}

static bool handleSettingsTouch(int tx, int ty, bool freshPress) {
    bool changed = false;

    // Volume bar (slightly enlarged hit area for drag).
    if (tx >= SettingsLayout::volBarX - 4 &&
        tx <= SettingsLayout::volBarX + SettingsLayout::volBarW + 4 &&
        ty >= SettingsLayout::volBarY - 8 &&
        ty <= SettingsLayout::volBarY + SettingsLayout::volBarH + 8) {
        int local = tx - (SettingsLayout::volBarX + 2);
        int range = SettingsLayout::volBarW - 4;
        if (local < 0)     local = 0;
        if (local > range) local = range;
        uint8_t newVol = (uint8_t)((long)local * 255 / range);
        if (newVol != volumeLevel) {
            volumeLevel = newVol;
            applySettings();
            changed = true;
        }
    }

    // Brightness chunks (only react to fresh press).
    if (freshPress &&
        ty >= SettingsLayout::briRowY &&
        ty <= SettingsLayout::briRowY + SettingsLayout::briRowH) {
        for (int i = 0; i < 4; ++i) {
            int x = SettingsLayout::briStartX +
                    i * (SettingsLayout::briColW + SettingsLayout::briColGap);
            if (tx >= x && tx <= x + SettingsLayout::briColW) {
                if (brightnessIndex != i) {
                    brightnessIndex = (uint8_t)i;
                    applySettings();
                    changed = true;
                    M5.Speaker.tone(1200, 60);
                }
                break;
            }
        }
    }
    return changed;
}

// ============================================================
// POWER OFF
// ============================================================
static void powerOff() {
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(COLOR_TEXT);
    canvas.setFont(&fonts::Font4);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.drawString("Goodbye", SCR_W / 2, SCR_H / 2);
    canvas.pushSprite(0, 0);
    delay(700);
    M5.Power.powerOff();
    // If AXP can't cut power (USB-powered): fall back to deep sleep.
    M5.Display.sleep();
    esp_deep_sleep_start();
}

// ============================================================
// STATE TRANSITIONS
// ============================================================
static void enterState(State s) {
    state = s;
    stateStartMs = millis();
    switch (s) {
        case STATE_IDLE:
            drawIdle();
            break;
        case STATE_CRACKING:
            initRings();
            resetMotion();
            lastImuMs = 0;
            break;
        case STATE_RESULT:
            // drawing handled by caller (after playing intro animation)
            break;
        case STATE_SETTINGS:
            drawSettings();
            break;
    }
}

// ============================================================
// ARDUINO ENTRY POINTS
// ============================================================
void setup() {
    auto cfg = M5.config();
    cfg.clear_display = true;
    M5.begin(cfg);

    // M5Unified may leave the backlight at 0 after begin() on Core 2 —
    // force it on before any draw so the panel actually lights up.
    M5.Display.setBrightness(255);
    M5.Display.fillScreen(COLOR_BG);

    // Off-screen sprite: prefer internal SRAM, fall back to PSRAM.
    canvas.setColorDepth(16);
    canvas.setPsram(false);
    if (!canvas.createSprite(SCR_W, SCR_H)) {
        canvas.setPsram(true);
        canvas.createSprite(SCR_W, SCR_H);
    }

    loadSettings();
    applySettings();

    enterState(STATE_IDLE);
}

static bool prevTouchPressed = false;
static uint32_t lastIdleRefresh = 0;

void loop() {
    M5.update();
    tickToneSequence();

    auto td = M5.Touch.getDetail();
    bool touchNow   = td.isPressed();
    bool touchFresh = touchNow && !prevTouchPressed;
    int  tx = td.x;
    int  ty = td.y;

    switch (state) {
        case STATE_IDLE: {
            // Display tap (touch only registers inside 0..240 vertically).
            if (touchFresh && ty < SCR_H) {
                enterState(STATE_CRACKING);
                break;
            }
            if (M5.BtnA.wasPressed()) {
                enterState(STATE_SETTINGS);
                break;
            }
            if (M5.BtnC.wasPressed()) {
                powerOff();
            }
            // Refresh battery indicator periodically.
            if (millis() - lastIdleRefresh > 5000) {
                drawIdle();
                lastIdleRefresh = millis();
            }
            break;
        }

        case STATE_CRACKING: {
            uint32_t elapsed = millis() - stateStartMs;

            // Sample IMU every ~25 ms.
            if (millis() - lastImuMs >= 25) {
                lastImuMs = millis();
                sampleMotion();
            }

            drawCrackFrame(elapsed);

            if (elapsed >= CRACK_DURATION_MS) {
                resultSuccess = !motionDetected;
                enterState(STATE_RESULT);
                playResultAnimation(resultSuccess);  // short blocking flash
                if (resultSuccess) {
                    startToneSequence(kOpenSeq,
                                      sizeof(kOpenSeq) / sizeof(kOpenSeq[0]));
                } else {
                    startToneSequence(kAlarmSeq,
                                      sizeof(kAlarmSeq) / sizeof(kAlarmSeq[0]));
                }
            }
            break;
        }

        case STATE_RESULT: {
            // Tap or A/C to leave (after a short guard so the user can see it).
            uint32_t age = millis() - stateStartMs;
            bool returnNow = false;
            if (age > 600 && (touchFresh || M5.BtnA.wasPressed())) returnNow = true;
            if (M5.BtnC.wasPressed()) { stopToneSequence(); powerOff(); }
            // Auto-return after ~4s
            if (age > 4500) returnNow = true;
            if (returnNow) {
                stopToneSequence();
                enterState(STATE_IDLE);
            }
            break;
        }

        case STATE_SETTINGS: {
            if (touchNow && ty < SCR_H) {
                if (handleSettingsTouch(tx, ty, touchFresh)) {
                    drawSettings();
                }
            }
            // Released: save (debounced — saves only on release after change).
            static bool dirty = false;
            if (touchNow && ty < SCR_H) {
                // Mark dirty whenever the user is touching (cheap).
                dirty = true;
            }
            if (!touchNow && prevTouchPressed && dirty) {
                saveSettings();
                dirty = false;
            }

            if (M5.BtnA.wasPressed()) {
                if (dirty) { saveSettings(); dirty = false; }
                enterState(STATE_IDLE);
            }
            if (M5.BtnC.wasPressed()) {
                if (dirty) { saveSettings(); dirty = false; }
                powerOff();
            }
            break;
        }
    }

    prevTouchPressed = touchNow;
    delay(8);
}
