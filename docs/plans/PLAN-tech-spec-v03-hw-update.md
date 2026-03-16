# PLAN: TECH_SPEC.md v0.3 Hardware Update

**Task:** Update TECH_SPEC.md from v0.2.0 to v0.3.0 with 3 recommended HW component changes from research phase
**Phase:** Pre-Phase 0 (specification update)
**Created:** 2026-03-16

---

## Executive Summary

> **TL;DR:** Update TECH_SPEC.md to reflect 3 component upgrades (nRF54L15, ICM-42688-P, ADXL372) that improve performance, reduce cost, and fix a critical BMI270 aliasing problem -- all with same PCB footprints.

### Key Decisions
- **MCU: nRF52840 -> nRF54L15** -- 2x faster, cheaper ($2.60 vs $4.70), lower power, same 6x6mm QFN48
- **IMU: BMI270 -> ICM-42688-P** -- Fixes critical aliasing issue, 2x lower noise, programmable AAF, same 2.5x3mm LGA-14
- **High-g Accel: ADXL375 -> ADXL372** -- 16x deeper FIFO, 6.4x lower power, official Zephyr driver, same +/-200g range
- **Dev Kit Strategy: TBD** -- nRF52840 DK for Phase 0 (proven) vs nRF54L15 DK from start (future-proof)
- **ODR: TBD** -- Keep 800Hz or increase to 1600Hz (ICM-42688-P supports up to 32kHz)

### Resource Impact
- **NVM:** 1MB Flash -> 1,524KB RRAM (~49% more)
- **RAM:** 256KB (unchanged)
- **Power (ACTIVE):** ~4.31mA -> TBD (mark as preliminary, verify against datasheets)
- **BOM (1pcs):** ~$62 -> ~$60 (-$2)
- **Storage:** FCB+NVS -> **ZMS** (RRAM requires different storage subsystem)

---

## Review Checklist

- [ ] All section cross-references consistent after sensor name changes?
- [ ] Power budget recalculated correctly for all states? (mark as PRELIMINARY)
- [ ] BOM prices updated for both Dev Kit and Custom PCB?
- [ ] Pin assignment updated for nRF54L15 GPIO mapping (31 GPIOs, not 48)?
- [ ] NVM partition layout updated for 1,524KB RRAM?
- [ ] **FCB -> ZMS migration documented? (CRITICAL: RRAM incompatible with FCB/NVS)**
- [ ] Feature engineering table references correct sensor names?
- [ ] ADXL372 resolution trade-off documented (12-bit 100mg/LSB vs 13-bit 49mg/LSB)?
- [ ] ADXL372 must use normal FIFO mode (not peak mode) for feature engineering?
- [ ] ICM-42688-P Zephyr driver known issues documented as risk?
- [ ] Risk register updated with new risks?
- [ ] Open questions updated?
- [ ] References section has new datasheets?
- [ ] v0.3 change summary captures all changes?
- [ ] CLAUDE.md updated with new component names?
- [ ] `.claude/agents/*.md` files updated with new component names?
- [ ] Sections 2.1, 2.2 architecture diagrams updated?
- [ ] Section 8 BLE version updated (5.0 -> 5.4)?

---

## Problem Analysis

### What
Update the technical specification document to reflect 3 hardware component changes identified during the research phase. This is a document-only change -- no code is affected (project is pre-Phase 0).

### Why
1. **BMI270 aliasing bug**: At 800Hz ODR, BMI270's 740Hz LPF causes vibration energy between 400-740Hz to alias into the measurement band, corrupting ML features. ICM-42688-P's programmable AAF fixes this.
2. **Cost reduction**: nRF54L15 is ~$2 cheaper per unit than nRF52840.
3. **Performance**: nRF54L15 is 2x faster, enabling faster ML inference and quicker return to sleep.
4. **ADXL372 advantages**: Official Zephyr driver eliminates custom driver work. 512-sample FIFO reduces interrupt frequency from every 13ms to every 213ms.

### Success Criteria
- [ ] TECH_SPEC.md v0.3.0 is internally consistent (no stale references to old parts)
- [ ] All numerical values (power, BOM, flash layout) are recalculated
- [ ] Change summary clearly explains rationale for each change
- [ ] New risks and open questions are documented
- [ ] Watch list for future components is included

---

## Technical Design

### Sections Requiring Changes

| Section | Change Type | Scope |
|---------|------------|-------|
| Header / v0.x change summary | **New** v0.3 summary block | Major rewrite |
| 3.1 MCU | **Replace** nRF52840 specs with nRF54L15 | Full section rewrite |
| 3.2.1 BMI270 | **Replace** with ICM-42688-P specs | Full section rewrite |
| 3.2.2 ADXL375 | **Replace** with ADXL372 specs | Full section rewrite |
| 3.2 Dual sensor role diagram | **Update** sensor names in ASCII art | Minor edit |
| 3.3 Battery & Power | **Recalculate** power budget table | Numerical update |
| 3.4 Form factor | **Update** height stackup if package heights differ | Minor check |
| 3.5 MVP BOM (Dev Kit) | **Update** breakout board options | Table update |
| 3.6 Custom PCB BOM | **Update** prices | Table update |
| 3.7 Pin Assignment | **Update** for nRF54L15 DK pins | Full rewrite |
| 4.1 RTOS Config | **Update** Kconfig for nRF54L15 | Table update |
| 4.2 Thread Model | **Review** imu_q sizing if ODR changes | Conditional |
| 4.4 Flash Partition | **Redesign** for 1.5MB RRAM | Full rewrite |
| 5.1 Impact Locator | **Update** sensor references | Minor edits |
| 5.1 Feature table | **Update** sensor source column | Table edit |
| 5.6 Memory budget | **Review** (likely unchanged) | Check only |
| 11 Implementation Phases | **Update** Phase 0 tasks for new parts | Table update |
| 12 Risk Register | **Update** risks for new components | Add/modify rows |
| 13 Open Questions | **Update** for new components | Add/modify rows |
| 14 References | **Add** new datasheets | Append rows |
| NEW: Watch List section | **Add** future component candidates | New section |

### Design Decisions

1. **Keep dual-sensor architecture** -- Even though LSM6DSV320X could replace both sensors in one chip, it lacks Zephyr driver support and has lower max range (+/-320g vs dedicated +/-200g + +/-16g with better resolution split).

2. **Keep 800Hz ODR as default** -- Mark 1600Hz as "Phase 1 investigation item" rather than committing now. The power and data volume trade-offs need empirical validation.

3. **Storage: FCB/NVS -> ZMS** -- nRF54L15 uses RRAM, not NOR Flash. RRAM has different write/erase semantics (~10,000 cycle endurance, no erase-before-write). Nordic recommends ZMS (Zephyr Memory Storage) over FCB/NVS for nRF54L-series. All storage architecture must be redesigned around ZMS.

4. **Dev Kit strategy: document both options** -- nRF52840 DK (proven, available now) as primary, nRF54L15 DK as alternative. Note that firmware is portable between the two via Zephyr board abstraction.

5. **ADXL372 resolution trade-off accepted** -- 12-bit (100mg/LSB) vs ADXL375's 13-bit (49mg/LSB). For impact detection threshold and peak amplitude features, 100mg/LSB is sufficient. Document as known trade-off.

6. **ADXL372 must use normal FIFO mode** -- Peak detection mode only stores max values, losing time-domain waveform needed for features #2 (impulse duration), #3 (impulse energy), #14 (time-to-peak). Document that peak mode is for future power optimization only.

---

## Implementation Plan

### Phase 1: Header & Change Summary
- [ ] `TECH_SPEC.md` line 1: Update version to v0.3.0
- [ ] Lines 6-19: Add v0.3 change summary block (similar format to v0.2 block)
  - MCU: nRF52840 -> nRF54L15
  - IMU: BMI270 -> ICM-42688-P (aliasing fix)
  - High-g: ADXL375 -> ADXL372 (FIFO, power, Zephyr driver)
  - Battery protection: PORON foam + PU conformal coat
  - NVM: 1MB Flash -> 1,524KB RRAM
  - Storage: FCB+NVS -> ZMS (RRAM-compatible)
  - Power: ACTIVE reduced (preliminary, pending datasheet verification)
  - BOM: -$2 per unit

### Phase 2: Section 3 — Hardware Specification
- [ ] 3.1 MCU: Full rewrite with nRF54L15 specs (Cortex-M33 128MHz, 1.5MB RRAM, 256KB RAM, BLE 5.4, 6x6mm QFN48, RISC-V coprocessor)
  - Include v0.3 change rationale box
  - Update spec table with all nRF54L15 parameters
- [ ] 3.2.1: Full rewrite as "ICM-42688-P (Vibration Analysis + Gyro)"
  - Programmable AAF (critical feature)
  - 70 ug/sqrt(Hz) noise
  - 20-bit FIFO resolution
  - 880uA power
  - 20,000g shock
  - Include v0.3 change rationale box explaining aliasing problem
- [ ] 3.2.2: Full rewrite as "ADXL372 (High-g Impact Capture)"
  - 512-sample FIFO
  - 22uA power
  - 4-pole selectable AAF
  - Peak detection mode
  - Instant-on 1.4uA
  - Official Zephyr driver
  - Include v0.3 change rationale box
- [ ] 3.2 Dual sensor role diagram: Replace "BMI270" with "ICM-42688-P" and "ADXL375" with "ADXL372" in ASCII art
- [ ] 3.1 MCU: Update GPIO count to 31 (was 48). Include pin budget table showing all required signals fit.
- [ ] 3.1 MCU: Note VDD range 1.7-3.6V (no VDDH). nPM1100 1.8V output is compatible -- no design change needed.
- [ ] 3.3 Power budget: Recalculate all 4 modes -- **MARK ALL VALUES AS PRELIMINARY**
  - ACTIVE: nRF54L15 TBD + ICM-42688-P 880uA + ADXL372 22uA + HFXO TBD (verify from nRF54L15 datasheet electrical specs table)
  - ARMED: nRF54L15 TBD + ICM-42688-P WOM ~14-20uA (verify from DS-000347) + ADXL372 instant-on 1.4uA
  - SLEEP: nRF54L15 ~0.8uA (Global RTC) + ICM-42688-P WOM ~14-20uA
  - SYNC: nRF54L15 BLE TX ~4.8mA @0dBm (verify)
  - Note: All nRF54L15 currents pending datasheet verification
  - Recalculate battery life table with preliminary values
- [ ] 3.3 Battery: Add "Mechanical Protection (v0.3)" subsection
  - PORON closed-cell foam (0.5mm) around battery
  - Polyurethane conformal coating
  - Strain relief on battery tab solder joints
- [ ] 3.2.2 ADXL372: Document resolution trade-off (12-bit 100mg/LSB vs ADXL375 13-bit 49mg/LSB)
- [ ] 3.2.2 ADXL372: Note must use normal FIFO mode (not peak mode) for feature engineering
- [ ] 3.2.2 ADXL372: Package is 3x3.25mm (not 3x3mm like ADXL375 -- 0.25mm longer footprint)
- [ ] 3.4 Form factor: Check height stackup -- VERIFY AGAINST DATASHEETS
  - ICM-42688-P: 0.91mm (vs BMI270 0.83mm) -- +0.08mm
  - ADXL372: 1.06mm (vs ADXL375 -- verify ADXL375 actual height from datasheet, TECH_SPEC says 1.45mm but may be wrong)
  - Recalculate net stackup change after verification
  - Note ADXL372 has smaller footprint: 3x3.25mm vs ADXL375 3x5mm (-5.25mm2 PCB area savings)
  - Update total height and weight budget
- [ ] 3.5 MVP BOM: Update dev kit parts
  - nRF52840 DK stays (or add nRF54L15 DK option)
  - ICM-42688-P breakout (SparkFun/Adafruit)
  - ADXL372 breakout (Adafruit ADXL372 or similar)
- [ ] 3.6 Custom PCB BOM: Update prices
  - nRF54L15 QFAA: ~$3-4 (1pcs), ~$2.60 (100pcs)
  - ICM-42688-P: ~$4 (1pcs), ~$2 (100pcs)
  - ADXL372: ~$4 (1pcs), ~$3 (100pcs)
- [ ] 3.7 Pin Assignment: Update for nRF54L15 DK
  - Check nRF54L15 DK pin mapping (different from nRF52840 DK)
  - SPI, CS, INT pins need new assignments
  - ADC pins for NTC and VBAT

### Phase 3: Section 4 — Software Architecture
- [ ] 4.1 RTOS Config: Update board target
  - `nrf54l15dk/nrf54l15/cpuapp` (or keep nrf52840dk as primary dev target)
  - Replace `CONFIG_FCB=y` with `CONFIG_ZMS=y`
  - Replace `CONFIG_NVS=y` with `CONFIG_ZMS=y` (ZMS replaces both)
  - Replace `CONFIG_BMI270=y` with ICM-42688-P Kconfig (`CONFIG_ICM42688=y`)
  - Add CONFIG notes specific to nRF54L15
- [ ] 4.2 Thread Model: Review imu_q slot size
  - If ODR stays 800Hz: slot size unchanged
  - ICM-42688-P 20-bit data: if unpacked to 32-bit per axis, each 6-axis sample = 24B (vs 12B at 16-bit). 120 samples x (24B + 6B ADXL372) = 3,600B -- exceeds current 2,200B slot. FLAG for Phase 0 verification.
  - Alternative: use ICM-42688-P in 16-bit mode (same slot size, lose 20-bit precision)
- [ ] **4.4 RRAM Partition: MAJOR REDESIGN (CRITICAL)**
  - **FCB -> ZMS migration**: RRAM has no erase-before-write semantics. FCB was designed for NOR Flash. Nordic recommends ZMS for nRF54L-series.
  - **NVS -> ZMS migration**: Same issue. ZMS replaces both FCB and NVS.
  - Redesign partition layout for 1,524KB RRAM:
    - MCUboot: 48KB
    - Slot 0: 500KB (application image)
    - Slot 1: 500KB (OTA staging)
    - ZMS (config/calibration): 16KB (replaces NVS)
    - ZMS (shot events): 452KB (replaces FCB)
    - Scratch: 8KB
    - Total: 1,524KB (matches nRF54L15 RRAM capacity exactly)
  - Update capacity: 452KB / 20B = ~23,100 shots = ~46 hours @500 shots/h (was 9,800 shots = ~16h)
  - Add RRAM endurance analysis: ~10,000 write cycles per cell. At 500 shots/h, 4h/week: ~100K shots/year. 452KB/4KB pages = 113 pages. Each page rewritten every ~205 shots = ~487 rewrites/year. 10,000/487 = **~20 year endurance** (adequate)
  - Verify MCUboot scratch partition works with RRAM (no erase needed)
  - Add note about RRAM write latency: 22us sequential (vs flash page erase ~85ms + write)

### Phase 4: Section 5 — ML Pipeline
- [ ] 5.1 Feature table: Replace "BMI270" with "ICM-42688-P", "ADXL375" with "ADXL372" in source column
- [ ] 5.1 Feature #9: Update note about AAF
  - ICM-42688-P programmable AAF means cleaner frequency analysis
  - Goertzel still appropriate but may benefit from higher SNR
- [ ] 5.6 Memory budget: Review (likely unchanged, same data structures)
- [ ] Add note about 1600Hz ODR as Phase 1 investigation item

### Phase 5: Sections 2, 4.3, 8, 11-14 — Architecture, State Machine, BLE, Phases, Risks
- [ ] 2.1 High-Level Architecture: Update ASCII diagram
  - "nRF52840 (64MHz Cortex-M4F, 1MB Flash, 256KB RAM)" -> nRF54L15 specs
  - "BMI270 (SPI, 800Hz) + ADXL375 (SPI, 800Hz)" -> ICM-42688-P + ADXL372
  - "BLE 5.0" -> "BLE 5.4"
- [ ] 2.2 Data Flow: Update sensor references
  - "BMI270 800Hz" -> "ICM-42688-P 800Hz"
  - "ADXL375 800Hz" -> "ADXL372 800Hz"
  - Update burst read byte counts for ICM-42688-P FIFO format
- [ ] 4.3 State Machine: Update sensor-specific references
  - "BMI270 any-motion INT2" -> "ICM-42688-P WOM (Wake-on-Motion)"
  - "BMI270 100Hz low-power" -> "ICM-42688-P low-power mode"
  - "ADXL375 100Hz low-power" -> "ADXL372 instant-on (1.4uA)"
  - "ADXL375 >30g" -> "ADXL372 >30g"
- [ ] 8 BLE Protocol: Update "BLE 5.0" references to "BLE 5.4" (backward compatible, no protocol changes needed)
- [ ] 11 Implementation Phases: Update Phase 0 tasks
  - "ICM-42688-P SPI 800Hz + FIFO watermark INT" (was BMI270)
  - "ADXL372 SPI 800Hz + FIFO" (was ADXL375)
  - Remove "ADXL375 Zephyr driver" risk (ADXL372 has upstream driver)
  - Add: "Verify ICM-42688-P Zephyr driver on nRF54L15 (known issues: #84833)"
  - Add: "Verify ZMS storage on RRAM works correctly"
- [ ] 12 Risk Register: Update/add risks
  - R2: Update -> "ICM-42688-P Zephyr driver has known issues (build failures, incorrect values in Zephyr #84833, #60549). Must validate in Phase 0." (prob: med, impact: high)
  - R4: Update sensor names in TPU damping risk
  - NEW R11: "nRF54L15 Zephyr maturity" (prob: low, impact: med)
  - NEW R12: "ADXL372 shock survivability" (prob: low, impact: med) — verify with ADI FAE
  - NEW R13: "ADXL372 resolution (100mg/LSB) insufficient for ML features" (prob: low, impact: med)
  - NEW R14: "RRAM endurance or ZMS compatibility issue" (prob: low, impact: high) — verify in Phase 0
  - NEW R15: "ICM-42688-P 20-bit FIFO format changes imu_q slot size" (prob: med, impact: low)
- [ ] 13 Open Questions: Update
  - Q1: Update -> "ICM-42688-P Zephyr driver stability on nRF54L15?"
  - Q2: Update -> "ICM-42688-P + ADXL372 FIFO synchronization?"
  - NEW Q7: "1600Hz vs 800Hz ODR trade-off for vibration capture?"
  - NEW Q8: "nRF54L15 DK vs nRF52840 DK for Phase 0?"
  - NEW Q9: "ICM-42688-P 16-bit vs 20-bit FIFO mode — precision vs buffer size?"
  - NEW Q10: "Does ADXL372 100mg/LSB resolution provide sufficient discrimination for ML features?"
  - NEW Q11: "ZMS API differences from FCB/NVS — impact on session.c and calibration.c?"
- [ ] 14 References: Add new entries
  - ICM-42688-P datasheet (DS-000347-ICM-42688-P-v1.7)
  - ADXL372 datasheet (Analog Devices)
  - nRF54L15 product spec
  - SmartDampener paper (ACM IMWUT 2024, Liu et al.)
  - Chen et al. eigenfrequency ratio paper (MDPI Sensors 2022)
  - Zephyr ZMS documentation
- [ ] NEW Section 15 (or 14.5): Component Watch List
  - LSM6DSV320X: single-chip replacement, monitor Zephyr driver
  - ICM-45605: best-in-class power/FIFO, monitor Zephyr driver
  - nPM1300: fuel gauge, monitor for nRF54 migration
  - BMI570: Bosch next-gen, HVM Q3 2026

### Phase 6: Final Consistency Pass (TECH_SPEC.md)
- [ ] Global find-replace: "nRF52840" references that should become "nRF54L15"
  - Keep nRF52840 references in change history and DK BOM (if using nRF52840 DK)
- [ ] Global find-replace: "BMI270" -> "ICM-42688-P" (except in change history)
- [ ] Global find-replace: "ADXL375" -> "ADXL372" (except in change history)
- [ ] Global find-replace: "FCB" -> "ZMS" (except in change history)
- [ ] Global find-replace: "NVS" -> "ZMS" for storage references (keep NVS in change history)
- [ ] Section 4.5 Source Tree: Update sensor comments ("BMI270 + ADXL375" -> "ICM-42688-P + ADXL372")
- [ ] Section 7.1 shot_event: Update comment "ADXL375" -> "ADXL372"
- [ ] Verify all ASCII art diagrams updated
- [ ] Update last-updated date and version footer
- [ ] Verify line count hasn't ballooned excessively

### Phase 7: Update CLAUDE.md and Agent Files
- [ ] **CLAUDE.md** (project root): Update all hardware references
  - Architecture diagram: nRF52840 -> nRF54L15, BMI270 -> ICM-42688-P, ADXL375 -> ADXL372
  - Source Tree section: Update sensor file comments
  - Development Commands: Add note about board target change if using nRF54L15 DK
  - "Adding a New Sensor Reading" pattern: Update sensor references
- [ ] **`.claude/agents/`**: Update all agent definition files with new component names
  - `zephyr-firmware.md` (~6 refs)
  - `hardware-reviewer.md` (~9 refs)
  - `code-reviewer.md` (~2 refs)
  - `concurrency-reviewer.md` (~2 refs)
  - `ble-contract-reviewer.md` (~1 ref)
  - `flutter-app.md` (~2 refs)
- [ ] **`.claude/commands/`**: Update command files
  - `review.md`, `test.md`, `research.md`, `myplan.md` — update sensor references
- [ ] **Auto-memory** (`MEMORY.md`): Update "nRF52840 + BMI270 + ADXL375" in Project Type

---

## Testing Strategy

### Document Validation
- Read through entire updated TECH_SPEC.md for internal consistency
- Verify all numerical calculations (power budget, battery life, ZMS capacity, RRAM endurance)
- Check that old sensor names don't appear outside of change history sections
- Grep for "FCB", "NVS", "BMI270", "ADXL375", "nRF52840", "BLE 5.0" to catch stale references

### Cross-Reference Check
- CLAUDE.md: grep for old component names
- All `.claude/agents/*.md` and `.claude/commands/*.md`: grep for old component names
- `MEMORY.md`: verify updated

---

## Risks & Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| FCB/NVS incompatible with RRAM | **High** | Migrate to ZMS in spec; verify ZMS Zephyr support on nRF54L15 in Phase 0 |
| ICM-42688-P Zephyr driver bugs (#84833) | High | Phase 0 validation gate; fallback to BMI270 if critical |
| nRF54L15 DK pin mapping differs from spec | Low | Document both DK options; Zephyr device tree abstracts pins |
| ADXL372 resolution (100mg/LSB) insufficient for ML | Med | Document trade-off; evaluate during Phase 1 data collection |
| Power budget estimates unverified | Med | Mark as PRELIMINARY; validate against datasheets before Phase 0 |
| Inconsistent sensor references after edit | Med | Phase 6+7 consistency pass with global grep |
| Stale agent files cause wrong code generation | High | Phase 7 explicitly updates all `.claude/` files |
| ADXL375 height in TECH_SPEC may be wrong (1.45mm) | Low | Verify against ADXL375 datasheet before updating stackup |

---

## Plan Validation (3-Agent Consensus)

**Method:** 2/3 consensus filtering across 3 independent validators
**Overall:** APPROVED WITH REVISIONS (1 critical finding addressed)

### Critical (3/3 consensus — ADDRESSED)
1. **FCB/NVS -> ZMS migration**: RRAM has different write/erase semantics from NOR Flash. Nordic recommends ZMS for nRF54L-series. Plan updated to redesign storage architecture around ZMS.

### Warnings (2/3+ consensus — added to review checklist)
2. **CLAUDE.md + agent files need updating** (2/3): 7+ refs in CLAUDE.md, 25+ refs across 8 agent/command files. Added Phase 7.
3. **Missing Sections 2.1, 2.2** (2/3): Architecture diagrams have hardcoded sensor names. Added to Phase 5.
4. **BLE version update** (2/3): Section 8 and diagrams say "BLE 5.0". Added to Phase 5.
5. **ADXL372 resolution trade-off** (2/3): 12-bit 100mg/LSB vs 13-bit 49mg/LSB. Added to design decisions and review checklist.
6. **ICM-42688-P Zephyr driver issues** (2/3): Known bugs in Zephyr #84833, #60549. Added to risk register.
7. **Power budget unverified** (3/3): All nRF54L15 and ICM-42688-P WOM currents need datasheet verification. Marked as PRELIMINARY.
8. **GPIO count 31 not 48** (2/3): Need pin budget verification. Added to Phase 2.
9. **ADXL372 must use normal FIFO mode** (2/3): Peak mode breaks time-domain feature extraction. Added to design decisions.

### Suggestions (2/3 consensus — documented)
10. **SPI bus speed compatibility** (2/3): Shared bus limited to slowest device (ADXL372). Document in spec.
11. **RRAM write speed** (2/3): 22us sequential write. Not a bottleneck but document for completeness.

### Factual Corrections (1/3 but likely valid — flagged for verification)
12. **ADXL375 height**: TECH_SPEC says 1.45mm, validator says actual datasheet is 1.0mm. Must verify before updating stackup.
13. **nRF54L15 no VDDH**: Not an issue for SpotOn (nPM1100 provides 1.8V), but document VDD range in MCU spec table.
