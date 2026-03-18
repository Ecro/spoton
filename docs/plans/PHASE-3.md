# Phase 3 — Polish + Open Source

**Objective:** Dampener form factor with custom PCB, power optimization, and open-source release.
**Timeline:** Week 11-14 (flex PCB fab adds 7-14 day lead time — budget for 1 respin)
```
Week 11: KiCad design + Gerber submission + 3D print case prototypes
Week 12: Wait for PCBs + power optimization firmware + production hardening
Week 13: PCB assembly + bring-up + DT overlay + GitHub release prep
Week 14: Beta tester recruitment + blog posts + final docs
```
**Hardware:** Custom folded flex PCB ~22x26mm + TPU case 30x28x12mm + pogo charge dock (~$60 for 1pcs)
**Depends on:** Phase 2 complete (stable firmware, working app, 10 court sessions)
**Created:** 2026-03-18

---

## Go/Kill Gate

- [ ] Weight <=10g with case
- [ ] 4+ hours battery life measured (continuous ACTIVE play)
- [ ] Survives 100+ serve impacts without failure (mechanical)
- [ ] SLEEP current < 50µA (battery life in standby)
- [ ] 5+ beta testers complete full sessions and provide feedback
- [ ] GitHub repo public with complete build guide
- [ ] All TECH_SPEC success criteria met or documented as known limitations

---

## Step 1: Custom PCB Design (KiCad)

Design the folded flex PCB for the dampener form factor.

**Tools:** KiCad 8+

> **Note:** TECH_SPEC §3.4 specifies PCB as "2-layer flexible, ~22×26mm" — this is the
> unfolded dimension. The PCB folds to fit the ~30×28×12mm dampener case. Bottom layer
> holds the nRF54L15 (QFN48 6×6mm), top layer holds sensors. Flex bend connects them.
> The 28×11mm dimension in earlier plans was the dampener string slot, not the PCB.

**PCB specifications (from TECH_SPEC §3.4-3.6):**
```
Board: 2-layer folded flex PCB, ~22×26mm unfolded (fits 30×28×12mm case)
  Bottom layer: nRF54L15 + nPM1100 + passives + pogo pads + antenna
  Flex bend: 0.5mm gap
  Top layer: ICM-42688-P + ADXL372 (sensor side, faces strings)
Components (all SMD):
  - nRF54L15 QFAA (QFN48, 6×6mm)
  - ICM-42688-P (LGA-14, 2.5×3mm)
  - ADXL372 (LGA-16, 3×3.25mm)
  - nPM1100 (WLCSP, 2.075mm) + NTC
  - Chip antenna (Johanson 2450AT18A100, 2×1.25mm) + π match
  - 32.768kHz crystal
  - Battery connector (2-pin)
  - Pogo pad (2× bottom, 2mm dia, gold plated)
  - Status LED (0402)
  - Passives + ESD TVS

Height stack-up: ~10.8mm total (within 12mm target)
Weight: ~4g PCB+components (+ 2.5g battery + 3g case = ~9.5g)
```

**Design checklist:**
- [ ] Schematic: all TECH_SPEC §3 components placed
- [ ] Pin assignment matches production DT overlay
- [ ] SPI traces: SCK short, matched length MISO/MOSI
- [ ] Antenna: chip antenna at board edge/corner, keepout zone per Johanson datasheet (~3-5mm ground clearance, no copper on top layer in keepout)
- [ ] Antenna keepout: verify battery/flex fold does not intrude into keepout on either layer
- [ ] Antenna matching: use 0402 pads for pi network (allow tuning on first prototype)
- [ ] Budget for VNA measurement or RSSI range test on first prototype
- [ ] ESD TVS on pogo VBUS line and SWD pads
- [ ] Battery: connector + PORON foam cavity space
- [ ] Pogo pads: bottom side, magnet alignment holes (test with 3D jig before finalizing)
- [ ] NTC: placed near battery for thermal monitoring
- [ ] Decoupling caps: 100nF on each VDD pin
- [ ] SWD test pads (for programming via DK J-Link)
- [ ] Flex bend: >=1mm bend radius (6× material thickness), no perpendicular traces across bend
- [ ] Stiffeners: polyimide stiffeners on component mounting areas (both sides)
- [ ] Validate JLCPCB flex capabilities: min trace width at bend, stack-up, min bend radius
- [ ] DRC clean, no acute angles on flex areas
- [ ] Budget: ~$30-50 for 5pcs flex prototype (more expensive than rigid FR4)

**Deliverables:**
- KiCad project files (schematic + PCB)
- Gerber files for JLCPCB ordering
- BOM (CSV, matched to JLCPCB parts library)
- Interactive BOM (HTML) for assembly reference

**Verification:**
- DRC + ERC pass in KiCad
- Review antenna placement against Nordic design guidelines
- Cross-check BOM against TECH_SPEC §3.6
- Order 5 PCBs from JLCPCB (~$15 for 5pcs flex)

---

## Step 2: Device Tree Overlay for Custom PCB

Create production device tree overlay matching the custom PCB pin assignment.

**Files:**
- `boards/spoton_v1.overlay` — production board overlay
- `boards/spoton_v1.conf` — board-specific Kconfig

**Design:**
```dts
/* Production board overlay — replaces DK overlay */
/* Pin assignments from KiCad schematic */

&spi0 {
    /* Actual pin numbers from PCB design */
    pinctrl-0 = <&spi0_default>;
    pinctrl-names = "default";
    cs-gpios = <&gpioX N GPIO_ACTIVE_LOW>,   /* ICM-42688-P CS */
               <&gpioX M GPIO_ACTIVE_LOW>;   /* ADXL372 CS */
    /* ... sensor nodes same as DK but with production pins */
};

/* Production partition layout (MCUboot + ZMS) */
&flash0 {
    partitions {
        /* Full TECH_SPEC §4.4 layout */
    };
};
```

**Verification:**
- `west build -b spoton_v1` compiles
- Flash to custom PCB via SWD, boot log matches DK behavior

---

## Step 3: 3D Print TPU Case + Pogo Charge Dock

Design and print the dampener enclosure and charging dock.

**Tools:** Fusion 360 / FreeCAD, TPU filament, FDM printer

**TPU dampener case:**
```
Material: TPU Shore A 92-95, white/light color
Dimensions: ~30×28×12mm outer (fits between strings)
Features:
  - Friction-fit cavity for PCB (±0.1mm tolerance)
  - String clip channels (2× top, for string mounting)
  - Battery compartment with PORON foam cradle
  - Bottom opening for pogo charge pads
  - Minimal air gap between PCB and case (vibration coupling)

Print settings:
  - Layer height: 0.1mm (detail for friction fit)
  - Infill: 100% (solid walls for vibration transmission)
  - No support needed (design for print-in-place)
```

**Pogo charge dock:**
```
Material: PLA or PETG (rigid)
Features:
  - 2× spring-loaded pogo pins (VBUS + GND)
  - Magnet alignment (2× 3mm neodymium magnets)
  - USB-C input → Adafruit Micro-Lipo → pogo pins
  - LED indicator (charging/charged)

Alignment: magnets in dock match magnets in TPU case bottom
```

**Assembly (before casing):**
- Apply polyurethane conformal coating (e.g., HumiSeal 1A33) to PCB
- Brush application for prototype quantities, cure 24h
- Apply BEFORE battery installation (coating must not contact battery)
- Install PORON foam around battery, assemble into case

**Verification:**
- PCB fits snugly in case (no rattle, no excessive force)
- String clips hold case securely on racket (>5N pull-off force)
- String clip tested on 15-18 gauge strings
- Pogo pins make reliable contact (>95% success rate across 20 docking attempts)
- Charging starts when docked (nPM1100 VBUS detect)
- 100 serve impacts with case mounted — no movement, no failure
- TPU damping < 50% (re-verify from Phase 0 with production case)

---

## Step 4: SLEEP/ARMED Power Optimization

Optimize power consumption for battery life target (4+ hours ACTIVE, weeks standby).

**Files:**
- `src/power.c` — power mode optimization
- `src/sensor.c` — sensor suspend/resume optimization
- `prj.conf` — power-related Kconfig tuning

**Power optimization targets:**
```
SLEEP mode:
  Target: < 50µA total system
  - nRF54L15: ~0.8µA (Global RTC only)
  - ICM-42688-P WOM: ~14-20µA
  - ADXL372 instant-on: 1.4µA
  - Regulator quiescent: ~5µA
  Total: ~22-27µA (well under 50µA target)
  Standby life: 100mAh / 0.027mA = ~3,700 hours = ~5 months

ARMED mode:
  Target: < 100µA
  - Same as SLEEP + ICM low-power polling

ACTIVE mode:
  Target: < 25mA system @ 3.7V (= 100mAh / 4h)
  Baseline: measured during Phase 2 court sessions
  - Measure actual current with Nordic PPK2
  - Identify hot spots (BLE? SPI? ML inference?)

Optimizations:
  1. Disable unused peripherals (UART in production, spare GPIOs)
  2. Enable DCDC mode (verify correct Kconfig symbol for nRF54L15 — must be enabled, not optional)
  3. Reduce log level (CONFIG_LOG_DEFAULT_LEVEL=1 in production)
  4. SPI clock gating between reads
  5. Sensor FIFO watermark tuning (larger watermark = less frequent wake)
```

**Measurement setup:**
```
Nordic PPK2 in ampere meter mode:
  - Cut VBAT trace on custom PCB (or use current sense resistor)
  - Measure each state: SLEEP, ARMED, ACTIVE, SYNC
  - Log current over 1-hour session
  - Calculate actual battery life from measured profile
```

**Verification:**
- SLEEP current measured < 50µA
- ACTIVE current measured and documented
- Battery life test: continuous play until shutdown, measure duration
- Target: 4+ hours continuous (100mAh battery)

---

## Step 5: Production Firmware Hardening

Remove debug code, optimize for production.

**Changes:**
```
prj.conf (production):
  CONFIG_LOG_DEFAULT_LEVEL=1        # Errors only
  CONFIG_ASSERT=n                   # Remove debug asserts
  CONFIG_THREAD_ANALYZER=n          # Remove stack analysis
  CONFIG_SIZE_OPTIMIZATIONS=y       # -Os instead of -O2
  # CONFIG_UART_CONSOLE is not set  # Disable serial console
  CONFIG_BOOT_BANNER=n              # Remove boot banner

Code cleanup:
  - Remove printk() statements
  - Remove test/debug code paths
  - Verify all LOG_ERR/LOG_WRN are genuine error conditions
  - Verify WDT feeds in all code paths
  - Final stack usage analysis before removing THREAD_ANALYZER
```

**MCUboot (DFU prep — not implementing OTA yet):**
```
child_image/mcuboot.conf:
  CONFIG_BOOT_SIGNATURE_TYPE_RSA=y
  CONFIG_BOOT_SIGNATURE_KEY_FILE="keys/signing_key.pem"

Partition layout already defined in Phase 2 (Slot 0 + Slot 1).
OTA BLE DFU implementation deferred to post-MVP.
```

**Verification:**
- Production build compiles clean (no warnings)
- Flash size report: fits in Slot 0 (500KB)
- RAM usage report: fits in 256KB
- Boot to SLEEP in < 2 seconds
- All features work identically to debug build

---

## Step 6: GitHub Release

Prepare the open-source release.

**Repository structure:**
```
spoton/
├── README.md              — Project overview, features, photos
├── LICENSE                 — Apache 2.0 (firmware/software) + CERN-OHL-S v2 (hardware)
├── TECH_SPEC.md           — Full technical specification
├── docs/
│   ├── MASTER_PLAN.md
│   ├── BUILD_GUIDE.md     — Step-by-step build instructions
│   ├── BOM.md             — Complete bill of materials with links
│   ├── CALIBRATION.md     — How to calibrate for your racket
│   └── HW_COMPONENTS.md
├── firmware/              — Zephyr project (src/, boards/, prj.conf, CMakeLists.txt)
├── hardware/
│   ├── kicad/             — KiCad project files
│   ├── gerber/            — Manufacturing files
│   ├── bom/               — Interactive BOM (HTML)
│   └── case/              — STL files for 3D printing
├── app/                   — Flutter app (spoton-app/)
├── models/                — emlearn C headers
├── tools/                 — Python training scripts
└── data/                  — Sample training data (subset)
```

**BUILD_GUIDE.md outline:**
```
1. Hardware
   - Parts list with Mouser/JLCPCB links
   - PCB ordering instructions (JLCPCB settings)
   - SMT assembly (reflow vs hand solder)
   - Case printing (TPU settings)
   - Battery + PORON assembly

2. Firmware
   - Install nRF Connect SDK + west
   - Build: west build -b spoton_v1
   - Flash: west flash (via DK J-Link + SWD cable)
   - Verify: minicom serial output

3. App
   - Flutter install
   - Build: flutter build apk
   - Install on Android device

4. Calibration
   - First-time racket setup
   - 20-shot calibration protocol
   - Verify accuracy

5. Usage
   - Play → sync → view hit-map
```

**Verification:**
- Fresh clone + build succeeds (firmware + app)
- BUILD_GUIDE followed by someone unfamiliar with project
- All links in BOM are valid

---

## Step 7: Beta Testing

Recruit 5-10 beta testers for real-world validation.

**Beta program:**
```
Requirements:
  - Tennis player (any level)
  - Android phone with BLE 5.0+
  - Willing to play 3+ sessions and provide feedback

Provide:
  - Assembled SpotOn device
  - Pre-installed app (APK)
  - Calibration guide
  - Feedback form (Google Form)

Feedback areas:
  - Hit-map accuracy ("does it match where you feel the ball hit?")
  - Sweet-spot rate ("does the percentage seem right?")
  - Durability (any falls, failures, disconnects)
  - Battery life (how many sessions before charge?)
  - App UX (confusing screens, missing features)
  - Mounting (does it stay on the racket?)
```

**Success criteria:**
- [ ] 5+ testers complete at least 3 sessions each
- [ ] No device failures (falls off, stops working)
- [ ] Hit-map accuracy "mostly right" from 4/5 testers
- [ ] Battery lasts 1+ hour for all testers
- [ ] Top 3 UX issues documented for post-MVP

> **Note:** Beta testing is Android-only for MVP. iOS app is post-MVP scope.

---

## Step 8: Blog Posts + Documentation

Write 3-5 blog posts AFTER beta testing (incorporate real-world results).

**Suggested posts:**
1. **Project introduction** — What SpotOn does, why, inspiration
2. **Sensor deep-dive** — ICM-42688-P + ADXL372 dual sensor, 800Hz FIFO, anti-aliasing
3. **ML on MCU** — SVR/RF with emlearn, 39 features, Goertzel, inference <2ms
4. **BLE protocol design** — GATT service, Indicate flow, CRC, batch sync
5. **Lessons learned** — What worked, what didn't, what I'd change (includes beta feedback)

**Platforms:** Personal blog, Hackaday.io, r/embedded, Zephyr community

> **Note:** CSV/PNG export was implemented in Phase 2 Step 7.
> TECH_SPEC §11 lists it under Phase 3 but it was moved earlier for court testing utility.

---

## File Summary

| File/Artifact | Step | Purpose |
|---------------|------|---------|
| KiCad project | 1 | Custom PCB design |
| Gerber + BOM | 1 | Manufacturing files |
| `boards/spoton_v1.overlay` | 2 | Production DT overlay |
| STL files (case + dock) | 3 | 3D printing |
| `src/power.c` optimizations | 4 | Power reduction |
| `prj.conf` (production) | 5 | Production Kconfig |
| `child_image/mcuboot.conf` | 5 | MCUboot prep |
| `README.md` | 6 | Project overview |
| `docs/BUILD_GUIDE.md` | 6 | Build instructions |
| `docs/BOM.md` | 6 | Bill of materials |
| Blog posts (3-5) | 7 | Community outreach |
| Beta feedback | 8 | Real-world validation |

## Resource Budget (Phase 3 — Production Build)

| Resource | Debug Build | Production Build | Budget |
|----------|------------|-----------------|--------|
| Flash | ~79 KB | ~60 KB (-Os, no logs) | 500 KB |
| RAM | ~82 KB | ~70 KB (reduced logging) | 256 KB |
| SLEEP current | N/A | < 50µA target | — |
| ACTIVE current | N/A | TBD (measure) | — |
| Weight | N/A | <=10g target | <=10g |
| Battery life | N/A | 4+ hours target | 4+ hours |

## Risks (Phase 3 Specific)

| Risk | ID | Mitigation | Checkpoint |
|------|----|------------|------------|
| Dampener falls off during play | R5 | Dual string clip + silicone O-ring | Step 3 |
| Battery doesn't fit form factor | R6 | Downsize to 70mAh (still sufficient) | Step 1 |
| Summer heat LiPo degradation | R7 | NTC + nPM1100 protection + white case + PORON | Step 3 |
| Custom PCB assembly defects | — | Order 5 PCBs, expect 1-2 failures | Step 1 |
| Beta tester attrition | — | Recruit 10, expect 5 active | Step 8 |

## Phase 2 Completion Checklist (must verify before starting Phase 3)

- [ ] Full session ARMED→ACTIVE→SYNC→SLEEP works reliably
- [ ] ZMS handles 500+ shots without corruption
- [ ] BLE sync < 15 seconds for 500 shots
- [ ] Flutter app: hit-map, calibration UI, export all working
- [ ] 10 court sessions completed without crashes
- [ ] Battery drain rate measured from court sessions
- [ ] Bug list from court testing documented and triaged

---

## Post-Phase 3 (Future Roadmap)

Not in scope for MVP but documented for reference:
- BLE OTA DFU (MCUboot + SMP transport)
- iOS app
- String tension monitoring (v0.4)
- Arm load monitor (v0.4)
- Racket comparison / Racket DNA (v0.5)
- Gamification (v0.5)
- Coach dashboard (v0.6)
- FCC/KC/CE certification

---

## Plan Validation (3-Agent Consensus)

**Method:** 2/3 consensus from 3 independent validators
**Overall:** APPROVED (all consensus findings addressed)

### Consensus Findings Applied

| # | Finding | Validators | Severity | Resolution |
|---|---------|-----------|----------|------------|
| 1 | PCB dimension: 28x11mm → ~22x26mm folded flex | 3/3 | Critical | Corrected to folded flex, documented fold design |
| 2 | Antenna keepout: needs detail + VNA/matching budget | 3/3 | Critical | Added keepout spec, 0402 tuning pads, VNA measurement step |
| 3 | ACTIVE power target undefined (TBD) | 3/3 | Critical | Target <25mA @ 3.7V, measured from Phase 2 baseline |
| 4 | CSV/PNG export listed as Phase 3 but done in Phase 2 | 3/3 | Warning | Added note clarifying moved to Phase 2 |
| 5 | Conformal coating missing | 3/3 | Warning | Added assembly step with coating spec |
| 6 | ESD TVS on pogo pads | 3/3 | Warning | Added to design checklist |
| 7 | Flex PCB: stiffeners, bend radius, cost | 2/3 | Warning | Added stiffener spec, JLCPCB validation, cost estimate |
| 8 | Flex PCB lead time not in timeline | 2/3 | Warning | Added weekly milestones with fab lead time |
| 9 | String clip retention force | 2/3 | Warning | Added >5N pull-off + 100 serve + multi-gauge test |
| 10 | Pogo alignment tolerance | 2/3 | Warning | Added 3D jig verification + 95% reliability target |
| 11 | DCDC mode not optional | 2/3 | Warning | Changed from "if available" to must-enable |
| 12 | Blog before beta | 2/3 | Suggestion | Swapped: beta first, then blog posts |
| 13 | iOS not in beta | 2/3 | Suggestion | Noted: Android-only for MVP |
| 14 | License undecided | 1/3+common sense | Suggestion | Apache 2.0 (SW) + CERN-OHL-S v2 (HW) |

### Dropped (1/3 only)
MCUboot key management, pogo dock electrical detail, TPU tolerance, waterproofing, production test procedure, battery tab strain relief, beta battery inconsistency, BLE sync mismatch, WDT coverage analysis, mechanical shock test, DT overlay validation
