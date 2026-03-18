# Phase 2 — Hello App + Storage

**Objective:** Full end-to-end pipeline: play tennis -> data stored on device -> BLE sync -> see hit-map in Flutter app.
**Timeline:** Week 7-10
**Hardware:** Same as Phase 1 (no additional purchases)
**Depends on:** Phase 1 complete (ML models trained, calibration working, <=5cm MAE)
**Created:** 2026-03-18

---

## Go/Kill Gate

- [ ] Full session: ARMED -> ACTIVE -> SYNC -> SLEEP works reliably
- [ ] ZMS handles 500+ shots across multiple sessions without corruption
- [ ] BLE batch sync of 1-hour session (500 shots) < 15 seconds
- [ ] Flutter app displays hit-map with correct positions from synced data
- [ ] Sweet-spot rate, face angle, swing speed stats display correctly
- [ ] Calibration UI completes 20-shot calibration flow
- [ ] Battery monitoring reads valid voltage (ADC)
- [ ] WDT recovers from artificial hang within 10 seconds
- [ ] No crashes across 10 live court sessions

---

## Phase 2 Architecture

```
Firmware (complete 4-thread model):

sensor_thread (P2) ─── imu_q ──► ml_thread (P5) ─── shot_q ──► session_thread (P6)
  ├─ FIFO reads                    ├─ features           ├─ ZMS write
  ├─ Ring buffer                   ├─ SVR/RF inference    ├─ Session mgmt
  └─ Impact detect                 ├─ Face angle          └─ BLE batch sync
                                   └─ Swing speed
                                                     ▲
                                          event_q    │
                                   all threads ──────► main_thread (P7)
                                                       ├─ State machine
                                                       ├─ WDT
                                                       ├─ Battery/temp monitor
                                                       └─ LED

State Machine:
  BOOT → SLEEP ←→ ARMED ←→ ACTIVE → SYNC → SLEEP
                                       ↑ BLE connect
  SHUTDOWN ← Battery <10% or Temp >60°C

Flutter App:
  BLE scan → Connect → Time sync → Session list → Download → SQLite
  Home | Hit-Map | Session Detail | Trends | Calibration | Settings
```

---

## Step 1: Session Storage (ZMS)

Implement session management with ZMS for shot events and session headers.

**Firmware files:**
- `src/session.h` — session API
- `src/session.c` — ZMS session storage, shot write, session lifecycle

**Design:**
```c
/* Session lifecycle:
 * 1. session_start() — create new session_header, assign session_id
 * 2. session_add_shot(shot_event) — write shot to ZMS
 * 3. session_end() — finalize header (end_timestamp, shot_count, sweet_spot_pct)
 * 4. session_sync(ble_callback) — iterate shots for BLE transfer
 * 5. session_delete(session_id) — remove after confirmed sync
 */

/* ZMS ID scheme (flat sequential — avoids bit-packing collisions):
 *
 * Uses TWO separate ZMS instances:
 *   zms_config — calibration + device config (16KB partition)
 *   zms_shots  — session headers + shot events (452KB partition)
 *
 * zms_shots ID allocation:
 *   next_id counter stored at ZMS ID 0x0001 in zms_config
 *   Each session_start() allocates: 1 header ID + N shot IDs (sequential)
 *   session_header stores its own ZMS ID + first_shot_id + shot_count
 *   Iteration: read header → iterate first_shot_id to first_shot_id + shot_count
 *
 * This avoids the bit-shift overflow that would occur with
 * (session_id << 12) + shot_index when session_id > 4096.
 */

int session_start(uint8_t racket_id);
int session_add_shot(const struct shot_event *evt);
int session_end(void);
int session_get_count(void);
int session_get_header(uint16_t session_id, struct session_header *hdr);
int session_iterate_shots(uint16_t session_id,
                          void (*cb)(const struct shot_event *evt, void *ctx),
                          void *ctx);
int session_delete(uint16_t session_id);
```

**ZMS partition (production layout from TECH_SPEC §4.4):**
```dts
zms_config_partition: partition@FE000 {
    label = "zms_config";
    reg = <0x0FE000 0x4000>;    /* 16KB — calibration + device config */
};
zms_shots_partition: partition@102000 {
    label = "zms_shots";
    reg = <0x102000 0x6F000>;   /* 452KB — session shot data */
};
```

**shot_event struct (must use `__packed` + BUILD_ASSERT):**
```c
/* TECH_SPEC fields sum to 17 bytes. Add reserved[3] for 20-byte alignment. */
struct __packed shot_event {
    uint32_t ts_offset_ms;       /* 4B, offset 0 */
    uint8_t  shot_type;          /* 1B, offset 4 */
    int16_t  impact_x_mm;       /* 2B, offset 5 */
    int16_t  impact_y_mm;       /* 2B, offset 7 */
    uint8_t  is_sweet_spot;     /* 1B, offset 9 */
    uint16_t peak_accel_mg;     /* 2B, offset 10 */
    uint16_t swing_speed;       /* 2B, offset 12 */
    int16_t  face_angle_deg10;  /* 2B, offset 14 */
    uint8_t  racket_id;         /* 1B, offset 16 */
    uint8_t  reserved[3];       /* 3B, offset 17 — pad to 20 bytes */
};
BUILD_ASSERT(sizeof(struct shot_event) == 20, "shot_event must be exactly 20 bytes");
```

**Capacity:** 452KB / 20B per shot = ~23,100 shots = ~46 hours at 500 shots/h

**session_thread design:**
```
session_thread (Priority 6, Stack 2KB):
  while(1) {
    /* Use timeout (not K_FOREVER) to allow WDT feed and state checks */
    int ret = k_msgq_get(&shot_q, &evt, K_SECONDS(5));

    task_wdt_feed(session_wdt_ch);  /* Feed WDT on every wake (timeout or data) */

    if (ret == 0) {
      /* Got a shot event */
      if (state == ACTIVE) {
        session_add_shot(&evt);
      }
    }

    /* Check for state transitions (e.g., SYNC requested via semaphore) */
    if (k_sem_take(&sync_request_sem, K_NO_WAIT) == 0) {
      session_do_ble_sync();  /* Iterate ZMS, send indications */
    }
  }
```

> **Note:** All threads use `k_msgq_get` with timeout < WDT period (10s) to ensure
> WDT is fed even during idle periods. This prevents spurious resets when no impacts
> occur for extended periods (e.g., between games).

**Verification:**
- Write 500 shots across 3 sessions, read back all, verify integrity
- Power cycle between sessions, verify persistence
- Fill partition to 90%, verify graceful handling

---

## Step 2: State Machine

Implement the full SLEEP -> ARMED -> ACTIVE -> SYNC -> SLEEP state machine.

**Files:**
- `src/main.c` — state machine, event loop, main_thread
- `src/power.h/.c` — power state transitions, sensor suspend/resume
- `src/config.h` — state machine timing constants

**State machine (from TECH_SPEC §4.3):**
```
States:
  BOOT    → Init sensors, load calibration, start threads → SLEEP
  SLEEP   → ICM WOM enabled, ADXL372 instant-on (1.4µA), sensors suspended
            Wake: ICM WOM interrupt → ARMED
            BLE connect + pending sessions → SYNC
  ARMED   → ICM low-power mode, ADXL372 instant-on
            Transition: First impact (ADXL >30g) → ACTIVE
            Timeout: 2 min no impact → SLEEP (no session created)
            BLE connect + pending sessions → SYNC (sync old sessions)
  ACTIVE  → 800Hz full sampling, ML inference, ZMS storage
            Transition: 5 min no impact → session_end() → SYNC (if BLE) or SLEEP
            BLE connect during ACTIVE: set deferred_sync flag, continue ACTIVE
            On session_end() with deferred_sync: → SYNC (not SLEEP)
  SYNC    → Sensors suspended, BLE TX active
            Transfer: session data → app → ACK → delete synced sessions
            On ACK: set session.synced=1 then delete (synced flag guards re-transfer)
            Complete/disconnect → SLEEP
  CHARGING → VBUS detected (any state except ACTIVE)
            nPM1100 handles charging autonomously
            Firmware: suspend sensors, LED orange/green
            VBUS removed → SLEEP
            If VBUS during ACTIVE: finish session first, then CHARGING
  SHUTDOWN → Battery <10% or temp >60°C → LED 3x red → system off

BLE disconnect during sync:
  Full retransmit on reconnect (acceptable: 500 shots ≈ 4-15 seconds).
  Partially received data discarded by app on disconnect.

event_q (all threads → main_thread):
  K_MSGQ_DEFINE(event_q, sizeof(struct system_event), 16, 4);

  struct system_event {
      uint8_t type;   /* EVT_IMPACT, EVT_WOM, EVT_BLE_CONNECT, EVT_BLE_DISCONNECT,
                         EVT_TIMEOUT, EVT_BATTERY_LOW, EVT_TEMP_HIGH */
      uint8_t data;   /* Optional payload */
  };
```

**config.h additions:**
```c
#define ARMED_TIMEOUT_MS        (2 * 60 * 1000)   /* 2 min */
#define ACTIVE_TIMEOUT_MS       (5 * 60 * 1000)   /* 5 min no impact */
```

**Power transitions:**
```c
/* power.c */
int power_enter_sleep(void);     /* Suspend sensors, enable WOM */
int power_enter_armed(void);     /* ICM low-power, ADXL instant-on */
int power_enter_active(void);    /* 800Hz full sampling */
int power_enter_sync(void);      /* Suspend sensors, BLE TX */
int power_shutdown(void);        /* System off */
```

**Verification:**
- Walk through full state cycle: BOOT → SLEEP → (tap) → ARMED → (hit) → ACTIVE → (wait 5 min) → SYNC → SLEEP
- Verify sensor power modes change at each transition
- Verify WDT does not fire during normal state transitions

---

## Step 3: Watchdog + Battery + Temperature

Implement safety monitoring.

**Files:**
- `src/main.c` — WDT setup, periodic battery/temp check
- `src/power.c` — battery ADC, NTC temperature, shutdown logic
- `prj.conf` — add WDT, ADC config

**WDT design:**
```c
/* Task WDT: each thread feeds its own channel */
CONFIG_WATCHDOG=y
CONFIG_WDT_NRFX=y
CONFIG_TASK_WDT=y

/* 10-second timeout per thread, reset on feed */
/* main_thread feeds in event loop */
/* sensor_thread feeds after each FIFO read */
/* ml_thread feeds after each inference */
/* session_thread feeds after each shot write */
```

**Battery monitoring:**
```c
/* ADC reads via Zephyr ADC API */
/* VBAT through resistor divider → ADC channel */
/* Read every 60 seconds in main_thread */
/* Battery level: linear map 3.0V (0%) to 4.2V (100%) */
/* BAS (Battery Service) notify on change */

CONFIG_ADC=y
CONFIG_ADC_NRFX_SAADC=y
```

**Temperature monitoring:**
```c
/* NTC thermistor through voltage divider → ADC channel */
/* Read every 60 seconds alongside battery */
/* >55°C warning (LED pattern), >60°C shutdown */
/* Connected to nPM1100 NTC input for hardware charge protection */
```

**Verification:**
- Artificially block a thread → WDT triggers reset within 10s
- Read battery voltage, compare to multimeter
- Heat NTC with fingers → verify temperature reading changes

---

## Step 4: BLE GATT Service

Implement production BLE service for session sync.

**Files:**
- `src/ble_svc.h` — GATT service definitions, UUIDs
- `src/ble_svc.c` — GATT attributes, callbacks, batch transfer
- `src/timesync.h/.c` — BLE time synchronization
- `prj.conf` — BLE config additions

**GATT service (from TECH_SPEC §8):**
```
Standard Services:
  DIS (0x180A): Manufacturer, Model, FW Rev, HW Rev
  BAS (0x180F): Battery Level (read + notify)

SpotOn Service (UUID: a1b2c3d4-e5f6-7890-ab12-cd34ef560001):
  Session Count (...0002)  — uint16_t (read)
  Session Data  (...0003)  — byte array (indicate)
  Control Point (...0004)  — uint8_t (write)
  Device Status (...0005)  — struct (read)
```

**Batch transfer protocol:**
```
On app connect:
  1. App writes SET_TIME (0x04 + unix_ts) → Control Point
  2. App reads Session Count
  3. App enables indications on Session Data

Per session transfer:
  1. Firmware sends Header packet [0x01][16B wire header]
     Wire header = {session_id:u32, start:u32, end:u32, shots:u16, sweet_pct:u8, racket_id:u8}
     (16 bytes — excludes internal synced/reserved fields from storage struct)
  2. Firmware sends Shot packets [0x02][shot_index:u16][shot_event 20B] × N
     shot_index enables debugging and partial-loss detection
  3. Firmware sends End packet [0xFF][total_sent:u16][crc32:u32]
     CRC32 (IEEE 802.3, polynomial 0x04C11DB7) covers:
       all bytes after type prefix in header + all shot packets
       i.e., CRC([16B header] + [20B shot × N]), excluding type bytes and End packet
  4. App verifies CRC32, writes ACK (0x01) or NACK (0x02) to Control Point
  5. On ACK: firmware sets session.synced=1 in ZMS, then deletes session
     On NACK: retransmit entire session (max 3 attempts, then skip + log error)

Indicate flow:
  - Server sends indication → waits for ATT confirmation → sends next
  - Guaranteed delivery (unlike notifications)
  - Throughput depends on connection interval (CI):
    - Best case (7.5ms CI, 2M PHY): 500 shots ≈ ~4 seconds
    - iOS typical (15ms CI): 500 shots ≈ ~8 seconds
    - Android conservative (30ms CI): 500 shots ≈ ~15 seconds
  - Go/Kill gate: <15 seconds for 500 shots (accounts for real-world CI)
```

**Time sync (from TECH_SPEC §8.2):**
```c
/* timesync.c */
static int64_t rtc_offset;  /* unix_ts - uptime_sec */

void timesync_set(uint32_t unix_ts) {
    rtc_offset = (int64_t)unix_ts - (k_uptime_get() / 1000);
}

uint32_t timesync_get_unix(void) {
    return (uint32_t)((k_uptime_get() / 1000) + rtc_offset);
}
```

**prj.conf additions:**
```
CONFIG_BT_DIS=y
CONFIG_BT_DIS_MANUF="SpotOn"
CONFIG_BT_DIS_MODEL="SpotOn-v1"
CONFIG_BT_DIS_FW_REV_STR="0.3.0"
CONFIG_BT_BAS=y
CONFIG_BT_PERIPHERAL_PREF_MIN_INT=6    /* 7.5ms CI for fast sync */
CONFIG_BT_PERIPHERAL_PREF_MAX_INT=12   /* 15ms CI fallback */
```

**Verification:**
- Connect from nRF Connect app, verify DIS/BAS reads correctly
- Write SET_TIME, verify timesync offset
- Trigger session with 10 impacts, sync via BLE, verify all 10 shots received
- Verify CRC32 check — inject corruption, verify NACK + retransmit

---

## Step 5: Flutter App — BLE + SQLite

Build the Flutter app core: BLE connection, data sync, SQLite storage.

**Flutter files:**
```
spoton-app/
├── lib/
│   ├── main.dart
│   ├── models/
│   │   ├── session.dart         — Session + Shot data classes
│   │   └── racket.dart          — Racket calibration model
│   ├── repositories/
│   │   ├── ble_repository.dart  — flutter_blue_plus abstraction
│   │   ├── session_repository.dart — SQLite CRUD
│   │   └── export_repository.dart  — CSV/PNG (Step 7)
│   ├── providers/
│   │   ├── ble_provider.dart    — BleNotifier (AsyncNotifier)
│   │   ├── session_provider.dart — SessionNotifier
│   │   └── calibration_provider.dart
│   ├── screens/
│   │   ├── home_screen.dart
│   │   ├── hitmap_screen.dart   — Step 6
│   │   ├── session_detail_screen.dart
│   │   ├── calibration_screen.dart — Step 7
│   │   └── settings_screen.dart
│   └── widgets/
│       ├── racket_canvas.dart   — Step 6
│       └── session_card.dart
├── pubspec.yaml
└── test/
```

**SQLite schema:**
```sql
CREATE TABLE sessions (
  id INTEGER PRIMARY KEY,
  session_id INTEGER UNIQUE,
  start_timestamp INTEGER,
  end_timestamp INTEGER,
  shot_count INTEGER,
  sweet_spot_pct INTEGER,
  racket_id INTEGER,
  synced INTEGER DEFAULT 0
);

CREATE TABLE shots (
  id INTEGER PRIMARY KEY,
  session_id INTEGER REFERENCES sessions(session_id),
  ts_offset_ms INTEGER,
  shot_type INTEGER,
  impact_x_mm INTEGER,
  impact_y_mm INTEGER,
  is_sweet_spot INTEGER,
  peak_accel_mg INTEGER,
  swing_speed INTEGER,
  face_angle_deg10 INTEGER,
  racket_id INTEGER
);

CREATE INDEX idx_shots_session ON shots(session_id);
```

**BLE repository design:**
```dart
class BleRepository {
  // Scan + connect
  Future<void> connect(String deviceId);
  Future<void> disconnect();
  Stream<BleConnectionState> get connectionState;

  // Time sync (immediate on connect)
  Future<void> syncTime();

  // Session sync
  Future<int> getSessionCount();
  Stream<SyncProgress> syncSessions();  // yields progress updates

  // Calibration
  Future<void> startCalibration(int racketId);
  Future<void> setRacket(int racketId);

  // Binary parsing
  SessionHeader _parseHeader(List<int> bytes);
  ShotEvent _parseShot(List<int> bytes);
}
```

**BLE packet parsing (shot_event, 20 bytes):**
```dart
ShotEvent _parseShot(Uint8List bytes) {
  final data = ByteData.sublistView(bytes);
  return ShotEvent(
    tsOffsetMs: data.getUint32(0, Endian.little),
    shotType: data.getUint8(4),
    impactXMm: data.getInt16(5, Endian.little),
    impactYMm: data.getInt16(7, Endian.little),
    isSweetSpot: data.getUint8(9) == 1,
    peakAccelMg: data.getUint16(10, Endian.little),
    swingSpeed: data.getUint16(12, Endian.little),
    faceAngleDeg10: data.getInt16(14, Endian.little),
    racketId: data.getUint8(16),
  );
}
```

**pubspec.yaml dependencies:**
```yaml
dependencies:
  flutter_blue_plus: ^1.32.0
  sqflite: ^2.3.0
  flutter_riverpod: ^2.5.0
  path_provider: ^2.1.0
```

**Verification:**
- Connect to firmware, sync 1 session, verify data appears in SQLite
- Verify session count matches, shot data matches serial log
- Disconnect + reconnect, verify no duplicate sync

---

## Step 6: Flutter Hit-Map Canvas

Implement the racket face hit-map visualization.

**Files:**
- `lib/widgets/racket_canvas.dart` — CustomPainter for racket face
- `lib/screens/hitmap_screen.dart` — Hit-map screen with filters
- `lib/providers/hitmap_provider.dart` — HitmapNotifier

**Hit-map design:**
```
Racket face canvas:
  - Oval outline (~28×21cm mapped to screen coordinates)
  - Sweet spot circle (configurable radius, default 4cm)
  - Impact dots colored by:
    - Sweet spot: green, Outside: red/orange (default)
    - Shot type: FH=blue, BH=red, SV=green, VL=orange, SL=purple (filter)
  - Face angle indicator arrow at each dot (optional toggle)
  - Touch a dot → show shot details (type, speed, angle)

Filters:
  - By session (dropdown)
  - By shot type (toggle chips)
  - By date range
  - Sweet spot radius slider (3-6cm)

Stats overlay:
  - Sweet-spot rate: XX%
  - Average swing speed: XX km/h
  - Average face angle: XX°
  - Shot count: XX
```

**CustomPainter:**
```dart
class RacketFacePainter extends CustomPainter {
  final List<ShotEvent> shots;
  final double sweetSpotRadiusCm;
  final ShotFilter filter;

  @override
  void paint(Canvas canvas, Size size) {
    // 1. Draw racket outline (oval)
    // 2. Draw sweet spot circle
    // 3. Draw string grid lines (decorative)
    // 4. Plot each shot as colored dot
    //    x_screen = (shot.impactXMm / 10.0 + racketWidth/2) * scale
    //    y_screen = (shot.impactYMm / 10.0 + racketHeight/2) * scale
    // 5. Draw face angle arrows (if enabled)
  }
}
```

**Verification:**
- Display 100 shots from a synced session
- Verify positions match expected zones
- Sweet spot circle toggle works
- Filter by shot type shows correct subset

---

## Step 7: Calibration UI + Data Export

Implement calibration flow and session export in the app.

**Calibration UI files:**
- `lib/screens/calibration_screen.dart`
- `lib/providers/calibration_provider.dart`

**Calibration flow:**
```
1. Select racket (existing or create new)
2. "Hit 20 times at center" instruction
3. Progress: X/20 hits (live counter from BLE)
4. Outlier feedback: green checkmark / red X per hit
5. Complete: show offset values, "Calibration saved"
6. Option to recalibrate
```

**Export files:**
- `lib/repositories/export_repository.dart`

**CSV export:**
```dart
Future<File> exportSessionCsv(int sessionId) async {
  final shots = await sessionRepo.getShots(sessionId);
  final csv = StringBuffer();
  csv.writeln('timestamp,shot_type,x_cm,y_cm,sweet_spot,peak_g,speed_kmh,angle_deg,racket');
  for (final shot in shots) {
    csv.writeln('${shot.tsOffsetMs},${shot.shotTypeName},...');
  }
  // Save to Downloads or share via Share sheet
}
```

**PNG export:**
```dart
// Capture RacketFacePainter to image
Future<File> exportHitmapPng(int sessionId) async {
  final recorder = ui.PictureRecorder();
  final canvas = Canvas(recorder);
  painter.paint(canvas, size);
  final image = await recorder.endRecording().toImage(width, height);
  final bytes = await image.toByteData(format: ui.ImageByteFormat.png);
  // Save to file
}
```

**Verification:**
- Complete calibration flow in app, verify calibration stored on device
- Export CSV, open in Excel, verify data matches app display
- Export PNG, verify hit-map image is correct

---

## Step 8: LED Status Patterns

Define and implement LED patterns for all states.

**Files:**
- `src/led.c` — LED pattern implementation using Zephyr LED API or GPIO + timer
- `src/led.h` — pattern definitions

**LED patterns:**
```
BOOT:       Rapid blink (100ms on/off)
SLEEP:      Off
ARMED:      Slow pulse (2s period)
ACTIVE:     Solid on
SYNC:       Fast blink (200ms on/off)
IMPACT:     Single flash (50ms on, during ACTIVE)
CALIBRATION: Double blink pattern
BATTERY_LOW: 3x red blink → off
CHARGING:   Orange (steady)
CHARGED:    Green (steady)
ERROR:      SOS pattern (3 short, 3 long, 3 short)
```

**Verification:**
- Walk through each state, verify LED matches expected pattern

---

## Step 9: Integration + Court Testing

10 live court sessions to validate the full system.

**Integration test (before going to court):**
```
Bench test:
  1. Boot → SLEEP (LED off)
  2. Tap desk → ARMED (LED slow pulse)
  3. Hit harder → ACTIVE (LED solid)
  4. 10 impacts → 10 shot_events in ZMS
  5. Connect phone → SYNC (LED fast blink)
  6. App: time sync → session list → download
  7. App: hit-map shows 10 dots
  8. ACK → session deleted from ZMS
  9. Disconnect → SLEEP

Edge cases:
  - Multiple sessions before sync (accumulate, sync all)
  - BLE disconnect during transfer (resume on reconnect)
  - Battery read during active play
  - WDT feed during long inference (should not trigger)
```

**Court test protocol (10 sessions):**
```
Per session:
  1. Wake device, verify ARMED state
  2. Play 20-30 minutes (mixed strokes)
  3. Open app, sync session
  4. Verify: hit-map looks reasonable, shot count matches play
  5. Check: sweet-spot rate, face angle distribution, swing speed range
  6. Export CSV for offline analysis

Track across sessions:
  - Crash count (target: 0)
  - Missed impacts (compare felt hits vs recorded)
  - False impacts (recorded without actual hit)
  - Sync failures
  - Battery drain rate
```

**Success criteria:**
- [ ] 10/10 sessions complete without crash
- [ ] Hit-map positions visually match play experience
- [ ] Sweet-spot rate seems reasonable (30-70% for recreational)
- [ ] No FIFO overflows during play
- [ ] BLE sync completes for all sessions
- [ ] Battery sufficient for 1+ hour session

---

## File Summary

### Firmware
| File | Step | Purpose |
|------|------|---------|
| `src/session.h/.c` | 1 | ZMS session storage |
| `src/main.c` | 2 | State machine + event loop |
| `src/power.h/.c` | 2,3 | Power transitions + battery/temp |
| `src/ble_svc.h/.c` | 4 | Production GATT service |
| `src/timesync.h/.c` | 4 | BLE time sync |
| `src/led.h/.c` | 8 | Status LED patterns |
| `prj.conf` | 1-4 | Kconfig additions |
| `boards/*.overlay` | 1 | ZMS production partitions |

### Flutter App
| File | Step | Purpose |
|------|------|---------|
| `lib/models/session.dart` | 5 | Data classes |
| `lib/repositories/ble_repository.dart` | 5 | BLE abstraction |
| `lib/repositories/session_repository.dart` | 5 | SQLite CRUD |
| `lib/repositories/export_repository.dart` | 7 | CSV/PNG export |
| `lib/providers/*.dart` | 5,6,7 | Riverpod state management |
| `lib/screens/*.dart` | 5,6,7 | All app screens |
| `lib/widgets/racket_canvas.dart` | 6 | Hit-map CustomPainter |

## Resource Budget (Phase 2 Cumulative)

| Resource | Phase 0+1 | Phase 2 Added | Cumulative | Budget |
|----------|-----------|---------------|------------|--------|
| Flash | ~49 KB | ~30 KB (BLE+session+state+WDT) | ~79 KB | 500 KB |
| RAM | ~52 KB | ~30 KB (BLE stack ~25KB + session 2KB + event_q + ADC) | ~82 KB | 256 KB |
| Threads | 2 | +2 (session, main) | 4 | 4 planned |
| ZMS config | ~16 KB | (same partition) | 16 KB | 16 KB |
| ZMS shots | 0 | 452 KB | 452 KB | 452 KB |

## Risks (Phase 2 Specific)

| Risk | ID | Mitigation | Checkpoint |
|------|----|------------|------------|
| ZMS partition full (no sync) | R10 | LED warning, app notification, auto-delete oldest | Step 1 |
| BLE disconnect during sync | — | Resume protocol, per-session CRC | Step 4 |
| State machine deadlock | — | WDT recovery + event_q timeout | Steps 2,3 |
| Flutter BLE reconnection | — | Auto-reconnect in BleRepository | Step 5 |
| MCUboot partition conflict | — | Validate partition layout in DT | Step 1 |

## Open Questions to Resolve

| Q | Question | Resolved By |
|---|----------|-------------|
| Q6 | MCUboot BLE DFU timing (Phase 2 vs 3)? | Step 1: partition layout only, DFU in Phase 3 |

## Phase 1 Completion Checklist (must verify before starting Phase 2)

- [ ] Impact location <=5cm MAE post-calibration
- [ ] Shot classification >=85% (5-class)
- [ ] Face angle +-5° vs camera
- [ ] Swing speed +-15% vs radar
- [ ] ML inference <10ms on nRF54L15
- [ ] Calibration protocol reduces MAE >=30%
- [ ] Models exported as C headers in models/

---

## Dependencies for Phase 3

Phase 2 outputs that Phase 3 builds on:
- Complete firmware (all 4 threads, state machine, BLE, ZMS)
- Working Flutter app (BLE sync, hit-map, calibration UI)
- 10 court sessions of real-world validation data
- Battery drain measurements from court sessions
- List of bugs/issues found during court testing
- Baseline UX feedback for polish phase

---

## Plan Validation (3-Agent Consensus)

**Method:** 2/3 consensus from 3 independent validators
**Overall:** APPROVED (all findings addressed)

### Consensus Findings Applied

| # | Finding | Severity | Resolution |
|---|---------|----------|------------|
| 1 | ZMS ID scheme overflow/collision | Critical | Redesigned: flat sequential IDs with separate ZMS instances |
| 2 | shot_event = 17B not 20B + missing `__packed` | Critical | Added reserved[3], `__packed`, BUILD_ASSERT(== 20) |
| 3 | BLE header 20B struct vs 16B wire format | Warning | Defined 16B wire format (excludes synced/reserved) |
| 4 | session_thread blocks on shot_q during SYNC | Warning | Use K_SECONDS(5) timeout + sync_request semaphore |
| 5 | ACTIVE→SYNC ambiguity (BLE during play) | Warning | Deferred sync flag: continue ACTIVE, sync after session_end |
| 6 | CRC32 scope undefined | Warning | Defined: IEEE 802.3 over header+shots payload (no type bytes) |
| 7 | RAM budget underestimates BLE stack | Warning | Updated to ~82KB cumulative (BLE ~25KB realistic) |
| 8 | No BLE disconnect resume protocol | Warning | Document: full retransmit on reconnect (MVP acceptable) |
| 9 | No duplicate sync prevention | Warning | Set synced=1 before delete; skip synced sessions |
| 10 | WDT starvation during blocked threads | Warning | All threads use K_SECONDS(5) timeout + WDT feed |
| 11 | CHARGING state missing | Suggestion | Added to state machine with VBUS transitions |
| 12 | SQLite missing index on session_id | Suggestion | Added CREATE INDEX |
| 13 | BLE CI negotiation / throughput | Suggestion | Added CI prefs in prj.conf, realistic timing (4-15s) |
| 14 | Riverpod pubspec redundancy | Suggestion | Removed standalone riverpod dep |

### Dropped (1/3 only)
Dynamic GATT DB, timesync precision, control point type, trends screen, ZMS wear
