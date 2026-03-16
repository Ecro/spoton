# Hardware Component List

> Managed list of all hardware components by development phase.
> Last updated: 2026-03-16

## Phase 0 — Hello Dual Sensor (Bench Testing)

**목적:** SPI 통신, FIFO 읽기, 임팩트 감지 검증 (책상 위)

### 구매 리스트 (Mouser Korea — kr.mouser.com)

| # | 부품 | 제품명 | Mouser 링크 | 예상가 |
|---|------|--------|------------|--------|
| 1 | MCU Dev Kit | nRF54L15-DK (Nordic) | [Mouser](https://www.mouser.com/ProductDetail/Nordic-Semiconductor/nRF54L15-DK?qs=3vio67wFuYon2dql6VAuBQ%3D%3D) | ~$42 |
| 2 | IMU Eval Board | EV_ICM-42688-P (TDK InvenSense) | [Mouser](https://www.mouser.com/ProductDetail/TDK-InvenSense/EV_ICM-42688-P?qs=hWgE7mdIu5SqHj7syF9KYA%3D%3D) | ~$15 |
| 3 | High-g Accel Stamp Board | EVAL-ADXL372Z (Analog Devices) | [Mouser](https://www.mouser.com/ProductDetail/Analog-Devices/EVAL-ADXL372Z?qs=%252BdQmOuGyFcG0QLzCQuUeJw%3D%3D) | ~$25 |
| 4 | 점퍼와이어 | Jumper wire kit | Mouser에서 "jumper wire" 검색 | ~$5 |
| | **Phase 0 합계** | | | **~$87** |

### Phase 0 벤치 구성도

```
┌──────────────────────────────────────────┐
│              책상 (Bench)                 │
│                                          │
│  nRF54L15 DK ──── SPI ──── EV_ICM-42688-P│
│      │                                   │
│      └──────── SPI ──── EVAL-ADXL372Z    │
│      │                                   │
│      └──── USB ──── PC (minicom 로그)     │
└──────────────────────────────────────────┘
```

### Phase 0 검증 항목

- [ ] ICM-42688-P SPI 통신 (WHO_AM_I 레지스터)
- [ ] ADXL372 SPI 통신 (DEVID 레지스터)
- [ ] ICM-42688-P FIFO watermark interrupt
- [ ] ADXL372 FIFO watermark interrupt
- [ ] 800Hz ODR 확인 (타이밍 측정)
- [ ] 책상 탁탁 두드려서 impact detection 확인

---

## Phase 1 — Hello ML (라켓 마운트 + 데이터 수집)

**목적:** 라켓에 센서 장착, 실제 타구 데이터 수집, ML 모델 훈련

### Phase 0 → Phase 1 변경점

| 항목 | Phase 0 | Phase 1 | 이유 |
|------|---------|---------|------|
| MCU 위치 | 책상 위 DK | 소형 모듈 → 핸들 고정 | 라켓 스윙 중 데이터 수집 |
| MCU 보드 | nRF54L15 DK | Fanstel BM15A 모듈 (~10×15mm) | DK는 너무 크고 무거움 |
| IMU 보드 | EV_ICM-42688-P (큼) | 소형 ICM-42688-P breakout (~15×15mm) | 댐프너 위치에 장착 필요 |
| High-g 보드 | EVAL-ADXL372Z (OK) | 그대로 사용 (stamp 크기 ~20×15mm) | 충분히 작음 |
| 전원 | USB (DK → PC) | LiPo 배터리 (100mAh) | 무선 동작 필요 |
| 데이터 저장 | USB 시리얼 로그 | RRAM 내부 저장 → BLE 다운로드 | 무선 동작 |
| 프로그래밍 | DK 내장 J-Link | DK의 J-Link → SWD 케이블로 모듈 연결 | 집에서만 |

### Phase 1 추가 구매 리스트

| # | 부품 | 제품명 | 예상가 | 비고 |
|---|------|--------|--------|------|
| 5 | 소형 MCU 모듈 | Fanstel BM15A (nRF54L15, ~10×15mm) | ~$15 | Fanstel 직구 또는 Mouser |
| 6 | 소형 IMU Breakout | ICM-42688-P breakout (소형) | ~$8 | AliExpress / Amazon |
| 7 | 배터리 | LiPo 3.7V 100mAh (Adafruit 1570) | ~$6 | Mouser |
| 8 | 충전기 | Adafruit 4410 Micro-Lipo USB-C | ~$7 | Mouser |
| 9 | 마운트 재료 | 벨크로, 테이프, 수축튜브 | ~$5 | |
| | **Phase 1 추가 합계** | | **~$41** | |

### Phase 1 라켓 마운트 구성도

```
┌─ 라켓 헤드 ─────────────────────┐
│    ┌─────────────────────┐       │
│    │     스트링 bed       │       │
│    │                     │       │
│    │  [IMU] [ADXL372]    │       │  ← 댐프너 위치, 테이프 고정
│    └────────┬────────────┘       │
└─────────────│────────────────────┘
              │ SPI 와이어 (~10cm)
         ┌────┴─────┐
         │ BM15A    │  ← 핸들 상단, 벨크로 고정
         │ + 배터리  │     ~15×10mm + 배터리, 총 ~5g
         └──────────┘

DK (집에서 펌웨어 개발/디버깅 전용)
  └── SWD 케이블로 BM15A에 프로그래밍
```

---

## Phase 2 — Custom PCB (댐프너 폼팩터)

**목적:** 28×11mm PCB에 모든 부품 통합, 댐프너 형태

> Phase 2 BOM은 TECH_SPEC.md §3.6 참조.
> 모든 부품이 하나의 PCB에 실장되어 DK/breakout 보드 불필요.

### Phase 1 → Phase 2 변경점

| 항목 | Phase 1 | Phase 2 | 이유 |
|------|---------|---------|------|
| MCU | BM15A 모듈 | nRF54L15 QFAA (QFN48) 직접 실장 | 커스텀 PCB |
| IMU | breakout 보드 | ICM-42688-P (LGA-14) 직접 실장 | 커스텀 PCB |
| High-g | EVAL-ADXL372Z | ADXL372 (LGA-16) 직접 실장 | 커스텀 PCB |
| PMIC | 외부 충전기 | nPM1100 (WLCSP) 직접 실장 | 통합 |
| 크기 | 분산 (~핸들+스트링) | **28×11mm 단일 PCB** | 댐프너 폼팩터 |
| 무게 | ~20g+ | **~7g** | 댐프너 무게 |
| 케이스 | 없음 (테이프) | TPU 3D 프린트 | 보호 + 고정 |

---

## 누적 예산 요약

| Phase | 추가 비용 | 누적 합계 | Mouser 주문 |
|-------|----------|----------|------------|
| Phase 0 | ~$87 | ~$87 | **지금 주문** |
| Phase 1 | ~$41 | ~$128 | Phase 0 완료 후 |
| Phase 2 | ~$60 (1pcs) | ~$188 | Phase 1 완료 후 |
