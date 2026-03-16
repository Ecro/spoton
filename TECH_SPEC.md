# SpotOn — Technical Specification v0.3.0

#spoton #zephyr #nrf54l15 #icm42688p #adxl372 #tinyml #ble #tennis-sensor #2026

> **"Are you spot on?"** — Racket face hit-map + sweet-spot rate sensor
>
> **v0.3 Change Summary (6-Agent Research + 3-Agent Plan Validation):**
> - MCU: nRF52840 → **nRF54L15** (same 6×6mm QFN48, 2× faster 128MHz M33, 1,524KB RRAM, BLE 5.4, **cheaper**)
> - IMU: BMI270 → **ICM-42688-P** (fixes critical 800Hz aliasing, programmable AAF, 2× lower noise 70µg/√Hz)
> - High-g: ADXL375 → **ADXL372** (16× deeper FIFO 512 vs 32, 6.4× lower power, official Zephyr driver)
> - Storage: FCB+NVS → **ZMS** (RRAM requires Zephyr Memory Storage, not Flash-based FCB/NVS)
> - Battery: Added **PORON foam + PU conformal coat** for high-g shock protection
> - NVM: 1MB Flash → **1,524KB RRAM**, expanded partitions
> - Power: ACTIVE mode reduced (PRELIMINARY — pending datasheet verification)
> - BOM: ~$2/unit savings (nRF54L15 cheaper than nRF52840)
> - New: **Component Watch List** section for future upgrades
>
> **v0.2 Change Summary (3-Agent Consensus Review applied):**
> - MCU: nRF5340 → nRF52840 (6×6mm, 13mm² saved, sufficient performance)
> - Sensors: BMI270 alone → BMI270 + ADXL375 dual sensor (resolves ±16g saturation)
> - ODR: 400Hz → 800Hz (captures 382Hz torsion mode)
> - Window: 50ms → 150ms asymmetric (-10ms pre + 140ms post)
> - ML: RF Regressor → SVR (RBF kernel) (better fit for continuous coordinate regression)
> - New: Swing speed algorithm, Impact face angle, Calibration protocol
> - Storage: NVS → FCB (optimal for bulk sequential writes)
> - Charging: USB-C → Magnetic Pogo Pin (impact durability)
> - Antenna: PCB trace → Chip Antenna (minimal battery interference)
> - BLE: UUID revision + DIS/BAS standard services + Indicate
> - Flash: MCUboot partition Day 1 design
> - Safety: NTC thermal protection, WDT, ARMED state added

---

## 1. Overview

### 1.1 Purpose

MVP technical specification for an open-source sensor embedded in a tennis dampener form factor with dual sensors (IMU + high-g accelerometer) that estimates **where** the ball hits the racket face and provides **hit-map + sweet-spot rate + face angle + swing speed**.

### 1.2 Product Definition

| Item | Detail |
|------|--------|
| Product Name | SpotOn |
| Core Value | "Open the app after your session — see your racket face hit-map. Today's sweet-spot rate: 68%." |
| Form Factor | Dampener-mount sensor (installed between strings) |
| Target | Tech-savvy recreational tennis players |
| Price | DIY Kit $25~35 / Assembled $49~65 |
| Differentiator | Racket face impact location + face angle — impossible to measure with cameras |

### 1.3 MVP Scope

**In Scope (MVP):**
- Impact detection (ADXL372 high-g accelerometer threshold)
- ★ **Impact location estimation** (string vibration pattern ML → racket face XY coordinates, ≤4cm)
- ★ **Sweet-spot rate calculation** (percentage within sweet-spot area)
- ★ **Impact face angle** (open/closed/flat, ±5°)
- ★ **Swing speed** (gyro-based racket head speed, ±15%)
- Stroke classification (forehand / backhand / serve / volley / slice, ≥90%)
- On-device local session storage (ZMS)
- BLE batch sync (post-session → phone app)
- Flutter app: racket face hit-map visualization + sweet-spot rate + face angle + trends
- Per-racket quick calibration (20 shots, 2 minutes)

**Out of Scope (Post-MVP):**
- Spin estimation (ball spin not measurable from IMU alone)
- Real-time BLE streaming (v0.2: LIVE mode architecture prep only)
- String tension monitoring (v0.4)
- Arm load monitor (v0.4)
- Racket comparison / Racket DNA (v0.5)
- Gamification (v0.5)
- Coach dashboard (v0.6)
- Certification (FCC/KC/CE, Phase 3+)

### 1.4 Success Criteria

| Criterion | Target | Measurement |
|-----------|--------|-------------|
| Impact location accuracy | ≤ 4cm MAE (calibrated racket) | Ball machine + high-speed camera cross-validation |
| Stroke classification accuracy | ≥ 90% (5-class) | Confusion matrix vs video labels |
| Sweet-spot rate reliability | ±5% vs manual count | 3 sessions × video verification |
| Swing speed accuracy | ±15% vs radar gun | 50 serves radar gun comparison |
| Impact face angle | ±5° | High-speed camera frame comparison |
| Battery life | 4+ hours continuous play | Measured |
| Weight | ≤ 10g | Scale |
| BLE sync time | 1-hour session < 5 seconds | Measured |

---

## 2. System Architecture

### 2.1 High-Level Architecture

```
┌────────────────────────────────────────────────────────────┐
│              SpotOn Sensor (Dampener Form Factor)           │
│                                                            │
│  ┌────────────┐  ┌────────────┐  ┌─────────────────┐      │
│  │ Sensor      │  │ ML         │  │ Session         │      │
│  │ Thread      │  │ Thread     │  │ Manager Thread  │      │
│  │             │  │            │  │                 │      │
│  │ ICM-42688-P │  │ Impact     │  │ ZMS storage     │      │
│  │ 800Hz 6-axis│  │ Locator    │  │ BLE sync        │      │
│  │ ADXL372     │  │ Shot Class │  │ Power mgmt      │      │
│  │ 800Hz 3-axis│  │ Face Angle │  │ Time sync       │      │
│  │ FIFO+INT    │  │ Swing Spd  │  │                 │      │
│  └──────┬──────┘  └──────┬─────┘  └───────┬─────────┘      │
│         │               │                 │                 │
│  ┌──────▼───────────────▼─────────────────▼──────────┐      │
│  │                Main Thread                         │      │
│  │  State: SLEEP → ARMED → ACTIVE → SYNC → SLEEP     │      │
│  │  Watchdog Timer (10s)                              │      │
│  └────────────────────────────────────────────────────┘      │
│                                                            │
│  HW: nRF54L15 (128MHz Cortex-M33, 1,524KB RRAM, 256KB RAM)│
│      ICM-42688-P (SPI, 800Hz) + ADXL372 (SPI, 800Hz)      │
│      LiPo 100mAh + nPM1100 + NTC                          │
│      Chip Antenna (2.4GHz) + Magnetic Pogo Charge          │
└────────────────────┬───────────────────────────────────────┘
                     │ BLE 5.4 (post-session batch)
                     ▼
┌────────────────────────────────────────────────────────────┐
│               SpotOn App (Flutter)                          │
│  ★ Racket face hit-map (scatter + heatmap)                  │
│  ★ Sweet-spot rate % + face angle distribution + swing speed│
│  ★ Per-stroke filter, trends, calibration                   │
│  Local DB: SQLite (versioned schema, Riverpod)              │
└────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow (ACTIVE State)

```
ICM-42688-P 800Hz (accel+gyro) ─┐
                                ├─→ FIFO watermark INT
ADXL372 800Hz (high-g accel) ───┘      → Sensor Thread burst read

Sensor Thread:
  1. Burst read ICM-42688-P FIFO + ADXL372 FIFO
  2. Impact Detection:
     ADXL372 |a| > IMPACT_THRESHOLD_G (e.g., 30g)
     ├─ No → Ring buffer cycles, wait for next FIFO
     └─ Yes →
  3. Capture window: -10ms pre + 140ms post = 150ms total
     ICM-42688-P: 120 samples × 12B = 1,440B
     ADXL372: 120 samples × 6B = 720B
  4. → imu_q (sensor → ML thread)

ML Thread:
  5. Feature extraction (39 features from combined sensor data)
  6. [Impact Locator SVR] → (x_cm, y_cm) + calibration offset
  7. [Shot Classifier RF] → forehand/backhand/serve/volley/slice
  8. [Face Angle] → 50ms pre-impact gyro+accel → pitch angle (°)
  9. [Swing Speed] → 100ms pre-impact gyro peak → racket head speed
 10. [Sweet Spot Judge] → √(x²+y²) ≤ SWEET_SPOT_RADIUS
 11. → shot_q (ML → Session thread)

Session Thread:
 12. Create shot_event struct (20 bytes)
 13. ZMS append (shot events storage)
```

---

## 3. Hardware Specification

### 3.1 MCU: Nordic nRF54L15

> **v0.3 Change:** nRF52840 (Cortex-M4F, 64MHz, 1MB Flash) → **nRF54L15 (Cortex-M33, 128MHz, 1,524KB RRAM)**
> Rationale: 2× faster ML inference (quicker return to sleep = better battery life), 1,524KB RRAM provides 49% more storage, BLE 5.4 future-proofs for BT 6.0, and unit cost is **lower** (~$2.60 vs ~$4.70). Same 6×6mm QFN48 package — no PCB redesign needed.
>
> Note: nRF54L15 VDD range is 1.7–3.6V (no VDDH). The nPM1100 1.8V buck output connects directly — no design change vs nRF52840.
> Note: nRF54L15 uses RRAM instead of NOR Flash. RRAM has different write/erase semantics requiring ZMS instead of FCB/NVS.

| Parameter | Value |
|-----------|-------|
| CPU | Arm Cortex-M33, 128MHz, FPU, DSP + RISC-V coprocessor |
| NVM | 1,524 KB RRAM |
| RAM | 256 KB |
| BLE | 5.4 (2Mbps PHY, DLE, Long Range, BT 6.0 ready) |
| USB | Not integrated (external UART for debug) |
| SPI | High-speed SPIM (×2: ICM-42688-P + ADXL372) |
| GPIO | 31 (2 ports: 11 @ 64MHz domain + 20 @ 16MHz domain) |
| ADC | 14-bit, 8-channel, 2 MS/s |
| Package | QFN48, **6×6mm** |
| Temp Range | -40°C ~ 105°C (system limited by battery: -10°C ~ 55°C) |
| Security | PSA Level 3, TrustZone, tamper detection |
| Zephyr | Supported by nRF Connect SDK (DK in mainline) |

**Pin Budget (31 GPIOs):**

| Signal | Count | Notes |
|--------|-------|-------|
| SPI (SCK, MOSI, MISO) | 3 | Shared bus, SCK on 64MHz port |
| CS (ICM-42688-P, ADXL372) | 2 | |
| INT (ICM-42688-P ×2, ADXL372 ×1) | 3 | FIFO watermark + WOM |
| ADC (NTC, VBAT) | 2 | |
| LED | 1 | Status indicator |
| Charging (VBUS detect, status) | 2 | |
| **Used** | **13** | **18 GPIOs free** |

### 3.2 Dual Sensor Configuration

> **v0.3 Change:** BMI270 → **ICM-42688-P**, ADXL375 → **ADXL372**
> Rationale: BMI270's fixed 740Hz LPF causes aliasing at 800Hz ODR — vibration energy between 400–740Hz folds into measurement band, corrupting ML features. ICM-42688-P's programmable AAF solves this. ADXL372 has 16× deeper FIFO, 6.4× lower power, and an official upstream Zephyr driver.

#### 3.2.1 ICM-42688-P (Vibration Analysis + Gyro)

> **v0.3 Change:** BMI270 → **ICM-42688-P**
> Critical fix: BMI270's 740Hz fixed LPF at 800Hz ODR provides no anti-aliasing (Nyquist = 400Hz). String vibration energy from 400–740Hz aliases into the measurement band, corrupting ML features. The ICM-42688-P's **programmable anti-aliasing filter** can be set to ~350Hz independently of ODR, providing clean frequency capture. Additionally offers 2× lower noise floor for better vibration pattern discrimination.

| Parameter | Value | Notes |
|-----------|-------|-------|
| Role | Post-impact string vibration + swing gyro | Core of position estimation |
| Accel Range | **±16g** | For vibration decay analysis (post-saturation decay) |
| Gyro Range | ±15.625–2000 dps | More granular selection than BMI270 |
| ODR | **800Hz** (accel + gyro) | Supports up to 32kHz if needed |
| AAF | **Programmable 2nd-order** (set ~350Hz for 800Hz ODR) | Critical improvement — independent of ODR |
| Accel Noise | **70 µg/√Hz** | 2× lower than BMI270 (150 µg/√Hz) |
| Gyro Noise | **~2.8 mdps/√Hz** | 2.5× lower than BMI270 |
| FIFO | 2 KB (supports **20-bit** data resolution) | Higher precision available |
| Current (6-axis LNM, 800Hz) | **880 µA** | +195µA vs BMI270 (trade-off for better noise/AAF) |
| Shock Resistance | 20,000g | Same as BMI270 |
| Package | LGA-14, **2.5×3.0×0.91mm** | Same footprint, +0.08mm height |
| Interface | SPI, **24MHz** max | Faster SPI than BMI270 (8MHz) |
| Init | Simple register config | No 8KB config blob upload (unlike BMI270) |
| External Clock | Yes (31–50 kHz) | Reduces ODR jitter across units |
| Zephyr Driver | `CONFIG_ICM42688=y` (upstream) | Verify stability — known issues in #84833 |

#### 3.2.2 ADXL372 (High-g Impact Capture)

> **v0.3 Change:** ADXL375 → **ADXL372**
> Rationale: 16× deeper FIFO (512 vs 32 samples), 6.4× lower power (22µA vs 140µA), official upstream Zephyr driver (ADXL375 had none), smaller footprint (3×3.25mm vs 3×5mm), instant-on mode (1.4µA) for SLEEP state monitoring.
> Trade-off: Resolution drops from 13-bit 49mg/LSB (ADXL375) to 12-bit 100mg/LSB (ADXL372). Acceptable for impact detection and peak features — evaluate ML impact during Phase 1 data collection.

| Parameter | Value | Notes |
|-----------|-------|-------|
| Role | Impact peak waveform capture (50-150g) | Impact intensity + position assist |
| Accel Range | **±200g** | Full tennis impact coverage (same as ADXL375) |
| Resolution | **12-bit** | **100mg/LSB** (vs ADXL375 13-bit 49mg/LSB — known trade-off) |
| ODR | **800Hz** | Synced with ICM-42688-P |
| AAF | **4-pole selectable** | Better filtering than ADXL375 |
| FIFO | **512 samples** | 213ms buffer @800Hz (vs ADXL375 32 samples = 13ms) |
| Current (800Hz) | **22 µA** | 6.4× lower than ADXL375 (140µA) |
| Peak Detection | **Hardware peak mode** | Future power optimization (NOT used in MVP — see note) |
| Instant-on | **1.4 µA** | For SLEEP state impact monitoring |
| Shock Resistance | **10,000g** | Same as ADXL375 |
| Package | LGA-16, **3×3.25×1.06mm** | Smaller footprint than ADXL375 (3×5mm), verify height vs datasheet |
| Interface | SPI, 10MHz | CS1 |
| Zephyr Driver | `CONFIG_ADXL372=y` (**official upstream**) | With RTIO streaming support |
| Unit Cost | ~$3~4 | Same as ADXL375 |

> **Important: FIFO Mode Selection.**
> ADXL372 must operate in **normal FIFO mode** (not peak detection mode) for MVP feature engineering. Peak mode only stores maximum values per measurement period, losing time-domain waveform data required for features #2 (impulse duration), #3 (impulse energy), and #14 (time-to-peak). Peak detection mode is reserved for future power-optimized monitoring modes.

**Dual Sensor Role Division:**

```
Impact Timing    ADXL372 (±200g)              ICM-42688-P (±16g + gyro)
──────────────────────────────────────────────────────────────────
  -100ms                                      Swing gyro → face angle, swing speed
  -10ms                                       Swing accel (non-saturated range)
   0ms           ★ Impact peak (50-150g)      Saturated (16g clipping) — ignore peak
  +5ms           Impact ends                  ★ Vibration decay begins (returns to ±16g range)
  +10-140ms                                   ★ String vibration pattern → position ML
──────────────────────────────────────────────────────────────────
                 Impact intensity,            Vibration characteristics → position estimation
                 speed correction             + gyro → face angle, classification
```

### 3.3 Battery & Power Management

| Parameter | Value | Notes |
|-----------|-------|-------|
| Battery | LiPo 3.7V, **100mAh**, Grepow GRP3515025 (25×15×3.5mm, 2.5g) | **High-temp discharge rated 60°C** |
| PMIC | Nordic **nPM1100** (WLCSP 2.075×2.075mm) | Charging + 1.8V buck |
| NTC | Murata NCP15XH103F03RC (0402, 10kΩ, B=3950K) | **Connected to nPM1100 NTC input** |
| Charging | **Magnetic Pogo Pin** (2-pin: VBUS + GND) | USB-C removed — impact durability |
| Charge Current | 50mA (nPM1100 configured) | 100mAh → 2 hours full charge |
| Regulator Output | 1.8V DC/DC buck, 92% efficiency | nRF54L15 VDD: 1.7–3.6V (compatible) |

**Thermal Protection (v0.2):**
- nPM1100 NTC input: Auto-stops charging above 45°C (hardware)
- Firmware: ADC reads NTC → 55°C warning, 60°C shutdown
- Case color: **White or light-colored TPU** (~40% reduction in solar absorption)
- Operating temp: -10°C ~ 55°C / Charging temp: 5°C ~ 45°C

**Battery Mechanical Protection (v0.3 new):**
- **PORON closed-cell foam** (0.5mm) wrap around battery — vibration damping, compression-set resistant
- **Polyurethane conformal coating** — moisture seal + vibration damping
- **Strain relief** on battery tab-to-PCB solder joints — prevent fatigue at 50-150g impacts
- Rigid inner cradle within TPU dampener housing

**Power Budget (PRELIMINARY — pending nRF54L15 datasheet verification):**

| Mode | System Load @1.8V | Battery Current @3.7V (η=92%) | Notes |
|------|-------------------|-------------------------------|-------|
| **ACTIVE** | nRF54L15 TBD + ICM-42688-P 880µA + ADXL372 22µA + HFXO TBD | **TBD** | Verify nRF54L15 electrical specs |
| **ARMED** | nRF54L15 TBD + ICM-42688-P WOM ~14-20µA + ADXL372 1.4µA | **TBD** | ICM-42688-P WOM current: verify DS-000347 |
| **SLEEP** | nRF54L15 ~0.8µA (Global RTC) + ICM-42688-P WOM ~14-20µA | **TBD** | RTC + wake-on-motion wait |
| **SYNC** | nRF54L15 BLE TX ~4.8mA @0dBm | **TBD** | Few seconds |

> **Note:** All power figures are PRELIMINARY estimates. The nRF54L15 active current, HFXO current, and ICM-42688-P WOM current must be verified against official datasheets before finalizing. Previous v0.2 ACTIVE budget was 4.31mA @1.8V → 2.28mA @3.7V with nRF52840+BMI270+ADXL375. The v0.3 configuration is expected to be **lower** due to nRF54L15's improved efficiency and ADXL372's dramatically lower power, but exact figures are pending.

**Battery Life (100mAh, estimated):**

| Scenario | Estimate | Notes |
|----------|----------|-------|
| Continuous play | **Significantly exceeds 4-hour target** | v0.2 was 44 hours; v0.3 expected better |
| 3×/week, 1.5h/session | **8+ weeks** | Conservative estimate pending final power figures |

### 3.4 Form Factor & Mechanical

| Parameter | Target |
|-----------|--------|
| Dimensions | ≤ 30 × 28 × 12mm |
| Weight | ≤ 10g |
| PCB | 2-layer flexible, ~22×26mm |
| Antenna | **Chip antenna** (Johanson 2450AT18A100, 2.0×1.25mm) + π matching |
| Case | TPU Shore A 92-95 (white), 3D printed |
| Charging | Bottom Pogo pad (2mm diameter, gold plated) + magnet alignment |
| PCB↔Case | **Friction-fit cavity** (±0.1mm) — minimal air gap |

**Height Stack-up Analysis:**

> **v0.3 Note:** ICM-42688-P is 0.91mm tall (vs BMI270 0.83mm, +0.08mm). ADXL372 package height to be verified against datasheet (v0.2 listed ADXL375 as 1.45mm — needs confirmation). ADXL372 footprint is smaller: 3×3.25mm vs ADXL375 3×5mm, saving ~5.25mm² PCB area.

| Layer | Height |
|-------|--------|
| Bottom PCB (nRF54L15 QFN 0.85mm + PCB 0.2mm + solder 0.1mm) | 1.15mm |
| Flex bend | 0.5mm |
| Top PCB (ICM-42688-P 0.91mm + ADXL372 ~1.06mm + PCB 0.2mm + solder) | ~1.37mm |
| Clearance | 0.3mm |
| Battery + PORON foam (Grepow GRP3515025, 3.5mm + 1mm foam) | 4.5mm |
| Case walls (TPU 1.5mm × 2) | 3.0mm |
| **Total outer height** | **~10.8mm** |

Weight budget: nRF54L15 ~0.15g + ICM-42688-P+ADXL372+nPM1100+passives ~0.4g + PCB 0.4g + battery 2.5g + magnet 0.3g + PORON foam 0.2g + TPU case 3.0g = **~7.0g** (target ≤10g, 3.0g margin)

### 3.5 MVP BOM (Dev Kit)

| # | Part | Model | Cost |
|---|------|-------|------|
| 1 | Dev Kit | nRF54L15 DK (Nordic) | ~$42 |
| 2 | IMU Eval Board | EV_ICM-42688-P (TDK InvenSense) | ~$15 |
| 3 | High-g Accel Eval Board | EVAL-ADXL372Z (Analog Devices) | ~$25 |
| 4 | Battery | Adafruit 1570 — LiPo 3.7V 100mAh | ~$6 |
| 5 | Charger | Adafruit 4410 — Micro-Lipo USB-C | ~$7 |
| 6 | Misc | Jumper wires, tape, velcro | ~$5 |
| | **Total** | | **~$100** |

> **Dev Kit Decision (resolved):** nRF54L15 DK selected. Firmware targets nRF54L15 directly — no porting step needed for custom PCB.
> **Supplier:** All parts available from Mouser Korea (kr.mouser.com) in single order.

### 3.6 Custom PCB BOM (Phase 2)

| # | Part | 1pcs | 100pcs |
|---|------|------|--------|
| 1 | nRF54L15 QFAA (QFN48) | $4 | $2.60 |
| 2 | ICM-42688-P (LGA-14) | $4 | $2 |
| 3 | ADXL372 (LGA-16) | $4 | $3 |
| 4 | nPM1100 (WLCSP) + NTC | $2.10 | $1.05 |
| 5 | Chip Antenna + π match | $0.30 | $0.15 |
| 6 | Crystal (32.768k only — nRF54L15 has internal HFXO option) | $0.50 | $0.20 |
| 7 | Battery 100mAh + magnet + PORON foam | $3.30 | $1.80 |
| 8 | Passives + LED + ESD TVS | $1.50 | $0.50 |
| 9 | PCB (2L Flex, JLCPCB) | $15 | $1 |
| 10 | TPU case (3D print) | $5 | $2 |
| 11 | SMT assembly | $20 | $3 |
| | **Total** | **~$60** | **~$17** |

### 3.7 Pin Assignment (Dev Kit)

> **v0.3 Note:** Pin assignment depends on DK choice (nRF52840 DK vs nRF54L15 DK). Below shows logical signal mapping. Actual pin numbers will be defined in the device tree overlay for the selected board.

```
Dev Kit ─── Sensors
═════════════════════════════

SPI0 (shared bus, 10MHz — limited by ADXL372 max SPI speed)
  SCK   →  (64MHz port pin required on nRF54L15)
  MOSI  →
  MISO  →
  CS0   →  ICM-42688-P (active low)
  CS1   →  ADXL372 (active low)

Interrupts
  INT0  →  ICM-42688-P INT1 (FIFO watermark)
  INT1  →  ICM-42688-P INT2 (WOM — Wake-on-Motion, SLEEP wakeup)
  INT2  →  ADXL372 INT1 (FIFO watermark)

Power / Charging
  ADC0  →  NTC temperature (voltage divider)
  ADC1  →  VBAT battery voltage
  GPIO  →  Charge status LED
  GPIO  →  Pogo VBUS detection (input)

Status
  GPIO  →  LED (status indicator)
```

---

## 4. Software Architecture

### 4.1 RTOS Configuration

| Item | Setting | Rationale |
|------|---------|-----------|
| RTOS | Zephyr (nRF Connect SDK) | nRF54L15 supported |
| Board | `nrf54l15dk/nrf54l15/cpuapp` (or `nrf52840dk/nrf52840`) | See Q8 |
| BLE | `CONFIG_BT=y`, `CONFIG_BT_PERIPHERAL=y` | |
| BLE PHY | `CONFIG_BT_CTLR_PHY_2M=y` | 2Mbps high-speed |
| BLE DIS | `CONFIG_BT_DIS=y` | Standard Device Info |
| BLE BAS | `CONFIG_BT_BAS=y` | Standard Battery Service |
| IMU | `CONFIG_SENSOR=y`, `CONFIG_ICM42688=y` | ICM-42688-P upstream driver |
| High-g | `CONFIG_ADXL372=y` | ADXL372 upstream driver |
| SPI | `CONFIG_SPI=y` | ICM-42688-P + ADXL372 |
| ZMS | `CONFIG_ZMS=y`, `CONFIG_FLASH=y` | **v0.3: Replaces FCB+NVS (RRAM-compatible)** |
| FPU | `CONFIG_FPU=y` | ML inference |
| WDT | `CONFIG_WATCHDOG=y`, `CONFIG_WDT_NRFX=y`, `CONFIG_TASK_WDT=y` | Crash recovery |
| MCUboot | `CONFIG_BOOTLOADER_MCUBOOT=y` | OTA partition prep |
| PM | `CONFIG_PM=y` | Power management |

### 4.2 Thread Model

```
┌─────────────────────────────────────────────────────────────────┐
│ Thread          Priority  Stack   Role                          │
├─────────────────────────────────────────────────────────────────┤
│ sensor_thread   HIGH (2)  2 KB    ICM-42688-P + ADXL372 FIFO   │
│                                   read, impact detection         │
│ ml_thread       MID  (5)  4 KB    SVR + RF inference +           │
│                                   face angle + swing speed       │
│ session_thread  MID  (6)  2 KB    ZMS storage, BLE sync          │
│ main_thread     LOW  (7)  2 KB    State machine, WDT, events     │
└─────────────────────────────────────────────────────────────────┘

IPC:
  imu_q        K_MSGQ  (sensor → ml)       8 slots × 2200 bytes
  shot_q       K_MSGQ  (ml → session)      32 slots × 20 bytes
  event_q      K_MSGQ  (all → main)        16 slots × 8 bytes
```

> **v0.3 Note:** imu_q slot size (2200B) assumes ICM-42688-P in 16-bit FIFO mode. If 20-bit mode is used, each 6-axis sample grows from 12B to ~18-24B, requiring slot size increase to ~3000B (+6.4KB total RAM). Decision deferred to Phase 0 — see Q9.

### 4.3 State Machine

> **v0.2 Change:** ARMED state added — distinguishes picking up racket vs actual play

```
                    ┌──────────┐
                    │  BOOT    │
                    └────┬─────┘
                         │ init sensors, load calibration, sync time if BLE
                    ┌────▼─────┐
              ┌─────│  SLEEP   │◄──────────────────────────────────┐
              │     └────┬─────┘                                    │
              │          │ ICM-42688-P WOM (Wake-on-Motion) INT      │
              │     ┌────▼─────┐                                    │
              │     │  ARMED   │ ICM-42688-P low-power mode          │
              │     │ (standby)│ ADXL372 instant-on (1.4µA)          │
              │     └────┬─────┘                                    │
              │          │           2min no impact                  │
              │          │ First impact    → SLEEP (no session)      │
              │          │ (ADXL372 >30g)                           │
              │     ┌────▼─────┐                                    │
              │     │  ACTIVE  │ 800Hz full sampling                 │
              │     │ (playing)│ ML inference + ZMS storage          │
              │     └────┬─────┘                                    │
              │          │ 5min no impact → session_end              │
              │          │ or app BLE connection                     │
              │     ┌────▼─────┐                                    │
              │     │  SYNC    │ BLE TX + sensor suspend             │
              │     │          │ Time sync received                  │
              │     └────┬─────┘                                    │
              │          │ Transfer complete/disconnect              │
              │          └──────────────────────────────────────────┘
              │
              │ Battery <10% or Temperature >60°C
         ┌────▼──────┐
         │ SHUTDOWN  │ → LED 3× red blink → system off
         └───────────┘

CHARGING (separate path):
  Pogo VBUS detected → ICM-42688-P suspend → nPM1100 charging → LED orange(charging)/green(full)
  ※ Charging only possible when docked. Cannot charge during play.
```

### 4.4 RRAM Partition Layout (MCUboot included)

> **v0.3 Change:** 1MB NOR Flash → **1,524KB RRAM**. Storage subsystem changed from FCB+NVS to **ZMS** (Zephyr Memory Storage) because RRAM has different write/erase semantics from NOR Flash. RRAM does not require erase-before-write, and has ~10,000 write cycle endurance per cell. Nordic recommends ZMS over FCB/NVS for nRF54L-series.

```
nRF54L15 RRAM (1,524 KB):

0x000000 ┌─────────────────────┐
         │ MCUboot Bootloader  │  48 KB
0x00C000 ├─────────────────────┤
         │ Slot 0 (Primary)    │  500 KB — firmware + ML models
         │ Application Image   │
0x085000 ├─────────────────────┤
         │ Slot 1 (Secondary)  │  500 KB — OTA staging
         │ OTA Update Image    │
0x0FE000 ├─────────────────────┤
         │ ZMS (config)        │  16 KB — calibration, device config
0x102000 ├─────────────────────┤
         │ ZMS (shot events)   │  452 KB — session shot data
0x171000 ├─────────────────────┤
         │ MCUboot Scratch     │  8 KB
0x173000 └─────────────────────┘
         Total: 1,524 KB
```

**ZMS Capacity:** 452KB / 20B per shot = **~23,100 shots** = **~46 hours** (at 500 shots/h) — 2.4× improvement over v0.2 (9,800 shots = ~16h)

**RRAM Endurance Analysis:** At 500 shots/h × 4h/week = ~100,000 shots/year. With 452KB across 113 pages (4KB each), each page is rewritten every ~205 shots = ~487 rewrites/year. At ~10,000 cycle endurance: **~20 year expected lifetime** (adequate).

**RRAM Write Latency:** 22µs sequential write (vs NOR Flash ~85ms page erase + write). No erase step needed — faster individual writes but lower endurance per cell.

### 4.5 Source Tree

```
spoton-firmware/
├── CMakeLists.txt
├── prj.conf
├── boards/
│   └── nrf54l15dk_nrf54l15.overlay   # (or nrf52840dk if using nRF52840 DK)
├── child_image/
│   └── mcuboot.conf              # MCUboot config
├── src/
│   ├── main.c                    # init, state machine, WDT, event loop
│   ├── sensor.h / sensor.c       # ICM-42688-P + ADXL372 SPI, FIFO, interrupt
│   ├── impact.h / impact.c       # Impact detection (ADXL372 threshold)
│   ├── ml.h / ml.c               # SVR + RF inference wrapper
│   ├── features.h / features.c   # Feature extraction (39 features)
│   ├── face_angle.h / face_angle.c  # Face angle estimation (complementary filter)
│   ├── swing_speed.h / swing_speed.c # Swing speed estimation (gyro peak)
│   ├── session.h / session.c     # ZMS session management + store/delete
│   ├── ble_svc.h / ble_svc.c     # GATT service + batch transfer
│   ├── calibration.h / calibration.c # Racket calibration logic
│   ├── power.h / power.c         # Power management, SLEEP/ARMED/ACTIVE
│   ├── timesync.h / timesync.c   # BLE time synchronization
│   ├── config.h                  # Constants, thresholds
│   └── led.h / led.c             # Status LED
├── models/
│   ├── impact_locator_svr.h      # emlearn SVR C code
│   ├── shot_classifier_rf.h      # emlearn RF C code
│   └── README.md
├── tools/
│   ├── train_impact_locator.py   # scikit-learn SVR → emlearn
│   ├── train_shot_classifier.py
│   ├── collect_data.py           # BLE raw data collection
│   └── label_impacts.py
├── README.md
└── west.yml
```

---

## 5. ML Pipeline

### 5.1 Impact Locator (Core Model)

> **v0.2 Change:** RF → **SVR (RBF kernel)**, window 50ms → **150ms**, features redesigned

**Input Data (150ms asymmetric window):**
- ICM-42688-P: -10ms pre + 140ms post = 120 samples × 6 axes
- ADXL372: Same window, 120 samples × 3 axes
- Total: 120 × 9 = **1,080 raw values**

**Feature Engineering (39 features):**

| # | Feature | Source | Dims | Notes |
|---|---------|--------|------|-------|
| 1 | ADXL372 peak amplitude (per axis) | ADXL372 | 3 | Accurate peak capture at ±200g (100mg/LSB resolution) |
| 2 | ADXL372 impulse duration | ADXL372 | 1 | Duration above 30g threshold (ms) — requires normal FIFO mode |
| 3 | ADXL372 impulse energy | ADXL372 | 1 | ∫|a|²dt over impulse |
| 4 | ICM-42688-P post-impact decay rate (per axis) | ICM-42688-P accel | 3 | Exponential fit on ±16g range signal |
| 5 | ICM-42688-P post-impact zero-crossing count | ICM-42688-P accel | 3 | Zero crossings within 140ms |
| 6 | ICM-42688-P post-impact RMS (per axis) | ICM-42688-P accel | 3 | 140ms window energy |
| 7 | ICM-42688-P vibration envelope: 1st vs 2nd half energy ratio | ICM-42688-P accel | 3 | 70ms/70ms split |
| 8 | ICM-42688-P inter-axis correlation (XY, XZ, YZ) | ICM-42688-P accel | 3 | Cross-axis vibration phase relationship |
| 9 | ICM-42688-P post-impact dominant frequency (Goertzel at 146Hz, 200Hz) | ICM-42688-P accel | 6 | Goertzel → extract specific frequency energy only (no FFT needed). Cleaner with programmable AAF. |
| 10 | Gyro magnitude at impact | ICM-42688-P gyro | 1 | Swing intensity |
| 11 | Gyro axis ratios at impact | ICM-42688-P gyro | 3 | Swing direction proxy |
| 12 | ADXL372 peak axis ratios | ADXL372 | 3 | Impact directionality |
| 13 | Combined accel magnitude at impact | ADXL372 | 1 | |a| = √(ax²+ay²+az²) |
| 14 | Time-to-peak (ADXL372 per axis) | ADXL372 | 3 | Impact propagation time differences — requires normal FIFO mode |
| 15 | ICM-42688-P decay time constant (τ, per axis) | ICM-42688-P | 2 | A·e^(-t/τ) fit, X and Y only |
| | **Total features** | | **39** | |

**Feature #9 Design Rationale (Goertzel vs FFT):**
- FFT at N=120 samples gives 6.67Hz resolution, but full spectrum computation cost is high
- **Goertzel algorithm**: Computes energy at a specific frequency in O(N). Only 146Hz and 200Hz needed
- 800Hz ODR → 382Hz is below Nyquist (400Hz) but attenuated at BW -3dB
- ICM-42688-P's programmable AAF set to ~350Hz provides clean anti-aliasing, improving Goertzel accuracy vs BMI270's aliased signal
- Use energy ratio of 146Hz (1st bending) and ~200Hz (frame 2nd mode) for position estimation
- Academic finding: eigenfrequency ratios are **force-irrelevant** (Chen et al., 2022) — consistent regardless of impact intensity

**Model: SVR (RBF Kernel)**

| Parameter | Value |
|-----------|-------|
| Type | SVR with RBF kernel (ε-SVR) |
| Output | (x_cm, y_cm) — separate SVR for X and Y |
| C | Selected via cross-validation (1~100) |
| γ | 'scale' (1 / (n_features × variance)) |
| ε | 0.5 (cm tolerance) |
| emlearn | `emlearn.convert(svr_model)` → C header |
| Code Size | ~3KB per axis × 2 = **~6KB** |
| Inference Time | ~50µs per axis × 2 = **~100µs** (faster on nRF54L15 128MHz) |
| Accuracy Target | ≤ 4cm MAE (after calibration) |

### 5.2 Shot Classifier

| Parameter | Value |
|-----------|-------|
| Input | 250ms pre-impact (200 samples × 6 axes, ICM-42688-P) → 30 statistical features |
| Output | forehand / backhand / serve / volley / slice (5-class) |
| Method | **Random Forest** (emlearn, 20 trees, max_depth=8) |
| Code Size | ~5KB |
| Inference Time | ~100µs (faster on nRF54L15) |
| Accuracy Target | ≥ 90% |

### 5.3 Face Angle Estimation (v0.2 new)

**Purpose:** Racket face pitch angle at impact moment (open/closed/flat)

```
Algorithm (Complementary Filter):
  1. 50ms pre-impact (40 samples @ 800Hz)
  2. Accel: Tilt estimation from acceleration (gravity direction)
     θ_accel = atan2(ay, az)  (pitch)
  3. Gyro: Angular velocity integration
     θ_gyro += gx × dt  (dt = 1.25ms)
  4. Fusion: θ = α × θ_gyro + (1-α) × θ_accel  (α = 0.95)
  5. θ at impact moment = face_angle
     0° = flat (vertical), +° = open, -° = closed

Storage: int16_t face_angle_deg10 (0.1° units, -900 ~ +900)
Accuracy: ±5° (pitch only without magnetometer, sufficient)
Coaching value: "Backhand average face angle 12° open → try reducing to 5°"
```

### 5.4 Swing Speed Estimation (v0.2 new)

**Purpose:** Racket head speed estimation (NOT ball speed)

```
Algorithm:
  1. 100ms pre-impact (80 samples @ 800Hz, ICM-42688-P gyro)
  2. ω_peak = max(√(gx² + gy² + gz²)) in rad/s
  3. v_head = ω_peak × RACKET_LENGTH  (default 0.685m)
  4. swing_speed_kmh = v_head × 3.6

RACKET_LENGTH: Configurable from app (0.65~0.72m)
Storage: uint16_t swing_speed (0.1 km/h units)
Accuracy: ±10-15% vs radar gun
Display: Recommend tier display over exact numbers (Fast/Medium/Slow)
Note: This is racket head speed, NOT ball speed — app must clarify this
```

### 5.5 Sweet Spot Judge

```c
#define SWEET_SPOT_RADIUS_CM  4.0f  // Adjustable 3.0~6.0 from app

bool is_sweet_spot(float impact_x, float impact_y) {
    float dist = sqrtf(impact_x * impact_x + impact_y * impact_y);
    return dist <= SWEET_SPOT_RADIUS_CM;
}
```

### 5.6 Memory Budget (ML)

| Component | Flash | RAM |
|-----------|-------|-----|
| Impact Locator SVR ×2 (X, Y) | ~6 KB | ~1.5 KB |
| Shot Classifier RF | ~5 KB | ~1 KB |
| Feature buffer (39 floats) | 0 | 156 B |
| Shot classifier features (30 floats) | 0 | 120 B |
| IMU window buffer (120 × 18B) | 0 | 2,160 B |
| emlearn runtime | ~2 KB | ~200 B |
| Face angle + swing speed | ~1 KB | ~200 B |
| **ML Total** | **~14 KB** | **~5.3 KB** |

> **v0.3 Note:** 1600Hz ODR investigation (Phase 1) — if adopted, IMU window buffer doubles to 4,320B. See Q7.

---

## 6. Calibration Protocol (v0.2 new)

### 6.1 Quick Calibration (Required, 2 minutes)

```
App-guided:
  1. "Hit 20 times at the center of the racket" (ball machine or wall practice)
  2. Device: CALIBRATION state → 800Hz collection → record 20 impacts
  3. ML inference: Compute mean predicted (x, y) of 20 impacts
  4. offset_x = 0.0 - mean_predicted_x
     offset_y = 0.0 - mean_predicted_y
  5. (Optional) Scale: predicted std dev vs expected std dev ratio
  6. ZMS storage:
     struct racket_calibration {
         uint8_t  racket_id;        // 0-7
         int16_t  offset_x_mm;
         int16_t  offset_y_mm;
         uint16_t scale_x_pct;      // 100 = 1.0
         uint16_t scale_y_pct;
         char     name[16];         // "Head Speed Pro"
     };
  7. Runtime application:
     adjusted_x = (raw_x + offset_x) * (scale_x / 100.0)
     adjusted_y = (raw_y + offset_y) * (scale_y / 100.0)
```

### 6.2 Recalibration Triggers

- After restringing (manual selection in app)
- After 30 days (natural tension loss)
- "Recalibrate" button in app

### 6.3 Multi-Racket Support

- Up to 8 racket profiles stored (ZMS, racket_id 0-7)
- Select racket in app before session → send racket_id via BLE
- Default: racket_id = 0

---

## 7. Data Structures

### 7.1 Shot Event (Unified 20 bytes)

```c
struct shot_event {
    uint32_t ts_offset_ms;       // From session start (max ~49 days)
    uint8_t  shot_type;          // 0:FH 1:BH 2:SV 3:VL 4:SL
    int16_t  impact_x_mm;       // Racket face X (mm, ±150)
    int16_t  impact_y_mm;       // Racket face Y (mm, ±200)
    uint8_t  is_sweet_spot;     // 0 or 1
    uint16_t peak_accel_mg;     // ADXL372 peak (mg, ±200,000)
    uint16_t swing_speed;       // Racket head speed (0.1 km/h)
    int16_t  face_angle_deg10;  // Face angle (0.1°, -900~+900)
    uint8_t  racket_id;         // 0-7
};  // Exactly 20 bytes — same struct for ZMS storage + BLE transfer
```

### 7.2 Session Header (20 bytes)

```c
struct session_header {
    uint32_t session_id;
    uint32_t start_timestamp;    // UTC (BLE time sync)
    uint32_t end_timestamp;
    uint16_t shot_count;
    uint8_t  sweet_spot_pct;     // 0-100
    uint8_t  racket_id;
    uint8_t  synced;             // 0: not sent, 1: sent
    uint8_t  reserved[3];
};  // 20 bytes
```

---

## 8. BLE Protocol

### 8.1 Services

> **v0.3 Note:** BLE 5.4 is backward compatible with 5.0. No protocol changes needed. Future BLE 5.4 features (PAwR, eSL) available but not used in MVP.

```
=== Standard Services (Zephyr built-in) ===
Device Information Service (0x180A):
├── Manufacturer Name (0x2A29): "SpotOn"
├── Model Number (0x2A24): "SpotOn-v1"
├── Firmware Revision (0x2A26): "0.3.0"
└── Hardware Revision (0x2A27): "DK-rev1"

Battery Service (0x180F):
└── Battery Level (0x2A19): uint8_t 0-100% (read + notify)

=== Custom Service (128-bit random UUID) ===
SpotOn Service (UUID: a1b2c3d4-e5f6-7890-ab12-cd34ef560001):
│
├── Session Count (UUID: ...0002)
│   Type: uint16_t (read)
│
├── Session Data (UUID: ...0003)
│   Type: byte array (indicate — guaranteed ACK)
│   Packet format:
│     [0x01] Header: {session_id:u32, start:u32, end:u32,
│                     shots:u16, sweet_pct:u8, racket_id:u8}
│     [0x02] Shot (20B): {ts:u32, type:u8, x:i16, y:i16,
│                         sweet:u8, peak:u16, speed:u16,
│                         angle:i16, racket:u8}
│     [0xFF] End: {total_sent:u16, crc32:u32}
│
├── Control Point (UUID: ...0004)
│   Type: uint8_t (write)
│   0x01 = ACK (allow deletion)
│   0x02 = NACK (retransmit)
│   0x03 = Delete All
│   0x04 = SET_TIME(unix_ts:u32)
│   0x05 = SET_RACKET(racket_id:u8)
│   0x06 = START_CALIBRATION
│
└── Device Status (UUID: ...0005)
    Type: struct (read)
    {total_sessions:u32, flash_used_pct:u8, temperature:i8}
```

### 8.2 Time Synchronization

```
On app BLE connection (immediate):
  1. App → Control Point: 0x04 + unix_timestamp (4 bytes)
  2. Device: rtc_offset = unix_ts - (k_uptime_get() / 1000)
  3. All subsequent session timestamps = uptime + rtc_offset

RTC maintenance: nRF54L15 Global RTC runs during SLEEP (0.8µA)
Accuracy: ±2ppm → ±0.17 seconds drift per day (sufficient)
Fallback: Sessions before time sync → app corrects based on sync timestamp
```

---

## 9. Flutter App Architecture (v0.2 enhanced)

### 9.1 Core Screens

| Screen | Function |
|--------|----------|
| **Home** | Last session sweet-spot rate, total sessions, quick sync |
| **Hit-Map ★** | Racket outline + impact dots + hit zones + face angle display + stroke filter |
| **Session Detail** | Shot count, sweet-spot rate, swing speed distribution, face angle distribution |
| **Trends** | Weekly/monthly sweet-spot rate & swing speed changes |
| **Calibration** | Add racket / calibration guide |
| **Settings** | Sweet-spot radius, racket length, data export |

### 9.2 Architecture

```
State Management: Riverpod (AsyncNotifier)
├── SessionNotifier → Session list, current session
├── BleNotifier → Connection state, sync progress
├── HitmapNotifier → Visualization filters, zoom/pan
└── CalibrationNotifier → Calibration state

Repository Layer:
├── SessionRepository (SQLite CRUD + stats computation)
├── BleRepository (flutter_blue_plus abstraction)
└── ExportRepository (CSV/PNG generation)

Database: SQLite (sqflite)
├── Version 1: sessions + shots
├── Version 2: ADD COLUMN face_angle, racket_id
└── Migration: onUpgrade → ALTER TABLE ADD COLUMN (preserves data)
```

### 9.3 Data Export

```
CSV export (per-session or all):
  session_id, timestamp, shot_type, impact_x_cm, impact_y_cm,
  is_sweet_spot, peak_accel_g, swing_speed_kmh, face_angle_deg, racket

Hit-map image: In-app PNG save + share (Share sheet)
JSON backup/restore: For app migration
```

---

## 10. Data Collection Protocol (Improved)

> **v0.2 Change:** Manual grid labeling → **Ball machine + high-speed camera** cross-validation

### Phase 1: Precision Data (Lab)

```
1. Ball machine → fire at target zones with consistent trajectory
2. Smartphone 240fps slow-motion captures impact point
3. Post-hoc labeling: extract contact (x,y) from video frames
4. Label accuracy: ~0.5cm (4× improvement over ~2cm grid method)

Racket face 5×5 grid (4cm spacing):
  100 hits per zone × 25 zones = 2,500 samples (precision labeled)
  Repeat with 3 rackets = 7,500 samples (multi-racket general model)
```

### Phase 2: Field Data (Court)

```
1. Live play + video recording
2. Post-hoc impact location labeling from video (approximate)
3. Include diverse strokes, speeds, spin conditions
4. 500~1,000 additional samples
```

---

## 11. Implementation Phases

### Phase 0: Hello Dual Sensor (Week 1~2)

| Task | Risk |
|------|------|
| Zephyr + DK setup, blinky (nRF52840 DK or nRF54L15 DK) | Low |
| ICM-42688-P SPI 800Hz + FIFO watermark INT | ⚠ Verify Zephyr driver (#84833) |
| ADXL372 SPI 800Hz + FIFO (official Zephyr driver) | Low |
| Dual sensor synchronized reads | Medium |
| Impact detection (ADXL372 threshold, normal FIFO mode) | Low |
| **Verify ZMS storage works correctly on RRAM** | ⚠ RRAM semantics validation |
| Racket tape mount + court impact verification | Low |
| **★ TPU case vs bare PCB vibration damping comparison** | ⚠ Critical validation |
| Measure actual peak g-forces at dampener location | Data collection |

**Go/Kill:** Dual sensor 800Hz stable + impact detection + TPU damping ≤50% + ZMS works on RRAM.

### Phase 1: Hello ML + Accuracy (Week 3~6)

| Task | Risk |
|------|------|
| BLE raw data streaming tool (→ Python CSV) | Medium |
| Ball machine + camera data collection (2,500+ samples) | ⚠ Time-intensive |
| Feature engineering (39 features) | Medium |
| **Evaluate ADXL372 100mg/LSB resolution impact on ML features** | ⚠ Resolution trade-off |
| **Investigate 1600Hz ODR vs 800Hz for vibration capture** | Medium |
| SVR training + emlearn conversion | Medium |
| Shot Classifier RF training | Low |
| Face angle + swing speed algorithm implementation | Medium |
| Zephyr ML inference integration | ⚠ Memory/performance |
| **Calibration protocol implementation + validation** | ⚠ Accuracy-critical |

**Go/Kill:** Post-calibration impact location ≤5cm. Shot classification ≥85%.

### Phase 2: Hello App + Storage (Week 7~10)

| Task | Risk |
|------|------|
| ZMS session storage + ARMED/ACTIVE state machine | Medium |
| WDT + battery monitoring + NTC thermal protection | Low |
| MCUboot partition (DFU not implemented yet) | ⚠ RRAM partition validation |
| BLE GATT (Indicate, DIS, BAS, time sync) | Medium |
| Flutter app: BLE + SQLite + Riverpod | Medium |
| ★ Hit-map canvas + sweet-spot rate + face angle + swing speed | Medium |
| Calibration UI | Medium |
| 10 live court test sessions | ⚠ Expect many bugs |

### Phase 3: Polish + Open Source (Week 11~14)

| Task | Risk |
|------|------|
| 3D print TPU dampener case + Pogo charge dock | ⚠ Mechanical |
| CSV/PNG export | Low |
| SLEEP/ARMED power optimization | Medium |
| GitHub release (README, BOM, build guide) | Low |
| Blog posts (3~5) | Low |
| Beta testers (5~10) | Final validation |

---

## 12. Risk Register

| ID | Risk | Probability | Impact | Phase | Mitigation |
|----|------|-------------|--------|-------|------------|
| R1 | Impact location ≤4cm not achieved | Medium | **High** | 1 | SVR→NN fallback, data augmentation, additional features, enhanced calibration |
| R2 | ICM-42688-P Zephyr driver issues (known bugs #84833, #60549) | **Medium** | **High** | 0 | Phase 0 driver validation gate; fallback to BMI270 if critical |
| R3 | Poor cross-racket model generalization | **High** | Medium | 1 | 3+ racket data, quick calibration 20 shots |
| R4 | TPU case vibration damping >50% | Medium | **High** | 0 | Shore A 95, friction-fit, potting compound. Phase 0 validation |
| R5 | Dampener falls off during play | Medium | Medium | 3 | Dual string clip + silicone O-ring |
| R6 | 100mAh battery doesn't fit form factor | Medium | Medium | 3 | Downsize to 70mAh (still sufficient per power budget) |
| R7 | Summer heat LiPo degradation | Medium | Medium | 0 | NTC + nPM1100 auto-protection + white case + PORON foam insulation |
| R8 | Per-stroke hit-map offset | Medium | Medium | 1 | Include stroke type as SVR input feature |
| R9 | Face angle ±5° not achieved (gyro drift) | Low | Medium | 1 | Integration within 50ms → drift negligible |
| R10 | ZMS partition full (no sync) | Low | Medium | 2 | LED warning, app push notification, auto-delete oldest session |
| R11 | nRF54L15 Zephyr maturity issues | Low | Medium | 0 | Fallback to nRF52840 DK; firmware portable via Zephyr board abstraction |
| R12 | ADXL372 mechanical shock survivability | Low | Medium | 0 | Datasheet lists 10,000g; verify with Analog Devices FAE for dampener use case |
| R13 | ADXL372 resolution (100mg/LSB) insufficient for ML features | Low | Medium | 1 | Evaluate during Phase 1 data collection; features #1,#12,#13 may need adjustment |
| R14 | RRAM endurance or ZMS compatibility issue | Low | **High** | 0 | Phase 0 validation: write-cycle test, verify Zephyr ZMS on nRF54L15 |
| R15 | ICM-42688-P 20-bit FIFO format changes imu_q slot size | Medium | Low | 0 | Use 16-bit mode initially; evaluate 20-bit in Phase 1 |

---

## 13. Open Questions

| # | Question | Decision Point | Impact |
|---|----------|----------------|--------|
| Q1 | ICM-42688-P Zephyr driver stability on nRF54L15? | Phase 0 | Entire sensor pipeline |
| Q2 | ICM-42688-P + ADXL372 FIFO synchronization timing? | Phase 0 | Data consistency |
| Q3 | Are Goertzel 146Hz/200Hz energies significant for position discrimination? | Phase 1 | Feature validity |
| Q4 | SVR vs NN accuracy comparison (same data)? | Phase 1 | Final model selection |
| Q5 | Pogo pin charge dock magnet alignment reliability? | Phase 3 | Charging UX |
| Q6 | MCUboot BLE DFU implementation timing (Phase 2 vs Phase 3)? | Phase 2 | OTA availability |
| Q7 | **1600Hz vs 800Hz ODR** — does higher sampling rate improve vibration harmonic capture for position estimation? | Phase 1 | Feature quality, power, buffer sizes |
| Q8 | ~~nRF52840 DK vs nRF54L15 DK for Phase 0?~~ **RESOLVED: nRF54L15 DK selected.** No porting step needed. | Pre-Phase 0 | Dev environment, build commands |
| Q9 | **ICM-42688-P 16-bit vs 20-bit FIFO mode** — 20-bit gives 16× more resolution but increases buffer size from 2,200B to ~3,000B per imu_q slot. | Phase 0 | RAM budget, feature precision |
| Q10 | **Does ADXL372 100mg/LSB resolution provide sufficient discrimination** for impact position ML features? | Phase 1 | ML accuracy, sensor choice validation |
| Q11 | **ZMS API differences from FCB/NVS** — how do they affect session.c and calibration.c design patterns? | Phase 0 | Storage architecture |

---

## 14. References

| Resource | Key Points |
|----------|------------|
| SmartDampener (Penn State, 2024, Liu et al.) | 3.03cm, 96.75% classification, 6.1g, $9.42, nRF52832 + LSM6DSO, SVR, ACM IMWUT |
| Chen et al. (MDPI Sensors, 2022) | Eigenfrequency ratios are force-irrelevant — position features consistent across impact intensities |
| ICM-42688-P DS (DS-000347-v1.7) | Programmable AAF, 70µg/√Hz noise, 20-bit FIFO, 880µA, 20,000g shock, SPI 24MHz |
| ADXL372 DS (Analog Devices) | ±200g, 512-sample FIFO, 22µA, 4-pole AAF, instant-on 1.4µA, official Zephyr driver |
| nRF54L15 Product Spec | 128MHz Cortex-M33, 1,524KB RRAM, 256KB RAM, BLE 5.4, 6×6mm QFN48, PSA L3 |
| nPM1100 Product Spec | 2.075mm WLCSP, NTC input, 400mA charging, 92% efficiency |
| emlearn | C99, SVR+RF support, nRF52840/nRF54L15 compatible, Zephyr module |
| Penn State Vibration Modes | 1st bending 146Hz, 1st torsion 382Hz, 2nd torsion 411Hz |
| Impact Localization (MDPI 2023) | SVR + Hilbert Transform, IoT MCU, single sensor |
| Goertzel Algorithm | O(N) single-frequency energy extraction, lower cost vs FFT |
| Zephyr ZMS Documentation | RRAM-compatible storage subsystem, replaces FCB+NVS for nRF54L-series |

---

## 15. Component Watch List

> Future component candidates monitored for potential adoption in hardware revisions.

| Component | Why Watch | When to Evaluate |
|-----------|-----------|-----------------|
| **LSM6DSV320X** (ST) | Single-chip: ±320g high-g + ±16g + ±4000dps gyro. Could replace both ICM-42688-P and ADXL372. | When Zephyr driver available |
| **ICM-45605** (TDK) | Best-in-class: 420µA, 8KB FIFO, 70µg/√Hz noise, same 2.5×3mm footprint. | When Zephyr driver available |
| **nPM1300** (Nordic) | Adds fuel gauge (battery %), I2C configurability, watchdog. Larger package. | If fuel gauge becomes critical requirement |
| **BMI570** (Bosch) | Next-gen IMU platform with built-in edge AI classification engine. | Samples Q1 2026, HVM Q3 2026 |
| **Solid-state batteries** | Superior shock survivability, wider temp range. Currently too expensive/limited for small form factors. | When chip-scale SSBs available at ≤2× LiPo cost |

---

*Last updated: 2026-03-16*
*Version: v0.3.0 (6-Agent Research + 3-Agent Plan Validation)*
*Status: UPDATED — 3 component upgrades + FCB→ZMS migration + battery protection*
