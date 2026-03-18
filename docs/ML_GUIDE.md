# SpotOn ML 파이프라인 가이드

> 임베디드 ML 초보자를 위한 상세 가이드.
> "어떤 데이터를, 어떻게 가공해서, 어떤 모델로 학습하고, MCU에 어떻게 올리는가"

---

## Part 1: 임베디드 ML 개요

### 1.1 일반 ML vs 임베디드 ML (TinyML)

```
일반 ML (서버/클라우드):
  데이터 → Python (scikit-learn/PyTorch) → 모델 → 서버에서 추론
  제약 없음: GPU, 수 GB 메모리, 수 TB 스토리지

임베디드 ML (TinyML):
  데이터 → Python (학습은 PC에서) → 모델 변환 → MCU에서 추론
  제약: 128MHz CPU, 256KB RAM, 1.5MB Flash, 배터리, 실시간
```

SpotOn에서의 ML:
```
학습 (PC, 1회):
  Python + scikit-learn → SVR/RF 모델 → emlearn → C 헤더 파일

추론 (MCU, 매 타구마다):
  센서 데이터 → 특징 추출 (C 코드) → SVR/RF 추론 (C 코드) → 결과
  소요 시간: ~2ms (128MHz Cortex-M33)
  메모리: ~14KB Flash, ~5KB RAM
```

### 1.2 왜 SVR/RF인가? (Neural Network가 아니라)

```
모델 비교 (임베디드 관점):

| 모델 | Flash | RAM | 추론 시간 | 정확도 | 학습 난이도 |
|------|-------|-----|----------|--------|-----------|
| SVR (RBF) | ~6KB | ~1.5KB | ~100µs | ≤4cm | 낮음 |
| Random Forest | ~5KB | ~1KB | ~100µs | ≥90% | 낮음 |
| MLP (3층) | ~20KB | ~5KB | ~500µs | ≤3cm? | 중간 |
| CNN (1D) | ~50KB+ | ~10KB+ | ~5ms+ | ≤2cm? | 높음 |
| Transformer | 수 MB | 수십 KB | 수십 ms | 최고? | 매우 높음 |

SpotOn 선택: SVR + RF
  이유:
  1. Flash 11KB (500KB 중 2%) — 매우 작음
  2. 추론 ~200µs — 실시간 여유
  3. 학습 데이터 300~600발로 충분 (NN은 수천~수만 필요)
  4. emlearn으로 C 변환 검증됨
  5. SmartDampener 논문이 SVR로 3.03cm 달성 → 검증된 방법

나중에 정확도가 부족하면?
  → SVR → MLP → CNN 순서로 업그레이드 고려
  → emlearn은 MLP도 지원, TFLite Micro로 CNN도 가능
```

### 1.3 Edge Impulse vs emlearn vs 기타

```
┌─────────────────────────────────────────────────────────────────┐
│                  임베디드 ML 플랫폼 비교                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Edge Impulse (edgeimpulse.com)                                 │
│  ├── 웹 GUI 기반 — 코딩 최소화                                    │
│  ├── 데이터 업로드 → 자동 전처리 → 모델 학습 → C++ 라이브러리 생성    │
│  ├── nRF 시리즈 공식 지원 (nRF Connect SDK 통합)                   │
│  ├── DSP 블록: FFT, Spectrogram, MFE 등 내장                     │
│  ├── 모델: NN (Keras), K-NN, Anomaly Detection                  │
│  └── 무료 (개인/개발자), 유료 (기업)                                │
│                                                                 │
│  emlearn (github.com/emlearn/emlearn)                           │
│  ├── Python 라이브러리 — scikit-learn 모델 → C 코드 변환           │
│  ├── 지원 모델: Decision Tree, Random Forest, SVR, MLP, K-NN     │
│  ├── 출력: 순수 C 헤더 파일 (의존성 없음!)                          │
│  ├── 어떤 MCU든 동작 (Zephyr, Arduino, bare-metal)                │
│  └── 완전 오픈소스, 무료                                          │
│                                                                 │
│  TensorFlow Lite Micro (tflite-micro)                           │
│  ├── Google의 임베디드 ML 프레임워크                                │
│  ├── TF/Keras → .tflite → C 배열 → MCU에서 추론                  │
│  ├── CNN, RNN 등 복잡한 모델 가능                                  │
│  ├── 메모리 오버헤드 큼 (~50KB+ Flash, ~20KB+ RAM)                 │
│  └── Zephyr에서 사용 가능 (CONFIG_TENSORFLOW_LITE_MICRO)          │
│                                                                 │
│  ONNX Runtime Micro                                             │
│  ├── PyTorch/ONNX → MCU 추론                                    │
│  ├── TFLite Micro와 비슷한 위치                                   │
│  └── Zephyr 통합은 실험적                                        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.4 SpotOn의 선택: emlearn (Edge Impulse 아님)

```
Edge Impulse를 쓰지 않는 이유:

1. 커스텀 특징 추출이 핵심
   - SpotOn의 39개 특징은 직접 설계한 것 (Goertzel, 감쇠율, 상관관계 등)
   - Edge Impulse는 표준 DSP 블록 (FFT, Spectrogram) 사용
   - 우리 특징을 Edge Impulse에 넣으려면 "커스텀 블록" 작성 필요 → 결국 직접 코딩

2. SVR 미지원
   - Edge Impulse는 NN, K-NN 위주
   - SVR (RBF kernel)은 지원하지 않음
   - SVR이 이 문제에 가장 적합 (SmartDampener 검증)

3. 오프라인 워크플로우 선호
   - Edge Impulse는 클라우드 의존 (데이터 업로드 필요)
   - emlearn은 로컬 Python 스크립트로 완결

Edge Impulse가 적합한 경우:
   - 음성 인식 (MFCC + NN)
   - 이미지 분류 (CNN)
   - 표준 진동 분류 (FFT + NN)
   - "빨리 프로토타입 만들고 싶다"

SpotOn에 emlearn이 적합한 이유:
   ✓ SVR + RF 직접 지원
   ✓ 순수 C 출력 (의존성 0, 어떤 MCU든 OK)
   ✓ scikit-learn과 1:1 호환
   ✓ Flash 11KB로 충분
   ✓ SmartDampener 논문에서도 사용
```

---

## Part 2: 데이터 파이프라인 상세

### 2.1 원시 데이터 → 특징 → 모델 입력

```
1발의 충격 데이터 (raw):

  시간축:
  ──────┬───────────────────────────────────────────────
  -250ms│          사전 데이터 (pre-impact)              │
        │  ICM: 200 samples × 6축 = 2,400B              │ → 샷 분류 특징
        │  (가속도 XYZ + 자이로 XYZ)                      │ → 스윙 속도
        │                                               │ → 면 각도
  ──────┤                                               │
  -10ms │                                               │
  ──────┼───────────────────────────────────────────────
   0ms  │ ★ 충격 순간                                    │
  ──────┤                                               │
  +140ms│          사후 데이터 (post-impact)              │
        │  ICM: 112 samples × 6축 = 1,344B              │ → 위치 추정 특징
        │  ADXL: 120 samples × 3축 = 720B               │ → 충격 특징
  ──────┴───────────────────────────────────────────────

  총: ~4,470 bytes/발 (impact_window 구조체)
```

### 2.2 특징 추출 (Feature Engineering) 상세

```
39개 위치 추정 특징:

┌─ ADXL372 기반 (충격 직접 측정) ──────────────────────────┐
│                                                          │
│  #1  피크 진폭 (3축)     max(|ax|), max(|ay|), max(|az|) │
│      → "얼마나 세게 맞았나" (축별)                         │
│                                                          │
│  #2  충격 지속시간 (1)    30g 초과 구간의 길이 (ms)         │
│      → "충격이 얼마나 오래 지속됐나"                       │
│                                                          │
│  #3  충격 에너지 (1)      ∫(ax²+ay²+az²)dt               │
│      → "충격의 총 에너지"                                 │
│                                                          │
│  #12 피크 축 비율 (3)     ax_peak/|a|, ay/|a|, az/|a|    │
│      → "충격 방향" (어느 방향으로 힘이 집중됐나)            │
│                                                          │
│  #13 합성 크기 (1)        √(ax²+ay²+az²) at peak         │
│      → "피크 순간의 총 충격 크기"                          │
│                                                          │
│  #14 피크 도달 시간 (3)   각 축별 최대값까지 걸린 시간       │
│      → "충격 전파 시간차" → 위치 정보 포함!                │
│                                                          │
│  ADXL 소계: 12개 특징                                    │
└──────────────────────────────────────────────────────────┘

┌─ ICM-42688-P 기반 (진동 패턴 분석) ─────────────────────┐
│                                                          │
│  #4  감쇠율 (3축)         ln(|signal|)의 기울기           │
│      → "진동이 얼마나 빨리 사라지나"                       │
│      → 중심 타격: 빠른 감쇠, 가장자리: 느린 감쇠           │
│                                                          │
│  #5  영교차 횟수 (3축)    140ms 내 부호 변환 횟수          │
│      → "진동 주파수" 의 proxy                             │
│                                                          │
│  #6  RMS (3축)            √(mean(x²)) per axis           │
│      → "진동의 평균 에너지"                               │
│                                                          │
│  #7  에너지 비율 (3축)    전반 70ms / 후반 70ms 에너지     │
│      → "감쇠 패턴" → 위치에 따라 다름                     │
│                                                          │
│  #8  축간 상관관계 (3)    Pearson(XY), (XZ), (YZ)         │
│      → "진동이 어떤 방향으로 결합되나"                     │
│                                                          │
│  #9  ★ Goertzel 에너지 (6) 146Hz + 200Hz, XY 축별        │
│      → 핵심 특징! 라켓 고유진동수의 에너지                  │
│      → 146Hz = 1차 굽힘 모드, 200Hz = 프레임 2차 모드     │
│      → "이 두 주파수의 비율 = 위치" (Chen et al., 2022)   │
│      → 충격 강도에 무관 (force-irrelevant!)               │
│                                                          │
│  #10 자이로 크기 (1)      √(gx²+gy²+gz²) at impact       │
│      → "스윙 강도"                                       │
│                                                          │
│  #11 자이로 축 비율 (3)   gx/|g|, gy/|g|, gz/|g|         │
│      → "스윙 방향"                                       │
│                                                          │
│  #15 시간 상수 τ (2)      X, Y축 감쇠 지수 피팅           │
│      → "감쇠 속도" (정량적)                               │
│                                                          │
│  ICM 소계: 27개 특징                                     │
└──────────────────────────────────────────────────────────┘

총 39개 특징 → float[39] 배열 → SVR 입력
```

### 2.3 Goertzel 알고리즘 — 왜 FFT가 아닌가

```
테니스 라켓의 고유진동수:
  1차 굽힘 모드:  ~146Hz (현이 상하로 진동)
  1차 비틀림 모드: ~382Hz (현이 좌우로 비틀림)
  프레임 2차 모드: ~200Hz

이 주파수에서의 에너지가 "어디에 맞았는지" 정보를 담고 있음.

방법 1: FFT (Fast Fourier Transform)
  120 samples → 60개 주파수 빈
  해상도: 800Hz / 120 = 6.67Hz
  계산: O(N log N), ~120 × 7 = ~840 연산
  결과: 60개 빈 전부 나옴 (우리는 2개만 필요)
  → 낭비!

방법 2: Goertzel (우리 선택)
  특정 주파수 1개의 에너지만 O(N)으로 계산
  146Hz + 200Hz = 2번 호출
  계산: O(N) × 2 = ~240 연산
  → FFT 대비 3.5배 효율적, 필요한 것만 정확히 계산

Goertzel 알고리즘 (의사 코드):
  float goertzel_energy(samples[], n, target_freq, sample_rate) {
      k = (int)(0.5 + n * target_freq / sample_rate);
      w = 2 * PI * k / n;
      coeff = 2 * cos(w);
      s0 = s1 = s2 = 0;
      for (i = 0; i < n; i++) {
          s0 = samples[i] + coeff * s1 - s2;
          s2 = s1; s1 = s0;
      }
      return s1*s1 + s2*s2 - coeff*s1*s2;  // 에너지
  }
```

### 2.4 30개 샷 분류 특징

```
별도 특징 세트 (위치 추정 39개와 다름):

입력: 250ms 사전 데이터 (ICM 가속도 3축 + 자이로 3축)

통계 특징 (6축 × 4 통계 + 크기 6개 = 30개):
  각 축(ax, ay, az, gx, gy, gz)별:
    평균 (mean)
    표준편차 (std)
    최대값 (max)
    최소값 (min)
  → 6축 × 4 = 24개

  합성 크기 (magnitude):
    가속도 크기: mean, max, std
    자이로 크기: mean, max, std
  → 6개

  총: 30개 → float[30] → Random Forest 입력

왜 이것으로 샷 분류가 가능한가:
  포핸드: gx 양수 피크 (오른쪽 회전), gy 큰 변화
  백핸드: gx 음수 피크 (왼쪽 회전)
  서브:   gz 매우 큼 (위에서 아래), 자이로 크기 최대
  발리:   자이로 작음 (짧은 스윙), 가속도 급격한 변화
  슬라이스: 비대칭 자이로 패턴 (비스듬한 스윙)
```

---

## Part 3: 학습 파이프라인 상세

### 3.1 Python 환경 설정

```bash
# 가상환경 생성
python3 -m venv ~/spoton-ml
source ~/spoton-ml/bin/activate

# 필수 패키지
pip install numpy pandas scikit-learn matplotlib
pip install emlearn    # scikit-learn → C 변환
pip install bleak      # BLE 통신 (collect_data.py용)
pip install opencv-python  # 잉크 자국 라벨링 (label_impacts.py용)
```

### 3.2 학습 스크립트 구조

```
tools/
├── collect_data.py          # BLE → CSV 수집
├── label_impacts.py         # 잉크 사진 → (x,y) 라벨링
├── train_impact_locator.py  # SVR 학습 + emlearn 변환
├── train_shot_classifier.py # RF 학습 + emlearn 변환
└── evaluate.py              # 교차 검증 + 시각화
```

### 3.3 SVR 학습 상세 (위치 추정)

```python
# train_impact_locator.py (핵심 부분)

import numpy as np
import pandas as pd
from sklearn.svm import SVR
from sklearn.model_selection import cross_val_score, GridSearchCV
from sklearn.preprocessing import StandardScaler
import emlearn

# 1. 데이터 로드
data = pd.read_csv('data/labels/impacts.csv')
features = np.load('data/features/impact_features.npy')  # (N, 39)
labels_x = data['x_cm'].values  # 라켓 면 X 좌표
labels_y = data['y_cm'].values  # 라켓 면 Y 좌표

# 2. 정규화 (중요! SVR은 스케일에 민감)
scaler = StandardScaler()
features_scaled = scaler.fit_transform(features)

# 3. 하이퍼파라미터 탐색
param_grid = {
    'C': [1, 10, 50, 100],
    'gamma': ['scale', 0.01, 0.1],
    'epsilon': [0.1, 0.5, 1.0]
}

# X좌표 모델
svr_x = GridSearchCV(
    SVR(kernel='rbf'),
    param_grid,
    cv=5,                    # 5-fold 교차 검증
    scoring='neg_mean_absolute_error',
    n_jobs=-1                # 모든 CPU 사용
)
svr_x.fit(features_scaled, labels_x)
print(f"Best X params: {svr_x.best_params_}")
print(f"X MAE: {-svr_x.best_score_:.2f} cm")

# Y좌표 모델 (동일 과정)
svr_y = GridSearchCV(SVR(kernel='rbf'), param_grid, cv=5,
                     scoring='neg_mean_absolute_error', n_jobs=-1)
svr_y.fit(features_scaled, labels_y)
print(f"Y MAE: {-svr_y.best_score_:.2f} cm")

# 4. emlearn 변환 → C 헤더
model_x = emlearn.convert(svr_x.best_estimator_)
model_x.save(file='models/impact_locator_svr_x.h', name='impact_svr_x')

model_y = emlearn.convert(svr_y.best_estimator_)
model_y.save(file='models/impact_locator_svr_y.h', name='impact_svr_y')

print("C headers saved to models/")

# 5. 결과 시각화
# → 라켓 면 위에 실제(x) vs 추정(o) 점 그리기
```

### 3.4 RF 학습 상세 (샷 분류)

```python
# train_shot_classifier.py (핵심 부분)

from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report, confusion_matrix

# 1. 데이터 로드
shot_features = np.load('data/features/shot_features.npy')  # (N, 30)
shot_labels = data['shot_type'].values  # 0=FH, 1=BH, 2=SV, 3=VL, 4=SL

# 2. 학습
rf = RandomForestClassifier(
    n_estimators=20,    # 트리 20개 (MCU 메모리 고려)
    max_depth=8,        # 깊이 제한 (과적합 방지 + 메모리)
    random_state=42
)

# 3. 교차 검증
scores = cross_val_score(rf, shot_features, shot_labels, cv=5)
print(f"Accuracy: {scores.mean():.1%} (±{scores.std():.1%})")

# 4. 전체 데이터로 최종 학습
rf.fit(shot_features, shot_labels)
print(classification_report(shot_labels, rf.predict(shot_features),
      target_names=['FH', 'BH', 'SV', 'VL', 'SL']))

# 5. emlearn 변환
model_rf = emlearn.convert(rf)
model_rf.save(file='models/shot_classifier_rf.h', name='shot_classifier')
```

### 3.5 emlearn 출력 → MCU 코드

```c
/* 생성된 C 헤더 파일 예시 (models/impact_locator_svr_x.h) */

#ifndef IMPACT_SVR_X_H
#define IMPACT_SVR_X_H

/* 자동 생성됨 — 직접 수정하지 마세요 */
/* SVR model: kernel=rbf, C=50, gamma=scale, epsilon=0.5 */
/* Training MAE: 3.2 cm, CV MAE: 3.8 cm */

static const int32_t impact_svr_x_n_support = 142;
static const float impact_svr_x_support_vectors[142][39] = {
    { -0.234, 1.023, ... },
    { 0.567, -0.891, ... },
    ...
};
static const float impact_svr_x_dual_coef[142] = { ... };
static const float impact_svr_x_intercept = 2.345;
static const float impact_svr_x_gamma = 0.0256;

/* 추론 함수 */
static inline float impact_svr_x_predict(const float features[39]) {
    float result = impact_svr_x_intercept;
    for (int i = 0; i < impact_svr_x_n_support; i++) {
        float kernel_val = 0;
        for (int j = 0; j < 39; j++) {
            float diff = features[j] - impact_svr_x_support_vectors[i][j];
            kernel_val += diff * diff;
        }
        kernel_val = expf(-impact_svr_x_gamma * kernel_val);  /* RBF kernel */
        result += impact_svr_x_dual_coef[i] * kernel_val;
    }
    return result;  /* X 좌표 (cm) */
}

#endif

/* 펌웨어에서 사용: */
#include "models/impact_locator_svr_x.h"
float x_cm = impact_svr_x_predict(features);
```

---

## Part 4: 학습 반복 루프 (Iteration)

### 4.1 첫 번째 학습 결과 해석

```
Round 1 (300발) 결과 예시:

  === Impact Locator (SVR) ===
  X MAE: 4.8 cm (목표: ≤4cm)  ← 아직 부족
  Y MAE: 5.2 cm (목표: ≤4cm)  ← 아직 부족
  Combined MAE: 5.0 cm

  === Shot Classifier (RF) ===
  Accuracy: 87.3% (목표: ≥90%)  ← 근접
  FH: 92%, BH: 90%, SV: 95%, VL: 72%, SL: 78%

분석:
  1. X/Y MAE 5cm → 4cm까지 1cm 개선 필요
  2. 발리/슬라이스 정확도 낮음 → 데이터 부족?
  3. 어디가 문제인지 시각화로 확인
```

### 4.2 에러 분석 (무엇을 개선할지 판단)

```python
# evaluate.py 로 시각화

# 1. 위치별 MAE 히트맵
#    → 가장자리에서 MAE가 높으면 → 가장자리 데이터 추가 수집
#    → 중심에서도 높으면 → 특징 품질 문제

# 2. 특징 중요도 (permutation importance)
from sklearn.inspection import permutation_importance
result = permutation_importance(svr_x, X_test, y_test, n_repeats=10)
# → 어떤 특징이 가장 중요한지, 불필요한 특징은 제거

# 3. 잔차(residual) 분석
#    실제값 - 추정값을 그래프로
#    → 체계적 편향(bias)이 있으면 보정 필요
#    → 랜덤 오차만 있으면 데이터 추가로 개선 가능

# 4. 학습 곡선 (learning curve)
from sklearn.model_selection import learning_curve
train_sizes, train_scores, val_scores = learning_curve(
    svr_x, features, labels_x, train_sizes=[50, 100, 200, 300])
# → 300발에서 아직 개선 추세면 → 데이터 추가가 효과적
# → 300발에서 이미 수렴이면 → 모델/특징 변경 필요
```

### 4.3 개선 전략 우선순위

```
MAE 개선이 필요할 때, 시도 순서:

1순위: 보정(Calibration) 적용 (가장 쉬움, 효과 큼)
  → 20발 중심 히트 → 오프셋 계산 → MAE 30%+ 감소 기대
  → 이것만으로 5cm → 3.5cm 가능할 수 있음

2순위: 하이퍼파라미터 튜닝 (코드만 수정)
  → C, gamma, epsilon 범위 확대
  → GridSearchCV로 자동 탐색

3순위: 데이터 추가 수집 (노력 필요)
  → Round 2: 300발 추가 (MAE 높은 영역 집중)
  → 학습 곡선에서 데이터 추가 효과 확인 후 결정

4순위: 특징 변경 (리서치 필요)
  → 새로운 특징 추가 (예: 2차 감쇠 항, 추가 주파수)
  → 불필요한 특징 제거
  → 특징 조합 (polynomial features)

5순위: 모델 변경 (최후 수단)
  → SVR → MLP (emlearn 지원)
  → SVR → CNN (TFLite Micro 필요, 메모리 증가)
```

### 4.4 보정(Calibration)의 위력

```
보정 전:
  모델 출력: (3.2, -1.5) cm   실제: (0, 0) cm  → 오차 3.5cm
  모델 출력: (5.1, 2.3) cm    실제: (2, 4) cm   → 오차 3.6cm

보정 과정 (20발 중심 히트):
  20발 추정 평균: (2.8, -0.7) cm
  오프셋 = (0, 0) - (2.8, -0.7) = (-2.8, +0.7)

보정 후:
  조정된: (3.2-2.8, -1.5+0.7) = (0.4, -0.8) cm  → 오차 0.9cm!
  조정된: (5.1-2.8, 2.3+0.7) = (2.3, 3.0) cm     → 오차 1.0cm!

→ 보정만으로 MAE 3.5cm → ~1cm 수준까지 감소 가능
→ 이것이 "라켓별 보정"이 필수인 이유
→ 라켓/현 장력이 바뀌면 재보정 필요
```

---

## Part 5: MCU 배포

### 5.1 PC → MCU 전환 시 주의점

```
Python (학습)                          C (MCU 추론)
─────────────────────────────────────────────────────
float64 (기본)                         float32 (FPU 지원)
  → C에서 반드시 float 사용, double 금지

StandardScaler 적용                    스케일러 파라미터를 C에 하드코딩
  scaler.mean_, scaler.scale_          static const float mean[39] = {...};
                                       static const float scale[39] = {...};

numpy 배열 연산                         for 루프로 직접 계산
  features @ weights                   for(i) for(j) result += f[i]*w[j];

sklearn.svm.SVR.predict()              emlearn 생성 함수
                                       impact_svr_x_predict(features);
```

### 5.2 정밀도 검증 (Python vs C)

```
반드시 검증:
  같은 입력 데이터 → Python 출력 vs C 출력 비교
  차이가 0.01cm 이하여야 함

검증 방법:
  1. Python에서 10개 테스트 데이터의 features와 예측값 저장
  2. C 코드에서 같은 features로 예측
  3. 값 비교

차이가 크면:
  → float64 vs float32 정밀도 차이
  → 스케일러 파라미터 반올림 오차
  → expf() 구현 차이 (MCU의 수학 라이브러리)
```

### 5.3 추론 시간 측정

```c
/* MCU에서 추론 시간 측정 */
uint32_t start = k_cycle_get_32();

features_extract_impact(&window, impact_feat);
features_extract_shot(&window, shot_feat);
float x = impact_svr_x_predict(impact_feat);
float y = impact_svr_y_predict(impact_feat);
int shot = shot_classifier_predict(shot_feat);

uint32_t end = k_cycle_get_32();
uint32_t us = k_cyc_to_us_floor32(end - start);
LOG_INF("Inference time: %u us", us);

/* 예상: ~2,000 us (2ms) — 10ms 목표 대비 여유 */
```

---

## Part 6: 고급 주제 (Phase 1 이후)

### 6.1 Pseudo-labeling (반자동 확장)

```
Round 1+2 모델(600발)이 있으면:

1. 잉크 없이 1시간 플레이 → 500발 수집 (비라벨)
2. 모델로 각 타구의 위치 추정
3. 추정 "확신도" (SVR의 경우 서포트 벡터까지 거리)로 필터링
4. 확신도 높은 것만 (예: 상위 70%) 훈련 데이터로 추가
5. 재학습 → 모델 개선 → 더 많은 데이터 확신 → 반복

결과: 수동 라벨 600발 + 자동 라벨 ~350발 = 950발 모델
```

### 6.2 전이 학습 (Transfer Learning)

```
라켓 A로 학습한 모델을 라켓 B에 적용:

방법 1: 보정만 (현재 계획)
  → 라켓 B로 20발 중심 히트 → 오프셋만 조정
  → 간단하지만 정확도 제한

방법 2: Fine-tuning
  → 라켓 B로 100발 잉크 데이터 수집
  → 라켓 A 모델을 기반으로 재학습 (warm start)
  → 적은 데이터로 높은 정확도

방법 3: 범용 모델
  → 3+ 라켓 데이터로 학습 (현 TECH_SPEC 계획)
  → 어떤 라켓이든 보정만으로 OK
  → 가장 많은 데이터 필요
```

### 6.3 NN으로 업그레이드 (SVR이 부족할 때)

```
SVR MAE > 4cm이고, 데이터/특징 개선으로도 안 되면:

Option 1: MLP (emlearn 지원)
  from sklearn.neural_network import MLPRegressor
  mlp = MLPRegressor(hidden_layer_sizes=(64, 32), max_iter=1000)
  → emlearn.convert(mlp) → C 코드
  → Flash ~20KB, RAM ~5KB, 추론 ~500µs
  → SVR 대비 2배 메모리, 5배 추론 시간

Option 2: 1D CNN (TFLite Micro 필요)
  → raw 진동 신호를 직접 입력 (특징 추출 불필요!)
  → 더 높은 정확도 가능 (2~3cm?)
  → Flash ~50KB+, RAM ~20KB+, 추론 ~5ms+
  → 학습 데이터 1,000+ 발 필요
  → Zephyr CONFIG_TENSORFLOW_LITE_MICRO=y

현재 SpotOn에서는 SVR + RF로 시작하고,
필요시에만 업그레이드하는 전략.
```

---

## 용어 정리

| 용어 | 설명 |
|------|------|
| SVR | Support Vector Regression — 서포트 벡터 머신의 회귀 버전 |
| RF | Random Forest — 결정 트리 앙상블 |
| RBF | Radial Basis Function — SVR의 커널 함수 (가우시안) |
| MAE | Mean Absolute Error — 평균 절대 오차 (cm) |
| emlearn | scikit-learn → C 변환 라이브러리 |
| Goertzel | 단일 주파수 에너지 추출 알고리즘 (FFT의 효율적 대안) |
| Feature Engineering | 원시 신호에서 의미 있는 숫자(특징) 추출 |
| Cross-validation | K-fold 교차 검증 (과적합 방지) |
| Pseudo-labeling | 모델 추정값을 라벨로 사용하는 반자동 기법 |
| Fine-tuning | 기존 모델을 새 데이터로 미세 조정 |
| TinyML | 마이크로컨트롤러에서의 머신러닝 |
