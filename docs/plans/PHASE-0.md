# Phase 0 — Hello Dual Sensor

**Objective:** Prove dual-sensor SPI at 800Hz, FIFO reads, impact detection, and ZMS on nRF54L15 DK.
**Timeline:** Week 1-2
**Hardware:** nRF54L15-DK + EV_ICM-42688-P + EVAL-ADXL372Z (~$118)
**Created:** 2026-03-18

---

## Go/Kill Gate

- [ ] ICM-42688-P WHO_AM_I reads correctly over SPI
- [ ] ADXL372 DEVID reads correctly over SPI
- [ ] Both sensors streaming at 800Hz with FIFO watermark interrupts (no overflows)
- [ ] Impact detection triggers on desk taps (ADXL372 threshold >30g)
- [ ] ZMS write/read cycle verified on RRAM (100 write/read round-trips)
- [ ] Serial log shows timestamped dual-sensor data + impact events

- [ ] TPU damping test: vibration attenuation <=50% (bare vs TPU-enclosed)

**Kill criteria:** If ICM-42688-P Zephyr driver is fundamentally broken on nRF54L15 (R2), fall back to BMI270. If ZMS doesn't work on RRAM (R14), escalate to Zephyr upstream.

---

## Architecture Decision: Zephyr Sensor API vs Raw SPI

> **Decision required before Step 1.** The plan must choose ONE approach:
>
> **Option A: Raw SPI register access** (recommended for Phase 0)
> - Use `CONFIG_SPI=y` only. Do NOT enable `CONFIG_ICM42688=y` or `CONFIG_ADXL372=y`.
> - Bind devices as generic SPI nodes in DT (use `compatible = "vnd,spi-device"` or similar).
> - Full control over register config, FIFO reads, and interrupt handling.
> - More code to write, but better debugging visibility for bring-up.
>
> **Option B: Zephyr upstream sensor drivers**
> - Enable `CONFIG_ICM42688=y` / `CONFIG_ADXL372=y`.
> - Use `sensor_sample_fetch()` / `sensor_channel_get()` or RTIO streaming.
> - Less code, but must verify driver supports FIFO watermark interrupts on nRF54L15.
> - Cannot mix raw register writes with the driver (driver owns the device).
>
> **Do NOT mix both approaches.** If using Option A, the register tables in Steps 1-4 apply directly. If using Option B, replace register tables with DT properties and sensor API calls.
>
> The register tables below assume **Option A** for reference. Verify all values against DS-000347 and the ADXL372 datasheet before coding.

---

## Step 0: Project Scaffold

Set up the Zephyr project structure and verify the toolchain builds for nRF54L15.

**Files to create:**
- `CMakeLists.txt` — Zephyr project boilerplate
- `prj.conf` — Kconfig (minimal: SPI, logging, GPIO for LED)
- `boards/nrf54l15dk_nrf54l15_cpuapp.overlay` — empty placeholder
- `src/main.c` — blinky + LOG_INF("SpotOn Phase 0")
- `src/config.h` — project-wide constants

**Verification:**
```bash
west build -b nrf54l15dk/nrf54l15/cpuapp
west flash
# minicom: see "SpotOn Phase 0" log + LED blink
```

**Kconfig (prj.conf) — initial:**
```
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_GPIO=y
CONFIG_SPI=y
# CONFIG_SENSOR=y        # Only if using Zephyr sensor driver API (Option B)
# CONFIG_ICM42688=y      # Only if using Zephyr sensor driver API (Option B)
# CONFIG_ADXL372=y       # Only if using Zephyr sensor driver API (Option B)
```

---

## Step 1: ICM-42688-P SPI Communication

Bring up ICM-42688-P over SPI. Validate WHO_AM_I register read.

**Files:**
- `boards/nrf54l15dk_nrf54l15_cpuapp.overlay` — add ICM-42688-P SPI node + CS + INT1 pin
- `src/sensor.h` — sensor API declarations
- `src/sensor.c` — ICM-42688-P init, WHO_AM_I read, basic register config
- `prj.conf` — add sensor driver config (see Architecture Decision above)

**DT overlay structure:**
```dts
/* NOTE: Pin numbers (X, Y, Z, W) are placeholders.
 * Before implementation, consult the nRF54L15-DK schematic to select actual pins.
 * CRITICAL: SPI SCK MUST be on a 64MHz-domain GPIO (P0.0-P0.10) for 10MHz SPI clock.
 * 16MHz-domain pins (P1.x) will not achieve 10MHz. */

&spi0 {
    status = "okay";
    /* SPI Mode 0 (CPOL=0, CPHA=0) — compatible with both ICM-42688-P and ADXL372 */
    cs-gpios = <&gpio0 X GPIO_ACTIVE_LOW>,   /* ICM-42688-P CS */
               <&gpio0 Y GPIO_ACTIVE_LOW>;   /* ADXL372 CS (Step 2) */

    icm42688p: icm42688p@0 {
        compatible = "invensense,icm42688";  /* or "vnd,spi-device" for raw SPI (Option A) */
        reg = <0>;
        spi-max-frequency = <10000000>;  /* 10MHz (shared bus, ADXL372 limit) */
        int-gpios = <&gpio0 Z GPIO_ACTIVE_LOW>;  /* INT1 - FIFO WM */
        /* INT2 (WOM) — reserved for Phase 2 SLEEP→ARMED transition */
        /* int2-gpios = <&gpio0 V GPIO_ACTIVE_LOW>; */
        accel-hz = <800>;
        gyro-hz = <800>;
    };
};
```

**Key registers to configure (ICM-42688-P):**
| Register | Value | Purpose |
|----------|-------|---------|
| WHO_AM_I (0x75) | 0x47 | Identity check |
| PWR_MGMT0 (0x4E) | 0x0F | Accel+Gyro LNM on |
| ACCEL_CONFIG0 (0x50) | 0x06 | ±16g, 800Hz |
| GYRO_CONFIG0 (0x4F) | 0x06 | ±2000dps, 800Hz |
| ACCEL_CONFIG1 (0x53) | See AAF below | ~350Hz anti-aliasing |
| FIFO_CONFIG (0x16) | Stream mode | Continuous FIFO |
| FIFO_CONFIG1 (0x5F) | 0x07 | Accel + Gyro + timestamp to FIFO |

**Verification:**
```
LOG_INF("ICM-42688-P WHO_AM_I: 0x%02X (expect 0x47)", id);
```

**AAF Configuration (requires Bank 2 registers):**
The ICM-42688-P anti-aliasing filter is configured via Bank 2 registers, not just ACCEL_CONFIG1:
1. Write `REG_BANK_SEL (0x76) = 0x02` (switch to Bank 2)
2. Write `ACCEL_CONFIG_STATIC2` (ACCEL_AAF_DELT) — compute from DS-000347 Table 17
3. Write `ACCEL_CONFIG_STATIC3` (ACCEL_AAF_DELTSQR low byte)
4. Write `ACCEL_CONFIG_STATIC4` (ACCEL_AAF_DELTSQR high + ACCEL_AAF_BITSHIFT)
5. Write `REG_BANK_SEL (0x76) = 0x00` (switch back to Bank 0)

Exact coefficient values for ~350Hz cutoff must be computed from the datasheet formula. Document the values in `config.h`.

**Risk gate:** If WHO_AM_I fails, check SPI wiring and DT pin assignment before proceeding.

---

## Step 2: ADXL372 SPI Communication

Add ADXL372 on the same SPI bus with separate CS.

**Files:**
- `boards/nrf54l15dk_nrf54l15_cpuapp.overlay` — add ADXL372 node (CS1)
- `src/sensor.c` — add ADXL372 init, DEVID read
- `prj.conf` — add ADXL372 config (see Architecture Decision)

**DT overlay addition:**
```dts
    adxl372: adxl372@1 {
        compatible = "adi,adxl372";
        reg = <1>;
        spi-max-frequency = <10000000>;  /* 10MHz max */
        int1-gpios = <&gpio0 W GPIO_ACTIVE_HIGH>;
    };
```

**Key registers (ADXL372):**
| Register | Value | Purpose |
|----------|-------|---------|
| DEVID_AD (0x00) | 0xAD | Analog Devices ID |
| DEVID_MST (0x01) | 0x1D | MEMS ID |
| PARTID (0x02) | 0xFA | ADXL372 part ID |
| TIMING (0x3D) | 800Hz ODR | Match ICM-42688-P |
| MEASURE (0x3F) | Bandwidth, normal mode | NOT peak detect mode |
| FIFO_CTL (0x3A) | Stream mode | Continuous FIFO |

**Verification:**
```
LOG_INF("ADXL372 DEVID: 0x%02X (expect 0xAD)", id);
LOG_INF("ADXL372 PARTID: 0x%02X (expect 0xFA)", id);
```

---

## Step 3: ICM-42688-P FIFO + Watermark Interrupt

Enable FIFO streaming at 800Hz with watermark interrupt.

**Files:**
- `src/sensor.c` — FIFO config, watermark INT handler, burst read
- `src/sensor.h` — sensor data callback type

**Design:**
```
ICM-42688-P FIFO (2KB, 16-bit mode)
  - Each FIFO packet includes a 1-byte header + sensor data
  - With accel+gyro enabled: 1B header + 6B accel + 6B gyro = 13B per packet
  - With accel+gyro+timestamp: 16B per packet (verify exact format in DS-000347 §5.2)
  - 2KB / 16B = ~128 packets max (not 170)
  - Watermark at 64 packets (~80ms @ 800Hz) — conservative to avoid overflow
  - INT1 fires → sensor_thread wakes → burst read FIFO

ISR (minimal):
  gpio_callback → k_sem_give(&fifo_sem)

sensor_thread:
  while(1) {
    k_sem_take(&fifo_sem, K_FOREVER);
    // Check overflow status (INT_STATUS bit 2) FIRST
    if (overflow) { LOG_WRN("ICM FIFO overflow"); flush_fifo(); overflow_count++; continue; }
    n = sensor_read_fifo_count();
    sensor_burst_read(buf, n);
    // log sample count + timing
  }
```

**FIFO read buffers:** Must be `static` (file-scope), NOT stack-allocated.
64 packets × 16B = 1,024B per burst — too large for a 2KB thread stack.

**SPI burst read performance:**
- 64 packets × 16B = 1,024B
- @ 10MHz SPI: ~0.8ms (well within 80ms budget)

**Verification:**
- LOG_INF every 800 samples (~1 second): "ICM FIFO: %d samples, dt=%d ms"
- Expect: 800 samples/sec +-1%, no FIFO overflow warnings

---

## Step 4: ADXL372 FIFO + Watermark Interrupt

Enable ADXL372 FIFO streaming at 800Hz.

**Files:**
- `src/sensor.c` — ADXL372 FIFO config, watermark INT, burst read

**Design:**
```
ADXL372 FIFO (512 entries, normal mode)
  - 512 FIFO entries, where each entry = 1 axis value (2 bytes)
  - 3-axis sample = 3 entries = 6 bytes
  - Effective depth: 512 / 3 = 170 complete XYZ readings = 213ms @ 800Hz
  - Watermark at 240 entries (= 80 XYZ readings, ~100ms) — synced with ICM cadence
  - INT1 fires → same sensor_thread handles both

sensor_thread:
  while(1) {
    k_sem_take(&fifo_sem, K_FOREVER);
    // Check overflow status (STATUS reg bit 1) for each sensor
    if (adxl_overflow) { LOG_WRN("ADXL FIFO overflow"); flush; overflow_count++; }
    // Read both FIFOs (check FIFO count first to avoid empty reads)
    if (icm_fifo_count > 0) icm_read_fifo();
    if (adxl_fifo_count > 0) adxl_read_fifo();
  }
```

ADXL372 FIFO read buffer: also `static`, 80 XYZ readings × 6B = 480B.

**Important:** ADXL372 must be in **normal FIFO mode** (not peak detection). Peak mode loses time-domain waveform data needed for Phase 1 features.

**Verification:**
- LOG_INF every second: "ADXL FIFO: %d samples, dt=%d ms"
- Expect: 800 samples/sec, no overflow

---

## Step 5: Dual Sensor Synchronized Reads

Ensure both sensors are read together without data loss.

**Files:**
- `src/sensor.c` — unified FIFO read logic
- `src/main.c` — create sensor_thread

**Design:**
```
sensor_thread (Priority 2, Stack 2KB):
  FIFO read buffers are static — stack only needs SPI driver frames + locals
  Both FIFOs → same semaphore (either INT wakes thread, reads both)
  Check FIFO count before reading to avoid empty SPI transactions
  Timestamp each batch with k_uptime_get()
  Log combined stats
  Use CONFIG_THREAD_ANALYZER=y during bench testing to verify stack usage

Timing budget per wake:
  ICM burst read: ~0.8ms (1,024B @ 10MHz)
  ADXL burst read: ~0.5ms (480B @ 10MHz)
  Total: ~1.3ms per 100ms window → 1.3% CPU
```

**Synchronization approach:**
- No hardware sync line needed for Phase 0 (bench testing)
- Both sensors configured to 800Hz independently
- Timestamp alignment: k_uptime_get() at read time (sufficient for Phase 0)
- Phase 1 may investigate external clock sync if drift is problematic

**Verification:**
```
LOG_INF("Dual sensor: ICM=%d ADXL=%d samples, drift=%d us");
```
- Run for 10 minutes, check no FIFO overflows
- Sample count ratio should stay ~1.0 (both at 800Hz)

---

## Step 6: Impact Detection

Detect impacts using ADXL372 threshold crossing.

**Files:**
- `src/impact.h` — impact detection API
- `src/impact.c` — threshold detection logic

**Design:**
```
Impact detection (software threshold on ADXL372 data):
  - Scan ADXL372 FIFO data for |accel| > IMPACT_THRESHOLD_MG (30,000 mg = 30g)
  - On trigger: log impact with timestamp + peak value
  - Debounce: ignore impacts within 500ms of last (no double-trigger)
  - Phase 0: just detect + log (no ML, no windowing)

// config.h
#define IMPACT_THRESHOLD_MG     30000   /* 30g */
#define IMPACT_DEBOUNCE_MS      500
```

**Note:** ADXL372 has hardware activity detection, but Phase 0 uses software threshold on FIFO data for simplicity and debugging visibility. Hardware threshold can be evaluated for power optimization in Phase 3.

**Verification:**
- Tap desk with hand → LOG_INF("IMPACT: peak=%d mg, t=%d ms")
- No false triggers from gentle handling
- No missed triggers from firm taps

---

## Step 7: ZMS Storage Validation

Verify ZMS (Zephyr Memory Storage) works correctly on nRF54L15 RRAM.

**Files:**
- `prj.conf` — add `CONFIG_ZMS=y`, `CONFIG_FLASH=y`
- `boards/nrf54l15dk_nrf54l15_cpuapp.overlay` — ZMS partition (16KB test area)
- `src/main.c` — ZMS write/read test sequence

**Test procedure:**
```c
// Write 100 shot_event structs, read them back, verify data integrity
struct shot_event test = { .ts_offset_ms = i, .impact_x_mm = 42, ... };
zms_write(&zms, id, &test, sizeof(test));
zms_read(&zms, id, &readback, sizeof(readback));
assert(memcmp(&test, &readback, sizeof(test)) == 0);
```

**What to validate:**
1. Basic write/read round-trip (100 entries)
2. Power cycle persistence (write → reset → read back)
3. Overwrite behavior (write same ID twice)
4. Storage full behavior (fill partition completely, check error codes + GC behavior)
5. Write latency measurement (expect ~22us per write)
6. Wear distribution: write 1,000+ entries, verify ZMS distributes across sectors (not concentrated)

**DT partition (test, 16KB):**
```dts
/* Use the production ZMS config address to avoid overlay rework in Phase 2.
 * Phase 0 does not use MCUboot, so no Slot 0/1 conflict.
 * Verify RRAM base address from nRF54L15 DTS (&flash0 base) before use. */
&flash0 {
    partitions {
        compatible = "fixed-partitions";
        #address-cells = <1>;
        #size-cells = <1>;

        zms_test_partition: partition@FE000 {
            label = "zms_test";
            reg = <0x0FE000 0x4000>;  /* 16KB — matches production ZMS config addr */
        };
    };
};
```

**Verification:**
```
LOG_INF("ZMS: %d/%d writes OK, avg latency %d us");
LOG_INF("ZMS: power-cycle persistence: %s", ok ? "PASS" : "FAIL");
```

**Risk gate (R14):** If ZMS fails on RRAM, this is a critical blocker. Document exact failure mode and check Zephyr issue tracker before escalating.

---

## Step 8: Integration + Bench Verification

Combine all components into a running system and verify on bench.

**Files:**
- `src/main.c` — integrate sensor_thread + impact detection + ZMS
- `src/config.h` — all constants consolidated
- `src/led.c/.h` — simple status LED (optional but useful for visual feedback)

**Integration behavior:**
```
Boot:
  1. Init LED (blink pattern: booting)
  2. Init SPI bus
  3. Init ICM-42688-P (WHO_AM_I check → LOG_INF)
  4. Init ADXL372 (DEVID check → LOG_INF)
  5. Init ZMS (mount → write/read test → LOG_INF)
  6. Start sensor_thread (FIFO reads begin)
  7. LED solid: ready

Running:
  - sensor_thread reads both FIFOs on watermark interrupt
  - impact.c scans ADXL372 data for threshold crossing
  - On impact: LOG_INF + LED blink + write shot_event to ZMS
  - Every 10 seconds: LOG_INF summary (sample rates, impact count, ZMS entries)
```

**Bench test protocol:**
1. Power on DK, connect minicom
2. Verify boot log (WHO_AM_I, DEVID, ZMS mount)
3. Let run 1 minute — verify stable 800Hz from both sensors
4. Tap desk 10 times — verify 10 impact detections logged
5. Check ZMS: 10 shot_events stored
6. Power cycle — verify ZMS data persists
7. Run 10 minutes — verify no FIFO overflows, no crashes

---

## Step 9: TPU Damping Validation

Validate that a TPU enclosure does not attenuate vibration by more than 50% — a Go/Kill gate criterion.

**Requires:** 3D-printed TPU test coupon (Shore A 92-95), racket with strings, tennis balls

**Test protocol:**
1. Tape-mount eval boards at dampener position on racket strings (bare, no case)
2. Hit 20+ balls (ball machine or hand-fed), log ADXL372 peak amplitudes + ICM-42688-P vibration RMS
3. Enclose sensors in TPU test coupon, repeat same test
4. Compare: `damping_ratio = peak_with_tpu / peak_bare`
5. **Pass:** damping_ratio >= 0.50 (less than 50% attenuation)
6. **Fail:** If >50% attenuation, investigate Shore A hardness, friction-fit gap, potting compound

**Also validates:**
- ADXL372 sees impacts in expected 50-150g range on real racket
- FIFO does not overflow under real vibration conditions
- ICM-42688-P vibration decay pattern is visible post-impact
- Actual peak g-forces at dampener location (data for Phase 1)

**Risk R4:** Medium probability, High impact. If TPU damping is too high, the entire sensor placement approach needs revision.

---

## File Summary

| File | Step | Purpose |
|------|------|---------|
| `CMakeLists.txt` | 0 | Zephyr project build |
| `prj.conf` | 0,1,2,7 | Kconfig options |
| `boards/nrf54l15dk_nrf54l15_cpuapp.overlay` | 0,1,2,7 | Device tree |
| `src/main.c` | 0,7,8 | Init + event loop |
| `src/config.h` | 0,6,8 | Constants + thresholds |
| `src/sensor.h` | 1 | Sensor API |
| `src/sensor.c` | 1,2,3,4,5 | Dual sensor driver |
| `src/impact.h` | 6 | Impact detection API |
| `src/impact.c` | 6 | Threshold detection |
| `src/led.h` | 8 | LED API (optional) |
| `src/led.c` | 8 | LED patterns (optional) |

## Resource Budget (Phase 0)

| Resource | Estimated | Budget | Notes |
|----------|-----------|--------|-------|
| Flash | ~30 KB | 500 KB (Slot 0) | Zephyr + SPI + sensor drivers + logging |
| RAM | ~22 KB | 256 KB | sensor_thread stack (2KB) + static FIFO buffers (~1.5KB) + ZMS |
| SPI bandwidth | ~1.3ms / 100ms | 100ms window | 1.3% utilization |
| GPIO | 6 pins (SPI3 + CS2 + INT1) | 13 budgeted | +LED +INT2(reserved) = 8 |

## Risks (Phase 0 Specific)

| Risk | ID | Mitigation | Checkpoint |
|------|----|------------|------------|
| ICM-42688-P Zephyr driver issues | R2 | Validate WHO_AM_I first, check #84833 | Step 1 |
| nRF54L15 Zephyr maturity | R11 | Fallback to nRF52840 DK | Step 0 |
| RRAM/ZMS compatibility | R14 | Early validation in Step 7 | Step 7 |
| SPI bus contention (shared bus) | — | Sequential reads, not concurrent | Step 5 |

## Open Questions to Resolve

| Q | Question | Resolved By |
|---|----------|-------------|
| Q1 | ICM-42688-P Zephyr driver stability on nRF54L15? | Step 1: WHO_AM_I + Step 3: FIFO streaming |
| Q2 | Dual sensor FIFO synchronization timing? | Step 5: drift measurement over 10 min |
| Q9 | 16-bit vs 20-bit FIFO mode? | Step 3: start with 16-bit, note if 20-bit needed |
| Q11 | ZMS API patterns for session/calibration design? | Step 7: hands-on validation |

---

## Dependencies for Phase 1

Phase 0 outputs that Phase 1 builds on:
- `sensor.c/h` — reused directly for data collection
- `impact.c/h` — triggers ML feature window capture
- Device tree overlay — reused (add BLE in Phase 2)
- ZMS validation — informs session.c design
- 800Hz stability data — confirms ODR choice for ML training
- TPU damping data — confirms sensor enclosure approach

---

## Plan Validation (3-Agent Consensus)

**Method:** 2/3 consensus from 3 independent validators
**Overall:** APPROVED (all findings addressed)

### Consensus Findings Applied

| # | Finding | Severity | Resolution |
|---|---------|----------|------------|
| 1 | ICM-42688-P FIFO packet = 16B not 12B (1B header) | Warning | Fixed Step 3: 16B packets, 128 max, watermark 64 |
| 2 | ADXL372 FIFO = 170 XYZ readings not 512 | Warning | Fixed Step 4: 512 entries / 3 axes = 170 readings |
| 3 | sensor_thread 2KB stack + large buffers = overflow | Warning | Fixed: static FIFO buffers, CONFIG_THREAD_ANALYZER |
| 4 | Zephyr sensor driver vs raw SPI must choose one | Warning | Added Architecture Decision section before Step 0 |
| 5 | ZMS partition 0x100000 overlaps production layout | Warning | Fixed Step 7: use production addr 0x0FE000 |
| 6 | No FIFO overflow detection or recovery | Warning | Fixed Steps 3-4: overflow check + flush + counter |
| 7 | TPU damping test missing from Go/Kill gate | Warning | Added Step 9 + Go/Kill checkbox |
| 8 | AAF config needs multi-register Bank 2 writes | Warning | Added AAF Configuration section in Step 1 |
| 9 | Shared semaphore loses interrupt source | Suggestion | Noted: check FIFO count before read (Step 4) |
| 10 | INT2 (WOM) pin not reserved | Suggestion | Added commented-out INT2 in DT overlay |
| 11 | SPI SCK must be on 64MHz GPIO domain | Suggestion | Added pin constraint note in DT overlay |
| 12 | No WDT for 10-min stability test | Suggestion | Accepted risk for Phase 0 bench testing |
| 13 | ZMS wear distribution not tested | Suggestion | Added test #6 in Step 7 |

### Dropped (1/3 only — noise)
Startup delay, SPI mode default, init error recovery, FPU config, logging rate, impact threading, ZMS write latency
