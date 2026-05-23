# LockCracker

**A pretend safe-cracking game for kids.** A playful toy for the
**M5Stack Core 2**. The child holds the device against a "safe" (a door,
a box, a piece of furniture), taps the screen, and a 10-second cracking
animation starts. If the device is moved during the animation, the
attempt fails and an alarm sounds. If it stays perfectly still, a green
**OPEN** lights up at the end and a success chime plays.

> This is a toy. There is no real magnetism, no real lock-picking, and
> nothing useful happens to the surface it's held against. It's a game
> of "stay still or the alarm goes off" — that's the whole thing.

## Hardware

- M5Stack Core 2 (ESP32, 320×240 touch display, MPU6886 IMU, speaker,
  AXP192/AXP2101 PMU, three capacitive buttons below the display)

## Features

- **Idle screen**: title, tap hint, battery indicator (top right),
  icons above the hardware buttons (gear = settings, power = off).
- **Cracking animation**: four concentric rings with gaps, counter-
  rotating in blue and purple shades. They "lock" into place one after
  another (every 2.5 s) and turn white. A click sound plays on each
  lock.
- **Motion detection**: sensitive (~0.10 g acceleration delta against a
  slowly-following baseline). Even a light tap on the case sets off the
  alarm — exactly what makes the game fun for kids.
- **Success**: full-screen green, "OPEN", ascending chime (C–E–G–C–E).
- **Failure**: full-screen red, "ALARM", alternating 880/440 Hz tones.
- **Settings screen** (Button A):
  - Volume as a touch slider (0–100 %, draggable).
  - Display brightness as a 4-step selector (25 / 50 / 75 / 100 %).
  - Both values are persisted to NVS on touch release.
- **Power off** (Button C): a short "Goodbye", then `M5.Power.powerOff()`.
  When running on USB (AXP can't fully cut the rail), the device falls
  back to deep sleep.

## Controls

| Action                                  | What happens                  |
|-----------------------------------------|-------------------------------|
| Tap on display (idle)                   | Cracking starts               |
| Button A (left)                         | Open settings / settings ⇒ idle |
| Button C (right)                        | Power off                     |
| Tap on volume slider                    | Set / drag volume             |
| Tap on a brightness box                 | Set brightness                |
| Tap after result                        | Back to idle screen           |

## Build & flash

Prerequisite: PlatformIO Core installed.

```
pio run -e m5stack-core2            # Build
pio run -e m5stack-core2 -t upload  # Build + flash
pio device monitor                  # Serial console (115200 baud)
```

## Project layout

```
platformio.ini      Board: m5stack-core2, framework: arduino, lib: M5Unified
src/main.cpp        Complete firmware (state machine, drawing, IMU, audio)
```

The entire firmware lives in a single file. State machine with four
states (`STATE_IDLE`, `STATE_CRACKING`, `STATE_RESULT`,
`STATE_SETTINGS`), drawing through a 320×240×16-bit `LGFX_Sprite`
(double-buffered to avoid flicker), persistence via the `Preferences`
library (namespace `mcc`, keys `vol` and `bri`).

## Tunable constants (top of `src/main.cpp`)

```cpp
CRACK_DURATION_MS = 10000   // animation duration
MOTION_THRESHOLD  = 0.10f   // g — smaller = more sensitive
RING_COUNT        = 4       // number of concentric rings
```

The color palette (`COLOR_RING[]`, `COLOR_BG`, …) sits at the top of
the file as well.
