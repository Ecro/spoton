/*
 * SpotOn — Project Configuration
 *
 * Constants, thresholds, and compile-time configuration.
 * Hardware-specific values (pins, peripherals) are in the device tree overlay.
 */

#ifndef SPOTON_CONFIG_H
#define SPOTON_CONFIG_H

/* Sensor ODR */
#define SPOTON_SENSOR_ODR_HZ        800

/* Impact detection */
#define SPOTON_IMPACT_THRESHOLD_MG  30000   /* 30g — ADXL372 threshold */
#define SPOTON_IMPACT_DEBOUNCE_MS   500     /* Ignore impacts within this window */

/* State machine timeouts */
#define SPOTON_ARMED_TIMEOUT_MS     (2 * 60 * 1000)   /* 2 min no impact → SLEEP */
#define SPOTON_ACTIVE_TIMEOUT_MS    (5 * 60 * 1000)   /* 5 min no impact → end session */

/* SPI bus speed (limited by ADXL372 max 10MHz) */
#define SPOTON_SPI_FREQ_HZ          10000000

/* ICM-42688-P registers (Bank 0) */
#define ICM42688_REG_WHO_AM_I       0x75
#define ICM42688_WHO_AM_I_VALUE     0x47
#define ICM42688_REG_PWR_MGMT0     0x4E
#define ICM42688_REG_GYRO_CONFIG0  0x4F
#define ICM42688_REG_ACCEL_CONFIG0 0x50
#define ICM42688_REG_FIFO_CONFIG   0x16
#define ICM42688_REG_FIFO_CONFIG1  0x5F
#define ICM42688_REG_INT_STATUS    0x2D
#define ICM42688_REG_FIFO_COUNTH   0x2E
#define ICM42688_REG_FIFO_COUNTL   0x2F
#define ICM42688_REG_FIFO_DATA     0x30
#define ICM42688_REG_BANK_SEL      0x76

/* ADXL372 registers */
#define ADXL372_REG_DEVID_AD       0x00
#define ADXL372_DEVID_AD_VALUE     0xAD
#define ADXL372_REG_DEVID_MST      0x01
#define ADXL372_REG_PARTID         0x02
#define ADXL372_PARTID_VALUE       0xFA
#define ADXL372_REG_STATUS         0x04
#define ADXL372_REG_FIFO_CTL       0x3A
#define ADXL372_REG_TIMING         0x3D
#define ADXL372_REG_MEASURE        0x3F

#endif /* SPOTON_CONFIG_H */
