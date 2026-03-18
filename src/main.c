/*
 * SpotOn — Main Entry Point (Phase 0)
 *
 * Phase 0: Blinky + log output to verify toolchain and DK connection.
 * Future phases add sensor threads, ML, state machine, BLE.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "config.h"

LOG_MODULE_REGISTER(spoton, LOG_LEVEL_INF);

/* LED0 from devicetree (DK onboard LED) */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
	int ret;

	LOG_INF("SpotOn Phase 0 — project scaffold");
	LOG_INF("Board: %s", CONFIG_BOARD_TARGET);

	/* Init LED */
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED: %d", ret);
		return ret;
	}

	LOG_INF("LED initialized, starting blink loop");

	/* Blink loop — confirms DK is alive and serial output works */
	while (1) {
		gpio_pin_toggle_dt(&led);
		k_sleep(K_MSEC(500));
	}

	return 0;
}
