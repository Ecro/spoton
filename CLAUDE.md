# SpotOn — Claude Code Project Guide

## Project Overview

SpotOn is a tennis dampener-form-factor sensor that estimates **where** the ball hits the racket face, providing hit-map + sweet-spot rate + face angle + swing speed via BLE sync to a Flutter app.

- **Firmware:** Zephyr RTOS (nRF Connect SDK) on nRF54L15
- **Language:** C99 (firmware), Dart (Flutter app)
- **Sensors:** ICM-42688-P (IMU, 800Hz) + ADXL372 (high-g accelerometer, 800Hz) via SPI
- **ML:** SVR (impact locator) + RF (shot classifier) via emlearn → C headers
- **Storage:** ZMS (shot events + calibration/config)
- **Communication:** BLE 5.4 (post-session batch sync)
- **App:** Flutter + Riverpod + SQLite + flutter_blue_plus

## Architecture

```
Firmware (Zephyr RTOS, nRF54L15)
├── sensor_thread (P2)  — ICM-42688-P+ADXL372 FIFO read, impact detection
├── ml_thread (P5)      — SVR/RF inference, face angle, swing speed
├── session_thread (P6) — ZMS storage, BLE sync
└── main_thread (P7)    — State machine, WDT, events

State Machine: SLEEP → ARMED → ACTIVE → SYNC → SLEEP
IPC: imu_q (sensor→ml), shot_q (ml→session), event_q (all→main)

Flutter App
├── Riverpod (AsyncNotifier) — state management
├── SQLite (sqflite) — local database
├── flutter_blue_plus — BLE abstraction
└── Canvas — racket face hit-map visualization
```

## Tech Spec

Full technical specification: `TECH_SPEC.md` (v0.3.0)

## Source Tree (Planned)

```
spoton/                    # ← project root (/home/noel/spoton)
├── CMakeLists.txt
├── prj.conf
├── boards/nrf54l15dk_nrf54l15.overlay
├── src/
│   ├── main.c          — init, state machine, WDT, event loop
│   ├── sensor.c/.h     — ICM-42688-P + ADXL372 SPI, FIFO, interrupt
│   ├── impact.c/.h     — impact detection (ADXL372 threshold)
│   ├── ml.c/.h         — SVR + RF inference wrapper
│   ├── features.c/.h   — feature extraction (39 features)
│   ├── face_angle.c/.h — face angle (complementary filter)
│   ├── swing_speed.c/.h— swing speed (gyro peak)
│   ├── session.c/.h    — ZMS session management
│   ├── ble_svc.c/.h    — GATT service + batch transfer
│   ├── calibration.c/.h— racket calibration
│   ├── power.c/.h      — power management, state transitions
│   ├── timesync.c/.h   — BLE time sync
│   ├── config.h        — constants, thresholds
│   └── led.c/.h        — status LED
├── models/             — emlearn C headers (SVR, RF)
└── tools/              — Python training scripts
```

## Development Commands

```bash
# Build firmware
west build -b nrf54l15dk/nrf54l15

# Flash to DK
west flash

# Build + flash
west build -b nrf54l15dk/nrf54l15 && west flash

# Clean build
west build -b nrf54l15dk/nrf54l15 --pristine

# Monitor serial output
minicom -D /dev/ttyACM0 -b 115200

# Flutter app
cd spoton-app && flutter run

# Flutter build
cd spoton-app && flutter build apk
```

## Key Conventions

> Full coding patterns: `.claude/rules/c-zephyr-patterns.md` and `.claude/rules/flutter-patterns.md`

### Thread Boundaries (Firmware IPC)
- `imu_q` (K_MSGQ): sensor_thread → ml_thread (8 slots × 2200B)
- `shot_q` (K_MSGQ): ml_thread → session_thread (32 slots × 20B)
- `event_q` (K_MSGQ): any thread → main_thread (16 slots × 8B)
- Never access shared state without proper Zephyr sync primitives

### Data Structures
- `shot_event`: 20 bytes (ZMS + BLE wire format)
- `session_header`: 20 bytes
- `racket_calibration`: ZMS stored, max 8 profiles

## Implementation Phases

| Phase | Focus | Key Risk |
|-------|-------|----------|
| 0 | Hello Dual Sensor (SPI, FIFO, impact detect) | ADXL372 driver, TPU damping |
| 1 | Hello ML + Accuracy (data collection, SVR/RF) | Position ≤4cm, calibration |
| 2 | Hello App + Storage (ZMS, BLE, Flutter) | RRAM partition, BLE GATT |
| 3 | Polish + Open Source | Mechanical, power optimization |

## Common Modification Patterns

### Adding a New Sensor Reading
1. Update device tree overlay (pin assignment)
2. Add SPI read/write in `sensor.c`
3. Update FIFO burst read sequence
4. Add feature extraction in `features.c`
5. Update `shot_event` struct if new data stored

### Adding a Kconfig Option
1. Add `config SPOTON_*` in `Kconfig` (or `prj.conf` for simple)
2. Use `IS_ENABLED(CONFIG_SPOTON_*)` in C code
3. Document default in `config.h`

### Adding a BLE Characteristic
1. Define UUID in `ble_svc.h`
2. Add GATT attribute in `ble_svc.c`
3. Implement read/write/notify callbacks
4. Update Flutter BLE repository to handle new characteristic

### Adding a New State
1. Update state enum in `main.c`
2. Add transition logic in state machine
3. Update power management in `power.c`
4. Update LED patterns in `led.c`
