# SpotOn 사전 학습 가이드

> Embedded Linux 18년차 엔지니어를 위한 MCU + Zephyr RTOS + SpotOn 프로젝트 기초 학습 자료.
> Linux 개념을 기준으로 MCU/Zephyr와의 차이점을 중심으로 설명합니다.

---

## Part 1: Linux → MCU 마인드셋 전환

### 1.1 가장 큰 차이: OS가 없다 (거의)

| | Embedded Linux | MCU (Zephyr RTOS) |
|---|---|---|
| 커널 | Linux kernel (수백만 LOC) | Zephyr kernel (~수만 LOC) |
| 메모리 보호 | MMU, 가상 메모리, 프로세스 격리 | MPU (선택), 물리 주소 직접 사용, 스레드 격리 제한 |
| 파일시스템 | ext4, tmpfs, sysfs, procfs | 없음 (Flash에 직접 읽기/쓰기, 또는 LittleFS/ZMS) |
| 동적 할당 | malloc/free 자유롭게 | **금지** (스택/정적 할당만) |
| 드라이버 | 커널 모듈, /dev/spidevN | Device Tree + Zephyr API (유사하지만 더 단순) |
| 쉘 | bash, sh | Zephyr Shell (선택, 디버깅용) |
| 부팅 | U-Boot → Kernel → init → systemd | Reset → main() (수 ms) |
| 디버깅 | gdb + strace + dmesg | J-Link + GDB + LOG_INF (printf 디버깅) |

### 1.2 메모리: 가장 중요한 제약

```
nRF54L15 메모리:
  RRAM (Flash 대체): 1,524 KB — 코드 + 데이터 (비휘발성)
  RAM:               256 KB   — 스택, 힙, 변수 (휘발성)

비교:
  라즈베리파이:  1-8 GB RAM, 16-64 GB SD카드
  nRF54L15:     256 KB RAM, 1.5 MB RRAM

→ 동적 할당 금지의 이유:
  - 256KB에서 malloc/free → 단편화 → 몇 시간 후 OOM
  - 해결: 모든 버퍼를 컴파일 시점에 크기 확정 (static, stack)
```

**Linux에서의 습관을 버려야 할 것:**
```c
// ❌ 절대 하지 말 것
char *buf = malloc(1024);
char *str = strdup("hello");
linked_list_t *node = calloc(1, sizeof(*node));

// ✅ 이렇게
static uint8_t buf[1024];           /* 파일 스코프 정적 */
char str[] = "hello";               /* 스택 (함수 내) */
K_MSGQ_DEFINE(my_q, 20, 32, 4);    /* 컴파일 타임 큐 */
```

### 1.3 스레드 vs 프로세스

Linux에서는 프로세스(fork) + 스레드(pthread)를 사용합니다.
Zephyr에서는 **스레드만** 있고, 모든 스레드가 같은 주소 공간을 공유합니다.

```
Linux:
  프로세스 A (독립 메모리) ←→ IPC (소켓, 파이프, D-Bus) ←→ 프로세스 B

Zephyr:
  스레드 A (같은 메모리) ←→ 메시지 큐 / 세마포어 ←→ 스레드 B
  ※ 전역 변수를 아무 스레드나 접근 가능 → 동기화 필수
```

SpotOn의 4개 스레드:
```
sensor_thread (P2)  — 센서 FIFO 읽기, 800Hz
ml_thread     (P5)  — ML 추론 (SVR + RF)
session_thread(P6)  — ZMS 저장, BLE 전송
main_thread   (P7)  — 상태 머신, WDT, 배터리
```

### 1.4 인터럽트 (ISR) — Linux에는 없는 개념

Linux에서는 인터럽트를 직접 다루지 않습니다 (커널이 처리).
MCU에서는 **인터럽트 핸들러(ISR)를 직접 작성**합니다.

```c
/* ISR: 센서가 "FIFO 가득 찼어" 신호를 보낼 때 호출됨 */
void fifo_watermark_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    /* ⚠ ISR 안에서 할 수 있는 것: 최소한만! */
    k_sem_give(&fifo_sem);  // ✅ 세마포어 시그널 (논블로킹)

    /* ⚠ ISR 안에서 절대 하면 안 되는 것: */
    // k_sleep(K_MSEC(10));     ❌ 블로킹 호출
    // spi_transceive(...);      ❌ SPI 통신 (블로킹)
    // LOG_INF("data ready");    ❌ 로깅 (블로킹 가능)
    // malloc(1024);             ❌ 동적 할당
}
```

Linux 비유:
```
Linux:     top-half (hardirq) → bottom-half (softirq/tasklet/workqueue)
Zephyr:    ISR (최소) → k_sem_give → 스레드에서 실제 처리
```

SpotOn에서의 패턴:
```
센서 INT 핀 → GPIO 인터럽트 → ISR → k_sem_give(&fifo_sem) → sensor_thread 깨어남 → SPI FIFO 읽기
```

---

## Part 2: Zephyr RTOS 기초

### 2.1 Zephyr 프로젝트 구조

```
spoton/                         ← 우리 프로젝트
├── CMakeLists.txt              ← Linux의 Makefile 같은 것 (CMake)
├── prj.conf                    ← Linux의 .config (Kconfig)
├── boards/
│   └── nrf54l15dk_...overlay   ← Linux의 Device Tree Overlay와 동일 개념!
├── src/
│   ├── main.c                  ← 진입점 (Linux의 init 프로세스)
│   ├── sensor.c/.h             ← 센서 드라이버
│   └── config.h                ← 상수 정의
└── build/                      ← 빌드 출력 (out-of-tree)
```

### 2.2 Kconfig — 익숙할 것임

Linux 커널과 **동일한 Kconfig 시스템**:
```
# prj.conf (= Linux의 .config)
CONFIG_LOG=y                    # 로깅 활성화
CONFIG_GPIO=y                   # GPIO 드라이버
CONFIG_SPI=y                    # SPI 드라이버
CONFIG_BT=y                     # BLE 스택
```

차이점:
```
Linux:   make menuconfig → .config → make
Zephyr:  prj.conf 직접 편집 → west build (자동 병합)
```

### 2.3 Device Tree — 거의 동일!

Embedded Linux의 DTS/DTSI/DT Overlay를 아시니, 이건 익숙할 겁니다:

```dts
/* Linux DT overlay와 거의 동일한 문법 */
&spi21 {
    status = "okay";
    cs-gpios = <&gpio1 12 GPIO_ACTIVE_LOW>;

    my_sensor: my_sensor@0 {
        compatible = "vnd,spi-device";
        reg = <0>;
        spi-max-frequency = <10000000>;
    };
};
```

차이점:
| | Linux DTS | Zephyr DTS |
|---|---|---|
| 컴파일러 | dtc → .dtb (런타임 파싱) | dtc → C 매크로 (컴파일 타임) |
| 접근 | of_property_read_u32() | DT_PROP(node, prop) 매크로 |
| 오버레이 | .dtbo 런타임 적용 | .overlay 컴파일 타임 병합 |

Zephyr에서 DT 노드 접근:
```c
/* Linux: of_find_compatible_node() + of_property_read_u32() */
/* Zephyr: 컴파일 타임 매크로 */
#define SPI_DEV DT_NODELABEL(my_sensor)
const struct device *spi = DEVICE_DT_GET(DT_BUS(SPI_DEV));
struct spi_cs_control cs = SPI_CS_CONTROL_INIT(SPI_DEV, 0);
```

### 2.4 빌드 시스템: west

```bash
# Linux BSP
bitbake my-image                     # 또는 make -C kernel

# Zephyr
west build -b nrf54l15dk/nrf54l15/cpuapp    # 빌드
west flash                                   # 플래시 (= dd if=image of=/dev/mmcblk0)
west debug                                   # GDB 디버깅
```

`west`는 Zephyr의 메타 빌드 도구:
- `repo` (Android)와 비슷한 multi-repo 관리
- CMake wrapper
- Flash/debug 도구 통합

### 2.5 Zephyr 핵심 API (Linux 대응표)

| Linux | Zephyr | 설명 |
|---|---|---|
| `pthread_create()` | `k_thread_create()` | 스레드 생성 |
| `pthread_mutex_lock()` | `k_mutex_lock()` | 뮤텍스 |
| `sem_post() / sem_wait()` | `k_sem_give() / k_sem_take()` | 세마포어 |
| `mq_send() / mq_receive()` | `k_msgq_put() / k_msgq_get()` | 메시지 큐 |
| `usleep() / nanosleep()` | `k_sleep(K_MSEC(n))` | 슬립 |
| `clock_gettime()` | `k_uptime_get()` | 시간 (ms) |
| `printk()` | `LOG_INF() / LOG_ERR()` | 로깅 |
| `ioctl(fd, SPI_IOC_MESSAGE)` | `spi_transceive()` | SPI 통신 |
| `open("/dev/gpiochipN")` | `gpio_pin_configure_dt()` | GPIO 설정 |

### 2.6 Zephyr 로깅 — printk 대신

```c
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor, LOG_LEVEL_INF);  /* 모듈 등록 */

LOG_INF("센서 초기화 완료");                    /* = printk + 타임스탬프 + 모듈명 */
LOG_ERR("SPI 통신 실패: %d", ret);             /* 에러 */
LOG_WRN("FIFO 오버플로우");                    /* 경고 */
LOG_DBG("레지스터 값: 0x%02X", val);           /* 디버그 (릴리즈에서 제거) */

/* 출력 예:
 * [00:00:00.123,456] <inf> sensor: 센서 초기화 완료
 * [00:00:00.234,567] <err> sensor: SPI 통신 실패: -5
 */
```

### 2.7 에러 처리 컨벤션

```c
/* Linux 커널과 동일: 0 = 성공, 음수 errno = 실패 */
int sensor_init(void)
{
    int ret;

    ret = spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);
    if (ret < 0) {
        LOG_ERR("SPI transceive failed: %d", ret);
        return ret;    /* 에러 전파 */
    }

    return 0;  /* 성공 */
}

/* 호출부 */
ret = sensor_init();
if (ret < 0) {
    LOG_ERR("Sensor init failed: %d", ret);
    /* 복구 또는 중단 */
}
```

---

## Part 3: SPI 통신 기초

### 3.1 SPI란?

Embedded Linux에서도 SPI를 사용하지만, 보통 `/dev/spidevN.M` + `ioctl()`로 추상화되어 있습니다. MCU에서는 SPI 프로토콜을 더 직접적으로 다룹니다.

```
SPI 버스 (4선):
  SCK  — 클럭 (마스터가 생성)
  MOSI — Master Out, Slave In (마스터 → 센서)
  MISO — Master In, Slave Out (센서 → 마스터)
  CS   — Chip Select (어떤 센서와 통신할지 선택, Active Low)

SpotOn 구성:
  nRF54L15 (마스터) ─── SCK/MOSI/MISO (공유) ─┬─ CS0 → ICM-42688-P
                                               └─ CS1 → ADXL372
```

### 3.2 SPI 레지스터 읽기/쓰기

센서는 내부에 "레지스터"를 가지고 있습니다. 각 레지스터는 주소(1 byte)를 가지고 있고, SPI를 통해 읽거나 씁니다.

```
레지스터 읽기 (ICM-42688-P WHO_AM_I):
  TX: [0x75 | 0x80]  [0x00]     ← 0x80 = 읽기 비트 (MSB)
  RX: [garbage]       [0x47]     ← 응답: WHO_AM_I = 0x47

레지스터 쓰기 (ICM-42688-P PWR_MGMT0):
  TX: [0x4E]  [0x0F]            ← 주소 + 값
  RX: (무시)                     ← 쓰기라 응답 없음
```

Zephyr 코드:
```c
int icm42688_read_reg(uint8_t reg, uint8_t *val)
{
    uint8_t tx[2] = { reg | 0x80, 0x00 };  /* 읽기: MSB=1 */
    uint8_t rx[2] = { 0 };

    struct spi_buf tx_buf = { .buf = tx, .len = 2 };
    struct spi_buf rx_buf = { .buf = rx, .len = 2 };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

    int ret = spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);
    if (ret < 0) return ret;

    *val = rx[1];  /* 첫 바이트는 garbage, 두 번째가 데이터 */
    return 0;
}
```

### 3.3 FIFO — 센서의 데이터 버퍼

센서가 800Hz로 측정하면 1.25ms마다 데이터가 생깁니다. MCU가 매번 읽으면 CPU 낭비.
해결: 센서 내부 **FIFO 버퍼**에 모아두고, 한 번에 읽기 (burst read).

```
ICM-42688-P FIFO (2KB):
  800Hz × 16B/sample = 12.8KB/s
  FIFO에 ~128 패킷 저장 가능
  Watermark = 64 패킷 → 인터럽트 발생 → MCU가 한 번에 1KB 읽기

시간흐름:
  0ms ────── 80ms ────── 160ms ────── 240ms
  [데이터 쌓임]   [INT!]   [데이터 쌓임]   [INT!]
                  │                        │
                  └→ burst read 1KB         └→ burst read 1KB
```

Linux 비유: DMA ring buffer + poll()/epoll() 패턴과 유사

### 3.4 Burst Read — 한 번에 많이 읽기

```c
/* 일반 읽기: 레지스터 1개 → SPI 트랜잭션 1개 */
icm42688_read_reg(REG_FIFO_DATA, &val);  /* 1바이트 */

/* Burst 읽기: FIFO 전체 → SPI 트랜잭션 1개 */
uint8_t tx[1025] = { FIFO_DATA_REG | 0x80 };  /* 주소 + 1024 dummy bytes */
uint8_t rx[1025];                               /* 1바이트 주소 + 1024 데이터 */
/* spi_transceive() 한 번으로 1KB 읽기 — 매우 효율적 */
```

이것이 바로 "Option A: Raw SPI"를 선택한 이유:
Zephyr 드라이버 중 일부는 FIFO를 한 샘플씩 읽는 비효율적 구현이 있음.

---

## Part 4: nRF54L15 하드웨어

### 4.1 MCU 개요

```
nRF54L15 — Nordic Semiconductor
  CPU: ARM Cortex-M33, 128MHz
  RAM: 256KB
  RRAM: 1,524KB (Flash 대체, 비휘발성)
  BLE: 5.4 (내장 라디오)
  GPIO: 31핀 (2개 포트)
  SPI: 여러 인스턴스 (SPIM00, SPIM20, SPIM21, SPIM22)
  ADC: 14-bit, 8채널
  패키지: QFN48, 6×6mm
```

Linux 비유:
```
Linux SoC (i.MX8, AM62x 등):
  - CPU: GHz급 A-core + M-core
  - RAM: GB 단위 (DDR)
  - Storage: eMMC/SD 수십 GB
  - 주변기기: 수십 개 IP 블록

nRF54L15:
  - CPU: 128MHz M-core 1개
  - RAM: 256KB (SRAM)
  - Storage: 1.5MB (RRAM, 코드+데이터 공존)
  - 주변기기: SPI, I2C, UART, GPIO, ADC, BLE radio
  → Linux SoC의 M-core만 따로 떼어놓은 것과 비슷
```

### 4.2 RRAM (Resistive RAM) — Flash와 다른 점

```
일반 NOR Flash:
  쓰기 전 반드시 지우기 (erase-before-write)
  지우기 단위: 섹터 (4KB~64KB)
  쓰기 단위: 페이지 (256B)
  수명: ~100,000 erase cycles

nRF54L15 RRAM:
  지우기 없이 바로 쓰기 가능!
  쓰기 단위: word (4B)
  수명: ~10,000 write cycles (Flash보다 낮음)
  쓰기 속도: 22µs (Flash보다 빠름)

→ ZMS (Zephyr Memory Storage)를 사용하는 이유:
  RRAM 특성에 맞는 저장 서브시스템 (FCB/NVS는 NOR Flash용)
```

### 4.3 nRF54L15-DK 보드

```
nRF54L15-DK (개발 키트):
  ┌─────────────────────────────────────────┐
  │  nRF54L15 칩                             │
  │  + J-Link OB (온보드 디버거/프로그래머)     │
  │  + USB-C (전원 + 시리얼 콘솔)              │
  │  + LED 4개 (GPIO로 제어)                  │
  │  + 버튼 4개                               │
  │  + 핀 헤더 (P0, P1, P2 커넥터)            │
  └─────────────────────────────────────────┘

Linux 비유:
  라즈베리파이 = SoC + USB + HDMI + GPIO 헤더
  nRF54L15-DK = MCU + J-Link + USB + GPIO 헤더
```

### 4.4 GPIO와 핀 도메인

```
nRF54L15 GPIO 포트:
  Port 0: P0.00 ~ P0.10 (11핀)
  Port 1: P1.00 ~ P1.15 (16핀)
  Port 2: P2.00 ~ P2.10 (11핀) — DK에서 QSPI Flash에 연결됨

주의사항:
  - SPI SCK는 "전용 클럭 핀"을 사용해야 함
  - 일부 주변기기는 특정 포트에서만 동작
    (SPIM00 → Port 2, SPIM20/21 → Port 0/1)
  - DK의 Port 2는 QSPI Flash에 연결 → 솔더 브리지 변경 필요

SpotOn 핀 배치 (SPIM21, Port 1):
  P1.09 = SCK
  P1.10 = MOSI
  P1.11 = MISO
  P1.12 = CS0 (ICM-42688-P)
  P1.13 = CS1 (ADXL372)
  P1.14 = INT0 (ICM FIFO watermark)
  P1.15 = INT1 (ADXL372 FIFO watermark)
```

---

## Part 5: 센서 이해

### 5.1 ICM-42688-P (6축 IMU)

```
역할: 테니스 라켓의 진동 패턴 분석 + 자이로 (스윙 각도/속도)
제조사: TDK InvenSense
측정:
  가속도계: ±16g, 3축 (X, Y, Z)
  자이로: ±2000°/s, 3축 (X, Y, Z)
  → 6축 (accel 3 + gyro 3)

데이터 흐름:
  충격 발생 → 라켓 진동 → IMU가 800Hz로 진동 측정 → FIFO에 저장
  → INT 핀 → MCU가 FIFO burst read → ML 추론 → 충격 위치 추정

핵심 레지스터:
  WHO_AM_I (0x75) = 0x47  ← "나 ICM-42688-P 맞아" (통신 확인용)
  PWR_MGMT0 (0x4E)        ← 전원 모드 (가속도계+자이로 켜기)
  ACCEL_CONFIG0 (0x50)     ← 범위 ±16g, ODR 800Hz 설정
  FIFO_CONFIG (0x16)       ← FIFO 스트림 모드
  FIFO_DATA (0x30)         ← FIFO에서 데이터 읽기

Anti-Aliasing Filter (AAF):
  프로그래밍 가능한 2차 필터 (BMI270에는 없는 기능)
  800Hz 샘플링 → Nyquist = 400Hz
  AAF를 ~350Hz로 설정 → 400Hz 이상 신호 차단 → ML 정확도 향상
  설정은 Bank 2 레지스터 (REG_BANK_SEL로 전환 필요)
```

### 5.2 ADXL372 (고g 가속도계)

```
역할: 테니스 공 충격의 피크 값 측정 (50~150g)
제조사: Analog Devices
측정:
  가속도계: ±200g, 3축 (X, Y, Z)
  해상도: 100mg/LSB (12-bit)

왜 두 개의 가속도계?
  ICM-42688-P: ±16g 범위 → 충격 순간 포화 (클리핑)
  ADXL372: ±200g 범위 → 충격 피크 정확히 측정

시간 축 데이터:
  -10ms          0ms (충격)        +140ms
  ─────────────────────────────────────────
  ICM:   스윙 gyro    │ 포화(clipping) │ ★ 진동 패턴 → ML
  ADXL:              │ ★ 피크 50-150g  │ 감쇠

핵심 레지스터:
  DEVID_AD (0x00) = 0xAD  ← "나 Analog Devices 센서야"
  PARTID (0x02) = 0xFA    ← "나 ADXL372야"
  TIMING (0x3D)            ← ODR 800Hz 설정
  MEASURE (0x3F)           ← 측정 모드 (normal, NOT peak detect)
  FIFO_CTL (0x3A)          ← FIFO 스트림 모드

FIFO 구조 (ICM과 다름!):
  ICM FIFO: 패킷 단위 (헤더 + 6축 데이터 = 16B)
  ADXL FIFO: 축 단위 (1축 = 2B, 3축 = 6B = 3 FIFO entries)
  512 entries / 3축 = 170개 XYZ 샘플 = 213ms 버퍼
```

### 5.3 두 센서의 역할 분담

```
타임라인: 공이 라켓에 맞는 순간

시간     ADXL372 (±200g)              ICM-42688-P (±16g + gyro)
───────────────────────────────────────────────────────────────
-250ms                                스윙 gyro → 샷 분류 (FH/BH/SV)
-100ms                                스윙 gyro → 면 각도, 스윙 속도
-10ms                                 스윙 accel (아직 ±16g 이내)
 0ms    ★ 충격 피크 (50-150g)         포화 (16g clipping) — 무시
+5ms    충격 끝남                      ★ 진동 시작 (±16g 이내로 복귀)
+10ms~                                ★ 현 진동 패턴 → 위치 추정 ML
+140ms                                진동 감쇠

정리:
  ADXL372: "얼마나 세게 맞았나" (충격 강도, 지속시간)
  ICM-42688-P: "어디에 맞았나" (진동 패턴), "어떻게 쳤나" (스윙)
```

---

## Part 6: SpotOn 프로젝트 구조

### 6.1 전체 시스템 아키텍처

```
┌─────────────────────────────────────────────────────┐
│                    SpotOn 시스템                      │
│                                                      │
│  ┌─────────────────┐        ┌──────────────────┐    │
│  │  펌웨어 (Zephyr)  │  BLE  │  앱 (Flutter)     │    │
│  │  nRF54L15        │ ◄───► │  Android/iOS      │    │
│  │                  │       │                    │    │
│  │  센서 → ML → 저장 │       │  히트맵 + 통계     │    │
│  └─────────────────┘        └──────────────────┘    │
│                                                      │
│  하드웨어: ICM-42688-P + ADXL372 + LiPo 배터리       │
│  폼팩터: 테니스 댐프너 (30×28×12mm, ~10g)             │
└─────────────────────────────────────────────────────┘
```

### 6.2 펌웨어 데이터 흐름

```
센서 → 스레드 → ML → 저장 → BLE 전송:

ICM-42688-P ──► sensor_thread ──► imu_q ──► ml_thread ──► shot_q ──► session_thread
ADXL372     ──►   (P2, 2KB)     (4slots    (P5, 4KB)    (32slots    (P6, 2KB)
  │                 │            ×4.5KB)      │           ×20B)        │
  INT핀             │                         │                       │
  └→ GPIO ISR       │                         │                       │
     └→ k_sem_give  │                         │                       │
        └→ 깨어남    │                         │                       ▼
                    FIFO burst               features              ZMS 저장
                    read (SPI)               → SVR/RF              + BLE sync
                                             → 위치 추정
                                             → 샷 분류
```

### 6.3 상태 머신 (State Machine)

```
테니스 한 세트 동안의 흐름:

SLEEP ──(라켓 집어들기)──► ARMED ──(첫 타구)──► ACTIVE
  ↑                         │                      │
  │                    (2분 무응답)            (타구마다 ML 처리)
  │                         │                      │
  │                         ▼                  (5분 무타구)
  │                       SLEEP                    │
  │                                                ▼
  └─────────────(동기화 완료)────────────── SYNC ◄──┘
                                            │
                                         (앱 연결)
                                         (세션 데이터 전송)
                                         (완료 → SLEEP)

각 상태의 전력:
  SLEEP:  ~27µA  (몇 달 대기 가능)
  ARMED:  ~100µA (라켓 들고 있지만 아직 안 침)
  ACTIVE: ~25mA  (800Hz 센서 + ML 추론)
  SYNC:   ~5mA   (BLE 전송)
```

### 6.4 ML 파이프라인 (간략)

```
1. 충격 감지 (ADXL372 > 30g)
2. 데이터 윈도우 캡처:
   - 250ms 사전 (ICM: 200 samples × 6축) → 샷 분류, 스윙 속도, 면 각도
   - 140ms 사후 (ICM: 112 samples × 6축) → 위치 추정 특징
   - 150ms ADXL (120 samples × 3축) → 충격 특징

3. 특징 추출 (39개):
   - ADXL: 피크 진폭, 충격 지속시간, 에너지
   - ICM: 감쇠율, 영교차 횟수, RMS, 주파수 에너지 (Goertzel)
   - Gyro: 크기, 축 비율

4. SVR 추론 → (x, y) 위치 추정 (mm)
5. RF 추론 → 샷 분류 (포핸드/백핸드/서브/발리/슬라이스)
6. 보정 적용 → 최종 shot_event (20 bytes) 저장
```

---

## Part 7: 개발 워크플로우

### 7.1 기본 사이클

```bash
# 1. 코드 편집
vim src/sensor.c

# 2. 빌드 (Linux의 make에 해당)
cd ~/ncs && west build -b nrf54l15dk/nrf54l15/cpuapp ~/spoton

# 3. 플래시 (DK에 다운로드)
west flash

# 4. 시리얼 모니터 (Linux의 minicom/picocom과 동일)
minicom -D /dev/ttyACM0 -b 115200

# 5. 로그 확인
# [00:00:00.000,000] <inf> spoton: SpotOn Phase 0 — project scaffold
# [00:00:00.001,234] <inf> spoton: LED initialized, starting blink loop
```

### 7.2 디버깅

```bash
# GDB 디버깅 (J-Link 경유)
west debug

# 또는 VS Code + nRF Connect Extension (GUI)

# 실시간 변수 확인, 브레이크포인트, 스텝 실행 가능
# Linux의 JTAG/OpenOCD 디버깅과 유사
```

### 7.3 클린 빌드

```bash
# 문제가 생기면 pristine build (Linux의 make clean + make)
cd ~/ncs && west build -b nrf54l15dk/nrf54l15/cpuapp ~/spoton --pristine
```

---

## Part 8: 추천 학습 순서

DK가 도착하기 전에 읽어볼 자료:

### Level 1: Zephyr 기본 (1-2일)
1. [Zephyr Getting Started](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) — 이미 설치 완료
2. [Zephyr Blinky 예제](https://docs.zephyrproject.org/latest/samples/basic/blinky/README.html) — 우리 scaffold와 동일
3. [Device Tree 가이드](https://docs.zephyrproject.org/latest/build/dts/index.html) — Linux DT 경험이면 빠르게 이해

### Level 2: 주변기기 (2-3일)
4. [SPI 드라이버 가이드](https://docs.zephyrproject.org/latest/hardware/peripherals/spi.html)
5. [GPIO 인터럽트](https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html)
6. [로깅 시스템](https://docs.zephyrproject.org/latest/services/logging/index.html)

### Level 3: 스레딩 (1-2일)
7. [Zephyr 커널 서비스](https://docs.zephyrproject.org/latest/kernel/services/index.html) — 스레드, 세마포어, 메시지 큐
8. [Zephyr 스레딩 예제](https://docs.zephyrproject.org/latest/samples/basic/threads/README.html)

### Level 4: 센서 데이터시트 (2-3일)
9. ICM-42688-P 데이터시트 (DS-000347) — 특히 §5 FIFO, §7 레지스터 맵
10. ADXL372 데이터시트 — 특히 FIFO 섹션, 레지스터 맵

### Level 5: BLE (Phase 2 때)
11. [Zephyr BLE 가이드](https://docs.zephyrproject.org/latest/connectivity/bluetooth/index.html)
12. [Nordic BLE 기초](https://academy.nordicsemi.com/courses/bluetooth-low-energy-fundamentals/)

---

## 용어 대응표

| 한국어 | 영어 | Linux 대응 |
|--------|------|-----------|
| 개발 키트 (DK) | Development Kit | 라즈베리파이, BeagleBone |
| 플래시 | Flash | dd, fastboot |
| 레지스터 | Register | /sys/class 또는 ioctl |
| 인터럽트 | Interrupt | IRQ (커널에서 처리) |
| 세마포어 | Semaphore | sem_t (POSIX) |
| 메시지 큐 | Message Queue | mqueue (POSIX) |
| 워치독 | Watchdog | /dev/watchdog |
| 디바이스 트리 | Device Tree | .dts/.dtsi/.dtbo (동일!) |
| Kconfig | Kconfig | .config (동일!) |
| 풋프린트 | Footprint | 코드 크기 (size 명령) |
| 보드 | Board | 타겟 플랫폼 (MACHINE) |
