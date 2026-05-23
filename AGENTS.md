# AGENTS.md

Guidance for AI coding agents working on this repository.

## What this project is

**LockCracker** — firmware for an **M5Stack Core 2** that emulates a
pretend safe-cracking game for kids. The child holds the device against
any surface, taps the screen, and has to keep it perfectly still for
10 seconds or the "alarm" goes off. Single-file Arduino-style firmware
(`src/main.cpp`) using the `M5Unified` library. State machine with
idle → cracking → result → idle, plus a settings page.

This is a toy aimed at children. Tone of UI strings, sound, and
animation should stay playful — no realism, no scary alarms.

## Build & flash

PlatformIO Core is installed and on the user's PATH. Use plain `pio` from
the shell — no need to activate a venv.

```bash
pio run -e m5stack-core2            # compile only
pio run -e m5stack-core2 -t upload  # compile + flash via USB-CDC
pio device monitor                  # serial @ 115200
```

The board is `m5stack-core2`, framework `arduino`, platform
`espressif32`. The user runs Windows 11; both `cmd` / PowerShell and the
`Bash` tool work for invoking `pio`.

## File layout

```
platformio.ini   Board + framework + lib_deps (M5Unified)
src/main.cpp     Entire firmware
README.md        End-user docs
AGENTS.md        This file
```

Keep it that way unless the file grows past ~1000 lines or a clear
domain split (UI / audio / persistence) makes the code easier to read.
Splitting prematurely makes navigation worse.

## Code style

- One file is fine. Prefer `static` functions over a class hierarchy.
- Use `constexpr` constants at the top of the file for anything tunable.
  Don't bury magic numbers inside drawing code.
- Drawing happens into the off-screen `LGFX_Sprite canvas` and is then
  pushed with `canvas.pushSprite(0, 0)`. Don't draw to `M5.Display`
  directly — it flickers.
- Touch coordinates are read via `M5.Touch.getDetail()`. The capacitive
  buttons (A/B/C) live **below** the display panel; the touch panel itself
  is 0..240 on Y. Always check `ty < SCR_H` before treating a touch as a
  screen tap, otherwise an accidental A/C press will register as a
  display tap as well.
- IMU access: `M5.Imu.getAccel(&ax, &ay, &az)` returns `bool`. Always
  guard against the false return — the IMU might not have a fresh sample
  on every call.
- Audio: `M5.Speaker.tone(freq_hz, duration_ms)` is non-blocking. Pair
  with `delay(duration_ms + small_margin)` if you need sequential notes.
- Settings persistence: `Preferences` namespace `mcc`. Keys: `vol`
  (uint8, 0..255 raw speaker volume), `bri` (uint8, 0..3 index into
  `kBrightTable`). Save on touch-release, not on every frame.

## Things that have already been tried / are deliberate

- **`MOTION_THRESHOLD = 0.10f`** is intentionally aggressive. The user
  asked for high sensitivity — even a slight tap on the case is meant
  to trigger an alarm; that's what makes the game fun for kids. The
  baseline follows the current
  acceleration slowly (`0.94 * old + 0.06 * new`) so the device can be
  tilted gradually without triggering — only sudden changes count.
- **10-second cracking duration** with locks at 25/50/75/100 % is
  intentional. User confirmed during initial design.
- **`canvas.setColorDepth(16)` then `createSprite(320,240)`** uses the
  internal RAM (~150 KB). Core 2 has enough SRAM, so no need to force
  PSRAM allocation — and PSRAM would be slower for drawing-heavy frames.
- **`M5.Display.sleep(); esp_deep_sleep_start();`** is the fallback in
  `powerOff()` because `M5.Power.powerOff()` is a no-op when the device
  is USB-powered (AXP can't cut the rail). Don't remove the deep-sleep
  fallback.
- `M5.config().clear_display = true` in `setup()` clears the boot logo
  area immediately. Leaving it false leaves stray pixels visible until
  the first `pushSprite`.

## Common pitfalls

- **`fillArc` angles are degrees, not radians**, and 0° is at 3 o'clock,
  CW positive. The ring code stores angles in radians internally and
  converts at draw time. Don't change one without the other.
- **Drawing fonts**: `Font7` is 7-segment style (digits only). For
  alphabetic text use `Font4` (with `setTextSize(2)` if you want big
  letters) or `Font2`. Don't pick `Font7` for "OPEN" / "ALARM".
- **`M5.update()` must be called every loop iteration** — it ticks
  buttons, touch state, and IMU sample buffers.
- The `delay(8)` at the end of `loop()` keeps idle CPU usage sane.
  During the cracking animation, frame draw time dominates (~25–40 ms),
  so the delay is effectively no-op there. Don't try to "improve"
  framerate by removing it.

## Tested target environment

- Board: M5Stack Core 2 (no Faces/expander)
- Power: USB-C and battery both work; powerOff via AXP fully cuts power
  only on battery
- Firmware size at time of writing: ~517 KB flash, ~27 KB RAM static
