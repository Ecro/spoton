/*
 * SpotOn — Dual Sensor Driver (Raw SPI)
 *
 * ICM-42688-P (6-axis IMU) + ADXL372 (high-g accelerometer)
 * Both on shared SPI bus (SPIM21), separate CS lines.
 *
 * Architecture: Raw SPI register access (Option A).
 * Zephyr upstream drivers not used because:
 *   - ADXL372 driver lacks FIFO watermark trigger
 *   - ICM42688 driver has known bugs (#84833, #88846)
 *   - ICM42688 AAF Bank 2 config not supported by driver
 */

#ifndef SPOTON_SENSOR_H
#define SPOTON_SENSOR_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

/* ICM-42688-P FIFO packet (16-bit mode, accel+gyro+header) */
struct icm42688_fifo_packet {
	uint8_t header;
	int16_t accel[3];   /* XYZ, ±16g, 2048 LSB/g */
	int16_t gyro[3];    /* XYZ, ±2000dps, 16.4 LSB/dps */
	uint8_t temp;       /* Temperature (optional, verify DS-000347 §5.2) */
} __packed;

/* ADXL372 FIFO sample (3-axis, normal mode) */
struct adxl372_fifo_sample {
	int16_t accel[3];   /* XYZ, ±200g, 100mg/LSB */
} __packed;

/*
 * Sensor initialization.
 * Configures SPI bus, reads WHO_AM_I/DEVID, sets ODR/range/FIFO/AAF.
 * Returns 0 on success, negative errno on failure.
 */
int sensor_init(void);

/*
 * ICM-42688-P operations.
 */
int icm42688_read_reg(uint8_t reg, uint8_t *val);
int icm42688_write_reg(uint8_t reg, uint8_t val);
int icm42688_read_who_am_i(uint8_t *id);
int icm42688_configure(void);          /* ODR, range, FIFO, AAF */
int icm42688_read_fifo_count(uint16_t *count);
int icm42688_read_fifo(struct icm42688_fifo_packet *buf, uint16_t max_packets,
		       uint16_t *packets_read);

/*
 * ADXL372 operations.
 */
int adxl372_read_reg(uint8_t reg, uint8_t *val);
int adxl372_write_reg(uint8_t reg, uint8_t val);
int adxl372_read_devid(uint8_t *id);
int adxl372_configure(void);           /* ODR, bandwidth, FIFO, normal mode */
int adxl372_read_fifo_count(uint16_t *count);
int adxl372_read_fifo(struct adxl372_fifo_sample *buf, uint16_t max_samples,
		      uint16_t *samples_read);

/*
 * Interrupt status checks (called from sensor_thread after semaphore wake).
 */
bool icm42688_fifo_overflow(void);
bool adxl372_fifo_overflow(void);

/*
 * FIFO flush (call after overflow detected).
 */
int icm42688_fifo_flush(void);
int adxl372_fifo_flush(void);

#endif /* SPOTON_SENSOR_H */
