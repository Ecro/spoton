# Phase 1 — Hello ML + Accuracy

**Objective:** Collect real tennis impact data, train SVR/RF models, achieve <=5cm impact location and >=85% shot classification on-device.
**Timeline:** Week 3-6
**Hardware:** Phase 0 kit + Fanstel BM15A + small IMU breakout + LiPo (~$41 additional)
**Depends on:** Phase 0 complete (stable 800Hz dual sensor + impact detection + ZMS + FIFO sync <2 samples)
**Created:** 2026-03-18

---

## Go/Kill Gate

- [ ] Post-calibration impact location <=5cm MAE (target: <=4cm)
- [ ] Shot classification >=85% (5-class: FH/BH/SV/VL/SL, target: >=90%)
- [ ] Face angle +-5deg vs high-speed camera reference
- [ ] Swing speed +-15% vs radar gun (50 serves)
- [ ] ML inference <10ms total on nRF54L15
- [ ] Calibration protocol (20 shots) reduces MAE by >=30%
- [ ] ADXL372 100mg/LSB resolution evaluated — ML features viable or mitigation identified

**Kill criteria:** If impact location >8cm MAE after calibration and data augmentation, consider NN fallback (R1). If ADXL372 resolution degrades ML significantly (R13), evaluate ADXL375 swap.

---

## Phase 1 Architecture

> **CRITICAL DESIGN NOTE (from 3-agent validation):**
> The TECH_SPEC defines different pre-impact windows per consumer:
> - Shot classifier: **250ms** pre-impact (200 samples, ICM gyro)
> - Swing speed: **100ms** pre-impact (80 samples, ICM gyro)
> - Face angle: **50ms** pre-impact (40 samples, ICM accel+gyro)
> - Impact locator (SVR): **10ms** pre + **140ms** post (ICM + ADXL)
>
> The impact_window struct must capture the **maximum** (250ms pre + 140ms post).
> sensor_thread maintains a **rolling circular buffer** of ICM data and snapshots
> on impact detection.

```
Phase 1 adds ml_thread and data collection pipeline:

sensor_thread (P2)                                     Python (PC)
  ├─ ICM-42688-P FIFO read ──┐                         ├─ collect_data.py
  │  (rolling 250ms ring buf) │                         │    BLE → raw CSV
  ├─ ADXL372 FIFO read ──────┤                         ├─ label_impacts.py
  └─ Impact detection ────────┤                         │    video → (x,y) labels
     (snapshot ring buf)      ▼                         ├─ train_impact_locator.py
                         imu_q (K_MSGQ)                 │    SVR → emlearn → .h
                         4 slots × ~4.5KB               ├─ train_shot_classifier.py
                              │                         │    RF → emlearn → .h
                              ▼                         └─ evaluate.py
                      ┌─ ml_thread (P5) ─┐                   cross-validation
                      │  features.c      │
                      │  ml.c (inference)│
                      │  face_angle.c    │
                      │  swing_speed.c   │
                      │  calibration.c   │
                      └───────┬──────────┘
                              │ shot_q (K_MSGQ)
                              ▼ 32 slots × 20B
                       [Phase 2: session_thread]

Data Collection Mode (Step 1):
  sensor_thread → BLE raw streaming → PC → CSV
  (bypass ml_thread, stream full 390ms window over BLE for training)
```

---

## Step 1: BLE Raw Data Streaming Tool

Stream raw sensor data over BLE to PC for training data collection.

**Firmware files:**
- `src/ble_stream.c/.h` — minimal BLE peripheral, raw data notify
- `src/main.c` — add DATA_COLLECTION mode (bypass ML, stream raw)
- `prj.conf` — add BLE config

**Python files:**
- `tools/collect_data.py` — BLE central, receives raw data → CSV

**Design:**
```
DATA_COLLECTION mode:
  sensor_thread detects impact → snapshots full window:
    ICM: 250ms pre + 140ms post = 312 samples × 6 axes (int16_t) = 3,744B
    ADXL: 150ms window (-10ms to +140ms) = 120 samples × 3 axes = 720B
  Total per impact: ~4,464 bytes raw

BLE transfer per impact:
  - MTU 247 → ~234B payload per indication (use indications for data integrity)
  - 4,464B / 234B = ~20 indications per impact
  - At 7.5ms CI with indications: ~150-200ms per impact (realistic estimate)
  - Packet format: [impact_id:u16][seq:u16][type:u8][data:229B]
    type: 0x01=ICM_PRE, 0x02=ICM_POST, 0x03=ADXL, 0xFF=END(crc16:u16)

  Flow control:
  - Use BLE Indications (ACK'd) instead of Notifications for training data
  - Per-impact CRC16 in END packet — receiver verifies data integrity
  - Ring buffer (16 impact slots) between sensor_thread and BLE for backpressure
  - collect_data.py sends NACK if CRC mismatch → firmware retransmits

collect_data.py:
  - Connects to SpotOn device, negotiates 7.5ms CI + 2M PHY
  - Receives indications → reassembles impact windows
  - Verifies per-impact CRC16
  - Saves to CSV: impact_id, timestamp, icm_ax..gz, adxl_ax..az (per sample)
  - Marks impact boundaries
```

**prj.conf additions:**
```
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_CTLR_PHY_2M=y
CONFIG_BT_L2CAP_TX_MTU=247
CONFIG_BT_BUF_ACL_TX_SIZE=251
CONFIG_BT_CTLR_SDC_TX_PACKET_COUNT=4
CONFIG_BT_GAP_AUTO_UPDATE_CONN_PARAMS=y
```

**Verification:**
- Connect from PC, tap desk, verify CSV output matches serial log data
- Verify CRC16 check catches injected corruption
- Measure BLE throughput: should handle 1 impact every ~200ms

**Note:** This is a temporary streaming mode for data collection only. The production BLE service (Phase 2) uses batch transfer of shot_events, not raw data.

---

## Step 2: Data Collection Campaign

Collect labeled impact samples using **stencil ink method** (SmartDampener 논문 검증 방식).
점진적 수집: 300발 → 평가 → 필요시 추가 (최대 ~1,000발).

**This is a manual/physical step, not a coding step.**

### 라벨링 방법: 스텐실 잉크 (Stencil Ink)

> SmartDampener (Penn State, 2024)가 3,328발로 3.03cm 정확도 달성한 방법.
> 240fps 비디오 방식 대비 2배 빠르고 장비 간단.

```
준비물:
  - 테니스 스텐실 잉크 (~$5, Amazon/쿠팡)
  - 흰색 또는 밝은 색 현 (잉크 자국이 잘 보여야 함)
  - 스마트폰 (일반 사진 — 슬로모션 불필요!)
  - 젖은 수건 (현 닦기용)
  - 볼머신 또는 벽 연습

프로토콜 (1발당 ~10초):
  1. 공에 스텐실 잉크 뿌림 (한 면)
  2. 라켓으로 공 치기 → BLE로 센서 데이터 자동 저장
  3. 라켓 면 사진 촬영 (잉크 자국 = ground truth)
  4. label_impacts.py에서 자국 위치 클릭 → (x, y) 기록
  5. 젖은 수건으로 현 닦기 (겹침 방지)
  6. 50발마다 새 공으로 교체 (잉크 무게 증가 방지)

라벨 정확도: ~0.5cm (사진에서 잉크 자국 중심 클릭)
```

### 점진적 수집 전략 (노동 최소화)

```
Round 1: 잉크 300발 (1일)
  - 라켓 면 전체에 고르게 분포하도록 조준
  - 볼머신 또는 벽 연습으로 다양한 위치에 타구
  - 각 타구: shot_type도 기록 (FH/BH/SV/VL/SL)
  → SVR 모델 첫 훈련 → MAE 확인

  결과에 따라 분기:
    MAE ≤ 4cm → 충분! Round 2 생략 가능
    MAE 4~6cm → Round 2 진행 (300발 추가)
    MAE > 6cm → 특징/파라미터 분석 후 Round 2

Round 2: 잉크 300발 추가 (1일, 필요시)
  - Round 1에서 MAE가 높았던 영역에 집중 수집
  - 가장자리, 프레임 근처 등 부족한 영역 보강
  → 총 600발로 재훈련 → MAE 재확인

Round 3: Pseudo-labeling 확장 (선택)
  - Round 1+2 모델로 실전 플레이 데이터에 의사 라벨 부여
  - 잉크 없이 자유롭게 플레이 → BLE로 센서 데이터 수집
  - 모델 추정값이 confident한 것만 훈련 데이터로 추가
  - 300~500발 자동 확장 (수동 라벨링 0)

예상 총 수동 라벨링: 300~600발 (기존 2,500발 대비 76~88% 감소)
```

**Python files:**
- `tools/label_impacts.py` — 라켓 면 사진에서 잉크 자국 클릭 → (x, y) 라벨링

**Output:**
- `data/raw/round{1,2}/impacts.csv` — raw sensor data
- `data/labels/impacts.csv` — (impact_id, x_cm, y_cm, shot_type, racket_id)
- `data/photos/impact_{id}.jpg` — 잉크 자국 사진 (ground truth 보관)

**샷 분류 데이터 (별도 수집):**
```
각 샷 종류별 최소 50발:
  - 포핸드 (FH): 100발
  - 백핸드 (BH): 100발
  - 서브 (SV): 50발
  - 발리 (VL): 50발
  - 슬라이스 (SL): 50발
  → 잉크 불필요 (샷 종류만 기록하면 됨)
  → 위치 수집과 동시 진행 가능 (잉크 + 샷 종류 기록)
```

**Also collect during this step:**
- 50+ serves with radar gun readings → swing speed validation dataset
- 비디오 촬영은 면 각도 검증용으로만 소량 (20발 정도)

**Open question Q7:** If time permits, collect a subset at 1600Hz ODR for comparison with 800Hz.

---

## Step 3: Feature Engineering

Implement the 39-feature extraction pipeline in firmware.

**Files:**
- `src/features.h` — feature extraction API
- `src/features.c` — all 39 features
- `src/config.h` — feature constants (window sizes, thresholds)

**Feature list (from TECH_SPEC §5.1):**

| # | Feature | Source | Dims | Implementation |
|---|---------|--------|------|----------------|
| 1 | Peak amplitude (per axis) | ADXL372 | 3 | max(abs(ax)), max(abs(ay)), max(abs(az)) |
| 2 | Impulse duration | ADXL372 | 1 | Count samples > 30g threshold |
| 3 | Impulse energy | ADXL372 | 1 | sum(ax^2 + ay^2 + az^2) over impulse |
| 4 | Post-impact decay rate | ICM accel | 3 | Log-linear fit: slope of ln(\|signal\|) vs time |
| 5 | Zero-crossing count | ICM accel | 3 | Count sign changes in 140ms window |
| 6 | Post-impact RMS | ICM accel | 3 | sqrt(mean(x^2)) per axis |
| 7 | Envelope energy ratio (1st/2nd half) | ICM accel | 3 | Energy in 0-70ms / 70-140ms |
| 8 | Inter-axis correlation | ICM accel | 3 | Pearson(XY), Pearson(XZ), Pearson(YZ) |
| 9 | Dominant frequency (Goertzel) | ICM accel | 6 | Energy at 146Hz + 200Hz, per XY |
| 10 | Gyro magnitude at impact | ICM gyro | 1 | sqrt(gx^2+gy^2+gz^2) at t=0 |
| 11 | Gyro axis ratios | ICM gyro | 3 | gx/|g|, gy/|g|, gz/|g| |
| 12 | Peak axis ratios | ADXL372 | 3 | ax_peak/|a|, ay_peak/|a|, az_peak/|a| |
| 13 | Combined magnitude | ADXL372 | 1 | sqrt(ax^2+ay^2+az^2) at peak |
| 14 | Time-to-peak (per axis) | ADXL372 | 3 | Index of max value × dt |
| 15 | Decay time constant (tau) | ICM accel | 2 | Log-linear slope, X and Y only |
| | **Total** | | **39** | |

**Shot classifier features (30 features, from 250ms pre-impact ICM gyro+accel):**

| # | Feature | Source | Dims |
|---|---------|--------|------|
| S1-S6 | Mean per axis | ICM accel+gyro (pre) | 6 |
| S7-S12 | Std dev per axis | ICM accel+gyro (pre) | 6 |
| S13-S18 | Max per axis | ICM accel+gyro (pre) | 6 |
| S19-S24 | Min per axis | ICM accel+gyro (pre) | 6 |
| S25-S27 | Accel magnitude: mean, max, std | ICM accel (pre) | 3 |
| S28-S30 | Gyro magnitude: mean, max, std | ICM gyro (pre) | 3 |
| | **Total** | | **30** |

**Design:**
```c
// features.h
#define IMPACT_FEATURE_COUNT 39
#define SHOT_FEATURE_COUNT   30

struct impact_window {
    /* Pre-impact: 250ms = 200 samples (covers shot classifier, swing speed, face angle) */
    int16_t pre_icm[200][6];     /* ICM accel(3)+gyro(3), 200 samples, 2400B */

    /* Post-impact: 140ms = 112 samples (covers SVR features, decay analysis) */
    int16_t post_icm[112][6];    /* ICM accel(3)+gyro(3), 112 samples, 1344B */

    /* ADXL372: 150ms window (-10ms to +140ms) = 120 samples */
    int16_t adxl[120][3];        /* ADXL accel XYZ, 120 samples, 720B */

    uint32_t timestamp_ms;
    uint8_t  pre_count;          /* Actual pre-impact samples (may be <200 for first impact) */
};
/* Total: ~4,470B per slot */

/* Extract 39 impact locator features (post-impact ICM + ADXL) */
int features_extract_impact(const struct impact_window *win, float features[IMPACT_FEATURE_COUNT]);

/* Extract 30 shot classifier features (pre-impact ICM) */
int features_extract_shot(const struct impact_window *win, float features[SHOT_FEATURE_COUNT]);
```

**Note on decay features (#4, #15):** Use log-linear regression instead of nonlinear
exponential fitting. Take `ln(|signal|)` and fit a line via simple linear regression —
slope gives `-1/tau`. This is O(N) with no iterative solver, suitable for MCU.
Benchmark timing in Step 3 before depending on the 1ms estimate.

**Note on Goertzel frequencies (#9):** The 146Hz and 200Hz values are from academic
literature on specific rackets. Different racket models may shift these frequencies.
During data collection (Step 2), do spectral analysis per racket to validate. If
frequencies vary >10%, consider bandpass energy features instead of single-frequency Goertzel.

**Goertzel algorithm (Feature #9):**
```c
/* O(N) single-frequency energy extraction — no FFT needed */
float goertzel_energy(const int16_t *samples, int n, float target_freq, float sample_rate);
/* Called for 146Hz (1st bending) and 200Hz (frame mode) per axis */
```

**Resource impact:**
- Flash: ~4KB (39 impact features + 30 shot features + Goertzel)
- RAM: see Step 5 resource table (impact_window ~4,470B per imu_q slot)
- Feature buffers: 39 × 4B + 30 × 4B = 276B

**Verification:**
- Extract features from 10 known impacts (saved CSV), compare firmware output vs Python reference implementation
- All 39 features produce non-zero, non-NaN values
- Goertzel energy at 146Hz/200Hz matches Python scipy.signal reference

---

## Step 4: ML Model Training + emlearn Conversion

Train SVR and RF models in Python, convert to C headers via emlearn.

**Python files:**
- `tools/train_impact_locator.py` — SVR training + emlearn export
- `tools/train_shot_classifier.py` — RF training + emlearn export
- `tools/evaluate.py` — cross-validation, confusion matrix, MAE reports

**Output files:**
- `models/impact_locator_svr.h` — emlearn SVR C header (X model)
- `models/impact_locator_svr_y.h` — emlearn SVR C header (Y model)
- `models/shot_classifier_rf.h` — emlearn RF C header
- `models/README.md` — training metrics, hyperparameters, dataset summary

**SVR Training (Impact Locator):**
```python
# Separate SVR for X and Y coordinates
svr_x = SVR(kernel='rbf', C=best_C, gamma='scale', epsilon=0.5)
svr_y = SVR(kernel='rbf', C=best_C, gamma='scale', epsilon=0.5)

# Cross-validation: 5-fold, stratified by zone
# Hyperparameter search: C in [1, 10, 50, 100]
# Metric: MAE in cm

# emlearn conversion
import emlearn
svr_x_model = emlearn.convert(svr_x)
svr_x_model.save(file='models/impact_locator_svr.h', name='impact_svr_x')
```

**RF Training (Shot Classifier):**
```python
# 5-class: FH=0, BH=1, SV=2, VL=3, SL=4
rf = RandomForestClassifier(n_estimators=20, max_depth=8)
# Input: 30 statistical features from 250ms pre-impact gyro
# Cross-validation: 5-fold
# Metric: accuracy, confusion matrix

rf_model = emlearn.convert(rf)
rf_model.save(file='models/shot_classifier_rf.h', name='shot_classifier')
```

**Evaluation criteria:**
| Metric | Target | Kill |
|--------|--------|------|
| Impact X MAE | <=4cm | >8cm |
| Impact Y MAE | <=4cm | >8cm |
| Combined MAE | <=4cm | >8cm |
| Shot classification accuracy | >=90% | <75% |
| Per-class F1 (each stroke) | >=80% | <60% |

**ADXL372 resolution evaluation (Q10):**
During training, systematically test:
1. Train with all 39 features → baseline MAE
2. Remove ADXL372-derived features (#1,2,3,12,13,14) → MAE without ADXL
3. Quantize ADXL372 features to 100mg resolution → MAE with quantized
4. If quantized MAE > baseline + 1cm: ADXL372 resolution is a problem, consider mitigations

---

## Step 5: ML Inference Integration

Integrate emlearn models into Zephyr firmware with ml_thread.

**Files:**
- `src/ml.h` — ML inference API
- `src/ml.c` — emlearn wrapper, imu_q consumer
- `src/main.c` — create ml_thread
- `prj.conf` — add `CONFIG_FPU=y`
- `models/*.h` — included by ml.c

**Design:**
```
ml_thread (Priority 5, Stack 4KB):
  while(1) {
    k_msgq_get(&imu_q, &window, K_FOREVER);

    // 1. Extract 39 impact features (post-impact ICM + ADXL)
    features_extract_impact(&window, impact_feat_buf);

    // 2. Extract 30 shot features (pre-impact ICM)
    features_extract_shot(&window, shot_feat_buf);

    // 3. Impact location (SVR × 2)
    float raw_x = emlearn_predict(&impact_svr_x, impact_feat_buf);
    float raw_y = emlearn_predict(&impact_svr_y, impact_feat_buf);

    // 4. Apply calibration offset + scale
    float adj_x = (raw_x + cal.offset_x_mm) * (cal.scale_x_pct / 100.0f);
    float adj_y = (raw_y + cal.offset_y_mm) * (cal.scale_y_pct / 100.0f);

    // 5. Shot classification (RF)
    int shot_type = emlearn_predict(&shot_classifier, shot_feat_buf);

    // 6. Face angle + swing speed
    float face_angle = face_angle_estimate(&window);
    float swing_speed = swing_speed_estimate(&window);

    // 7. Build shot_event
    struct shot_event evt = { ... };

    // 8. Send to session_thread (Phase 2) or log (Phase 1)
    k_msgq_put(&shot_q, &evt, K_NO_WAIT);
    LOG_INF("Shot: type=%d pos=(%d,%d)mm angle=%.1f speed=%.1f",
            shot_type, (int)adj_x, (int)adj_y, face_angle, swing_speed);
  }
```

**imu_q design:**
```c
/* sensor_thread snapshots ring buffer on impact and sends to ml_thread */
K_MSGQ_DEFINE(imu_q, sizeof(struct impact_window), 4, 4);
/* 4 slots × ~4.5KB = ~18KB — sufficient since impacts are seconds apart in play */
/* 4 slots (not 8): tennis impacts are spaced seconds apart, 4 provides adequate margin */

/* sensor_thread also needs a rolling ring buffer for pre-impact data */
static int16_t icm_ring[200][6];  /* 250ms @ 800Hz, circular, 2,400B */
static uint16_t ring_head = 0;
```

**Error handling for queue full:**
```c
/* sensor_thread: non-blocking put, log drops */
if (k_msgq_put(&imu_q, &window, K_NO_WAIT) != 0) {
    dropped_impacts++;
    LOG_WRN("imu_q full, impact dropped (total: %d)", dropped_impacts);
}

/* ml_thread: non-blocking put for shot_q, log drops */
if (k_msgq_put(&shot_q, &evt, K_NO_WAIT) != 0) {
    dropped_shots++;
    LOG_WRN("shot_q full, shot dropped");
}
```

**Resource impact (corrected from TECH_SPEC §5.6):**
| Component | Flash | RAM |
|-----------|-------|-----|
| SVR ×2 (X, Y) | ~6 KB | ~1.5 KB |
| RF (20 trees, depth 8) | ~5 KB | ~1 KB |
| Impact feature buffer (39 floats) | 0 | 156 B |
| Shot feature buffer (30 floats) | 0 | 120 B |
| imu_q (4 slots × 4,470B) | 0 | ~17.9 KB |
| ICM ring buffer (sensor_thread) | 0 | 2.4 KB |
| emlearn runtime | ~2 KB | ~200 B |
| Face angle + swing speed | ~1 KB | ~200 B |
| ml_thread stack | 0 | 4 KB |
| **Phase 1 ML total** | **~14 KB** | **~27.5 KB** |

**Cumulative RAM:** Phase 0 (~22KB) + Phase 1 (~28KB) = ~50KB / 256KB (20%)

> **Note:** TECH_SPEC §5.6 lists ML RAM as ~5.3KB (single buffer, no queue). This is
> misleading — `K_MSGQ_DEFINE` allocates all slot memory statically. The plan's ~28KB
> figure is the correct cost. TECH_SPEC should be updated.

**Inference timing target:**
- Feature extraction: ~1ms
- SVR ×2: ~100µs × 2 = ~200µs
- RF: ~100µs
- Face angle + swing speed: ~50µs
- **Total: <2ms** (well under 10ms target)

**Verification:**
- Flash firmware with trained models
- Hit racket 20 times at known positions
- Compare on-device predictions vs Python predictions on same data
- Timing: measure with k_cycle_get_32() around each inference step

---

## Step 6: Face Angle Algorithm

Implement complementary filter for racket face pitch at impact.

**Files:**
- `src/face_angle.h` — API
- `src/face_angle.c` — complementary filter implementation

**Algorithm (from TECH_SPEC §5.3):**
```c
/* 50ms pre-impact window (40 samples @ 800Hz)
 * With the redesigned impact_window, pre-impact data is in pre_icm[0..199]
 * where pre_icm[199] is the sample just before impact.
 * 50ms pre-impact = samples 160..199 (indices within pre_icm) */
int16_t face_angle_estimate(const struct impact_window *win) {
    float theta_accel, theta_gyro, theta;
    float alpha = 0.95f;
    float dt = 1.0f / 800.0f;  /* 1.25ms */

    int start = (win->pre_count >= 40) ? (win->pre_count - 40) : 0;

    /* Initial estimate from gravity direction (accel Y, Z in pre_icm[start]) */
    theta = atan2f((float)win->pre_icm[start][1], (float)win->pre_icm[start][2]);

    /* Fuse accel + gyro over pre-impact samples up to impact */
    for (int i = start + 1; i < win->pre_count; i++) {
        theta_accel = atan2f((float)win->pre_icm[i][1], (float)win->pre_icm[i][2]);
        theta_gyro = theta + (float)win->pre_icm[i][3] * GYRO_SCALE * dt;
        theta = alpha * theta_gyro + (1.0f - alpha) * theta_accel;
    }

    /* Return in 0.1° units: 0°=flat, +°=open, -°=closed */
    return (int16_t)(theta * RAD_TO_DEG * 10.0f);
}
```

> **Note:** `pre_icm[i][0..2]` = accel XYZ, `pre_icm[i][3..5]` = gyro XYZ.

**Verification:**
- Mount sensor, hold racket at known angles (0°, +15°, -15°, +30°)
- Compare firmware output vs protractor measurement
- Target: +-5° accuracy

---

## Step 7: Swing Speed Algorithm

Implement gyro-based racket head speed estimation.

**Files:**
- `src/swing_speed.h` — API
- `src/swing_speed.c` — gyro peak algorithm

**Algorithm (from TECH_SPEC §5.4):**
```c
/* 100ms pre-impact window (80 samples @ 800Hz)
 * With redesigned impact_window, pre-impact gyro is in pre_icm[i][3..5]
 * 100ms pre-impact = samples (pre_count-80)..pre_count */
uint16_t swing_speed_estimate(const struct impact_window *win) {
    float omega_peak = 0.0f;

    int start = (win->pre_count >= 80) ? (win->pre_count - 80) : 0;

    /* Find peak angular velocity magnitude in pre-impact window */
    for (int i = start; i < win->pre_count; i++) {
        float gx = (float)win->pre_icm[i][3] * GYRO_SCALE;  /* to rad/s */
        float gy = (float)win->pre_icm[i][4] * GYRO_SCALE;
        float gz = (float)win->pre_icm[i][5] * GYRO_SCALE;
        float omega = sqrtf(gx*gx + gy*gy + gz*gz);
        if (omega > omega_peak) omega_peak = omega;
    }

    /* v_head = omega_peak × racket_length */
    float v_head = omega_peak * RACKET_LENGTH_M;  /* default 0.685m */
    float speed_kmh = v_head * 3.6f;

    /* Return in 0.1 km/h units */
    return (uint16_t)(speed_kmh * 10.0f);
}
```

**config.h:**
```c
#define RACKET_LENGTH_M         0.685f  /* Configurable via app (0.65-0.72m) */
#define GYRO_SCALE              (2000.0f / 32768.0f * DEG_TO_RAD)  /* ±2000dps → rad/s */
```

**Verification:**
- 50 serves with radar gun comparison
- Target: +-15% accuracy
- Note: this measures racket head speed, NOT ball speed

---

## Step 8: Calibration Protocol

Implement per-racket quick calibration (20 shots, 2 minutes).

**Files:**
- `src/calibration.h` — API + racket_calibration struct
- `src/calibration.c` — calibration logic + ZMS storage
- `src/main.c` — add CALIBRATION state

**Design (from TECH_SPEC §6.1):**
```c
struct racket_calibration {
    uint8_t  racket_id;        /* 0-7 */
    int16_t  offset_x_mm;
    int16_t  offset_y_mm;
    uint16_t scale_x_pct;      /* 100 = 1.0 */
    uint16_t scale_y_pct;
    char     name[16];         /* "Head Speed Pro" */
};

/* Calibration flow:
 * 1. Enter CALIBRATION state (via BLE command or button)
 * 2. Collect 20 center-hit impacts
 * 3. Run ML inference on each → 20 predicted (x,y) pairs
 * 4. Outlier rejection: compute median, discard samples >2σ from median
 *    Require at least 15 valid samples after rejection
 *    LED blink pattern: green=accepted, red=outlier rejected
 * 5. offset_x = 0 - median(predicted_x)  (median, not mean — robust to outliers)
 *    offset_y = 0 - median(predicted_y)
 * 6. Scale: offset-only for MVP (scale_x_pct = scale_y_pct = 100)
 *    Scale calibration deferred — requires known-position edge hits to define expected_std
 * 7. Store to ZMS with racket_id
 */

int calibration_start(uint8_t racket_id);
int calibration_add_sample(float predicted_x, float predicted_y);
int calibration_finish(struct racket_calibration *result);  /* returns 0 when 20 samples collected */
int calibration_load(uint8_t racket_id, struct racket_calibration *cal);
int calibration_save(const struct racket_calibration *cal);
```

**ZMS storage:**
- Up to 8 racket profiles, each ~28 bytes
- ZMS IDs: `CALIBRATION_ZMS_BASE + racket_id`
- Load active calibration at boot (racket_id from ZMS config)

**Verification:**
- Calibrate with racket A: hit 20 center shots
- Compare MAE before vs after calibration
- Target: >=30% MAE reduction from calibration
- Test with racket B (different model): calibration for A should not apply to B

---

## Step 9: Integration + On-Court Validation

Full ML pipeline running on-device, validated on court.

**shot_event struct (must use `__packed`):**
```c
struct __packed shot_event {
    uint32_t ts_offset_ms;
    uint8_t  shot_type;
    int16_t  impact_x_mm;
    int16_t  impact_y_mm;
    uint8_t  is_sweet_spot;
    uint16_t peak_accel_mg;
    uint16_t swing_speed;
    int16_t  face_angle_deg10;
    uint8_t  racket_id;
    uint8_t  reserved[3];      /* Pad to 20 bytes */
};
BUILD_ASSERT(sizeof(struct shot_event) == 20, "shot_event must be exactly 20 bytes");
```

**Integration test:**
```
End-to-end flow:
  1. Impact detected (ADXL372 >30g)
  2. sensor_thread snapshots ring buffer → 250ms pre + 140ms post + ADXL window
  3. Window sent to ml_thread via imu_q (K_NO_WAIT, log if dropped)
  4. ml_thread: impact features → SVR → shot features → RF → face_angle → swing_speed
  5. shot_event logged via serial (Phase 1) or ZMS (Phase 2)
  6. LED blinks on each processed shot

Performance metrics to log:
  - Feature extraction time (impact + shot features) (ms)
  - SVR inference time (µs)
  - RF inference time (µs)
  - Total pipeline latency (ms)
  - imu_q utilization (slots used / 4)
  - Dropped impacts count
```

**On-court validation protocol:**
1. Calibrate for test racket
2. Play 3 sessions (30 min each, mixed strokes)
3. Video record for ground truth
4. Compare on-device predictions vs video labels
5. Measure: MAE, classification accuracy, false impact rate

**Success criteria check:**
- [ ] Impact location <=5cm MAE (post-calibration)
- [ ] Shot classification >=85%
- [ ] Face angle +-5°
- [ ] Swing speed +-15%
- [ ] No crashes over 3×30 min sessions
- [ ] Inference <10ms
- [ ] No FIFO overflows during play

---

## File Summary

| File | Step | Purpose |
|------|------|---------|
| `src/ble_stream.c/.h` | 1 | Raw data BLE streaming (data collection mode) |
| `src/features.h` | 3 | Feature extraction API |
| `src/features.c` | 3 | 39-feature pipeline + Goertzel |
| `src/ml.h` | 5 | ML inference API |
| `src/ml.c` | 5 | emlearn wrapper, ml_thread |
| `src/face_angle.h/.c` | 6 | Complementary filter |
| `src/swing_speed.h/.c` | 7 | Gyro peak algorithm |
| `src/calibration.h/.c` | 8 | Per-racket calibration + ZMS |
| `models/impact_locator_svr.h` | 4 | emlearn SVR C header (X) |
| `models/impact_locator_svr_y.h` | 4 | emlearn SVR C header (Y) |
| `models/shot_classifier_rf.h` | 4 | emlearn RF C header |
| `tools/collect_data.py` | 1 | BLE → CSV data collection |
| `tools/label_impacts.py` | 2 | Video frame labeling |
| `tools/train_impact_locator.py` | 4 | SVR training |
| `tools/train_shot_classifier.py` | 4 | RF training |
| `tools/evaluate.py` | 4 | Cross-validation reports |

## Resource Budget (Phase 1 Cumulative)

| Resource | Phase 0 | Phase 1 Added | Cumulative | Budget |
|----------|---------|---------------|------------|--------|
| Flash | ~30 KB | ~14 KB (ML) + ~5 KB (BLE) | ~49 KB | 500 KB |
| RAM | ~22 KB | ~28 KB (ML+ring) + ~2 KB (BLE) | ~52 KB | 256 KB |
| Threads | 1 (sensor) | +1 (ml) | 2 | 4 planned |
| ZMS | 16 KB test | +224 B (8 cal profiles) | ~16 KB | 16 KB config |

## Risks (Phase 1 Specific)

| Risk | ID | Mitigation | Checkpoint |
|------|----|------------|------------|
| Impact location >4cm MAE | R1 | SVR→NN fallback, data augmentation, more features | Step 4 |
| Cross-racket generalization poor | R3 | 3+ racket training data, quick calibration | Steps 2,8 |
| ADXL372 100mg/LSB degrades ML | R13 | Quantization test in Step 4, ADXL375 fallback | Step 4 |
| Per-stroke hit-map offset | R8 | Include shot_type as SVR input feature | Step 4 |
| Data collection insufficient | — | Minimum 2,500 samples, augmentation if needed | Step 2 |

## Open Questions to Resolve

| Q | Question | Resolved By |
|---|----------|-------------|
| Q3 | Goertzel 146Hz/200Hz significant for position? | Step 4: feature importance analysis |
| Q4 | SVR vs NN accuracy comparison? | Step 4: if SVR <4cm, no need for NN |
| Q7 | 1600Hz vs 800Hz ODR? | Step 2: optional comparison dataset |
| Q10 | ADXL372 100mg/LSB sufficient? | Step 4: quantization test |

## Phase 0 Completion Checklist (must verify before starting Phase 1)

- [ ] Dual sensor 800Hz FIFO streaming stable (no overflows in 10 min)
- [ ] FIFO temporal alignment: ICM-ADXL timestamp skew < 2 samples (2.5ms)
- [ ] Impact detection reliable on real ball hits (not just desk taps)
- [ ] ZMS write/read verified on RRAM
- [ ] TPU damping < 50% (otherwise all training data may be invalid)
- [ ] Peak g-forces at dampener location measured and documented

---

## Dependencies for Phase 2

Phase 1 outputs that Phase 2 builds on:
- `ml.c/h` + `features.c/h` — full ML pipeline
- `models/*.h` — trained models (frozen for Phase 2)
- `calibration.c/h` — calibration protocol
- `face_angle.c/h` + `swing_speed.c/h` — algorithms
- `shot_event` struct — verified correct on real data
- `ble_stream.c` — reference for production BLE service design
- Accuracy baseline — Phase 2 UI shows real numbers

---

## Plan Validation (3-Agent Consensus)

**Method:** 2/3 consensus from 3 independent validators
**Overall:** APPROVED (all findings addressed)

### Consensus Findings Applied

| # | Finding | Severity | Resolution |
|---|---------|----------|------------|
| 1 | **Pre-impact window too short** (10ms vs 250ms needed) | Critical | Redesigned impact_window: 250ms pre + 140ms post + ADXL. Ring buffer in sensor_thread. |
| 2 | imu_q RAM budget mismatch with TECH_SPEC | Warning | Corrected to ~28KB. Reduced to 4 slots (sufficient for tennis pace). Note to update TECH_SPEC. |
| 3 | BLE streaming: no flow control, CRC, seq wrap | Warning | Switched to Indications (ACK'd), added CRC16, u16 seq, realistic throughput estimate. |
| 4 | Exponential decay fit too expensive for MCU | Warning | Changed to log-linear regression (O(N), no iterative solver). |
| 5 | Calibration: no outlier rejection | Warning | Added median-based offset, 2-sigma outlier rejection, min 15 valid samples. |
| 6 | shot_event needs `__packed` + BUILD_ASSERT | Warning | Added `__packed`, `BUILD_ASSERT(== 20)`, explicit 3B reserved padding. |
| 7 | imu_q/shot_q full not handled | Warning | Added K_NO_WAIT + drop counter + LOG_WRN for both queues. |
| 8 | Shot classifier 30 features not defined | Warning | Added explicit 30-feature table (6 stats × 6 axes = 30). |
| 9 | Phase 0 FIFO sync dependency not explicit | Warning | Added Phase 0 Completion Checklist with sync requirement (<2 samples). |
| 10 | Goertzel frequencies may be racket-dependent | Suggestion | Added note to validate per-racket during data collection. |
| 11 | Calibration scale factor undefined | Suggestion | Deferred to offset-only for MVP (scale = 100%). |

### Dropped (1/3 only)
Timestamp sync for video, double-promotion guards, ball machine accuracy, data collection stroke diversity
