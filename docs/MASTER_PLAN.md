# SpotOn Master Plan

> Blueprint for building a tennis dampener sensor from bench prototype to open-source release.
> Per-phase implementation details: `docs/plans/PHASE-{N}.md`

## Project Timeline

```
Phase 0          Phase 1            Phase 2              Phase 3
Hello Dual       Hello ML +         Hello App +          Polish +
Sensor           Accuracy           Storage              Open Source
(Week 1-2)       (Week 3-6)         (Week 7-10)          (Week 11-14)
────────────────────────────────────────────────────────────────────►
  SPI+FIFO        Data collect       ZMS+BLE+App          Case+Power
  Impact detect   SVR/RF train       Flutter UI           GitHub release
  ZMS validate    Calibration        Court testing        Beta testers
```

## Current Status

- **Phase:** Pre-Phase 0 (project setup complete)
- **Next Action:** Order Phase 0 components, begin firmware scaffold

---

## Phase 0 — Hello Dual Sensor

**Objective:** Prove dual-sensor SPI communication, FIFO reads at 800Hz, impact detection, and ZMS storage on nRF54L15.

**Hardware:** nRF54L15-DK + EV_ICM-42688-P + EVAL-ADXL372Z (~$118) — see [HW_COMPONENTS.md](HW_COMPONENTS.md)

**Key Deliverables:**
- `sensor.c/h` — ICM-42688-P + ADXL372 SPI driver, FIFO watermark INT
- `impact.c/h` — ADXL372 threshold-based impact detection
- Device tree overlay for dual SPI bus
- ZMS read/write validation on RRAM
- Serial log showing 800Hz dual-sensor data + impact events

**Go/Kill Gate:**
- [ ] Dual sensor 800Hz stable (no FIFO overflows)
- [ ] Impact detection triggers reliably on desk taps
- [ ] ZMS writes/reads verified on RRAM
- [ ] TPU damping test: vibration attenuation <=50%

**Key Risks:** R2 (ICM-42688-P Zephyr driver), R4 (TPU damping), R11 (nRF54L15 maturity), R14 (RRAM/ZMS)

**Open Questions to Resolve:** Q1, Q2, Q9, Q11

**Detail:** [`docs/plans/PHASE-0.md`](plans/PHASE-0.md)

---

## Phase 1 — Hello ML + Accuracy

**Objective:** Collect real tennis data, train SVR/RF models, achieve <=5cm impact location accuracy post-calibration.

**Hardware:** + Fanstel BM15A + small IMU breakout + LiPo battery (~$41 additional)

**Depends on Phase 0:** Stable 800Hz sensor pipeline, impact detection working

**Key Deliverables:**
- BLE raw data streaming tool (firmware + Python receiver)
- 2,500+ labeled impact samples (ball machine + camera)
- `features.c/h` — 39-feature extraction pipeline
- `ml.c/h` — emlearn SVR + RF inference wrapper
- `face_angle.c/h` — complementary filter
- `swing_speed.c/h` — gyro peak algorithm
- `calibration.c/h` — 20-shot quick calibration protocol
- Trained SVR + RF models as C headers in `models/`

**Go/Kill Gate:**
- [ ] Post-calibration impact location <=5cm MAE
- [ ] Shot classification >=85% (5-class)
- [ ] Face angle +-5deg vs camera reference
- [ ] Swing speed +-15% vs radar gun
- [ ] ML inference <10ms on nRF54L15

**Key Risks:** R1 (<=4cm not achieved), R3 (cross-racket generalization), R8 (per-stroke offset), R13 (ADXL372 resolution)

**Open Questions to Resolve:** Q3, Q4, Q7, Q10

**Detail:** [`docs/plans/PHASE-1.md`](plans/PHASE-1.md)

---

## Phase 2 — Hello App + Storage

**Objective:** Full end-to-end pipeline: play tennis -> data stored on device -> BLE sync -> see hit-map in Flutter app.

**Hardware:** Same as Phase 1 (custom PCB deferred to Phase 3)

**Depends on Phase 1:** ML models trained, calibration protocol working

**Key Deliverables:**
- `session.c/h` — ZMS session storage with ARMED/ACTIVE state machine
- `ble_svc.c/h` — GATT service (Indicate, DIS, BAS, time sync)
- `power.c/h` — SLEEP/ARMED/ACTIVE transitions + WDT + battery monitoring
- `main.c` — Full state machine + event loop
- Flutter app: BLE sync + SQLite + Riverpod + hit-map Canvas
- Calibration UI in Flutter
- 10 live court test sessions completed

**Go/Kill Gate:**
- [ ] Full session capture -> BLE sync -> app display works
- [ ] ZMS partition handles 500+ shots without corruption
- [ ] BLE sync of 1-hour session <5 seconds
- [ ] Battery lasts 4+ hours continuous
- [ ] No crashes across 10 court sessions

**Key Risks:** R10 (ZMS partition full), R6 (battery form factor)

**Open Questions to Resolve:** Q6

**Detail:** [`docs/plans/PHASE-2.md`](plans/PHASE-2.md)

---

## Phase 3 — Polish + Open Source

**Objective:** Dampener form factor, power optimization, open-source release with documentation.

**Hardware:** Custom PCB 28x11mm + TPU case + pogo charge dock (~$60 for 1pcs)

**Depends on Phase 2:** Stable firmware + working Flutter app

**Key Deliverables:**
- Custom PCB design (KiCad) — all components on 28x11mm board
- 3D printed TPU dampener case + pogo pin charge dock
- SLEEP/ARMED power optimization (target: 4h+ battery on 100mAh)
- CSV/PNG export in Flutter app
- GitHub release: README, BOM, build guide, KiCad files
- 3-5 blog posts
- 5-10 beta testers feedback

**Go/Kill Gate:**
- [ ] Weight <=10g with case
- [ ] 4+ hours battery life measured
- [ ] Survives 100+ serve impacts without failure
- [ ] 5+ beta testers complete full sessions
- [ ] GitHub repo public with complete build guide

**Key Risks:** R5 (dampener falls off), R6 (battery fit), R7 (heat degradation)

**Open Questions to Resolve:** Q5

**Detail:** [`docs/plans/PHASE-3.md`](plans/PHASE-3.md)

---

## Cross-Phase Dependencies

```
Phase 0                    Phase 1                Phase 2              Phase 3
─────────────────────────────────────────────────────────────────────────────
sensor.c ──────────────► features.c
impact.c ──────────────► ml.c (trigger)
DT overlay ────────────► (reused)
ZMS validation ────────────────────────────► session.c
                         calibration.c ────► calibration UI
                         ble streaming ────► ble_svc.c (batch)
                         models/*.h ────────► ml.c (inference)
                                            power.c ─────────────► optimization
                                            Flutter app ─────────► export, polish
```

## Cumulative Budget

| Phase | Added | Total | Order When |
|-------|-------|-------|------------|
| 0 | ~$118 | ~$118 | Now |
| 1 | ~$41 | ~$159 | Phase 0 complete |
| 2 | $0 | ~$159 | — |
| 3 | ~$60 | ~$219 | Phase 2 complete |

## Success Criteria (from TECH_SPEC.md)

| Criterion | Target | Verified In |
|-----------|--------|-------------|
| Impact location accuracy | <=4cm MAE | Phase 1 |
| Stroke classification | >=90% (5-class) | Phase 1 |
| Sweet-spot rate reliability | +-5% vs manual | Phase 2 |
| Swing speed accuracy | +-15% vs radar | Phase 1 |
| Face angle accuracy | +-5deg | Phase 1 |
| Battery life | 4+ hours | Phase 3 |
| Weight | <=10g | Phase 3 |
| BLE sync time | <5 seconds (1h session) | Phase 2 |

## Risk Register Summary

| ID | Risk | Phase | Severity |
|----|------|-------|----------|
| R1 | Impact location <=4cm not achieved | 1 | **Critical** |
| R2 | ICM-42688-P Zephyr driver issues | 0 | **Critical** |
| R3 | Poor cross-racket generalization | 1 | Medium |
| R4 | TPU damping >50% | 0 | **Critical** |
| R5 | Dampener falls off during play | 3 | Medium |
| R11 | nRF54L15 Zephyr maturity | 0 | Medium |
| R14 | RRAM/ZMS compatibility | 0 | **Critical** |

Full risk register: TECH_SPEC.md section 12.

## References

- [TECH_SPEC.md](../TECH_SPEC.md) — Full technical specification (v0.3.0)
- [HW_COMPONENTS.md](HW_COMPONENTS.md) — Hardware BOM per phase
