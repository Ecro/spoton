---
globs:
  - "src/**/*.c"
  - "src/**/*.h"
  - "boards/**"
  - "**/*.overlay"
  - "prj.conf"
  - "Kconfig"
  - "CMakeLists.txt"
---

# C / Zephyr Coding Patterns

## Language
- C99 standard
- No dynamic allocation (no malloc/free/calloc/realloc)
- All buffers statically allocated or stack-allocated
- Use `__packed` for wire/storage structs

## Zephyr APIs
- Threads: `K_THREAD_STACK_DEFINE`, `k_thread_create`
- Message queues: `K_MSGQ_DEFINE`, `k_msgq_put`, `k_msgq_get`
- Semaphores: `k_sem_give`, `k_sem_take`
- Timers: `k_timer_start`, `k_timer_status_get`
- Sleep: `k_sleep(K_MSEC(n))`, `k_usleep(n)`
- Uptime: `k_uptime_get()` (ms), `k_cycle_get_32()` (cycles)

## SPI
- Device tree binding for pin assignment
- `spi_transceive` for full-duplex
- `spi_write` / `spi_read` for half-duplex
- CS managed via `spi_cs_control` in DT

## Logging
```c
LOG_MODULE_REGISTER(module_name, LOG_LEVEL_INF);
LOG_INF("message %d", value);
LOG_ERR("error: %d", ret);
LOG_WRN("warning");
LOG_DBG("debug only");
```

## Error Handling
- Return 0 for success, negative errno for failure
- Check every return value from Zephyr APIs
- Use `__ASSERT` for development-only invariants
- Never panic in production paths

## Naming
- Functions: `module_verb_noun()` (e.g., `sensor_read_fifo()`)
- Constants: `UPPER_SNAKE_CASE`
- Types: `struct module_thing` (no typedef for structs)
- Macros: `UPPER_SNAKE_CASE`
- File-local: `static` keyword

## Configuration
- Kconfig for compile-time options (`CONFIG_SPOTON_*`)
- Device tree for hardware pin/peripheral mapping
- `prj.conf` for project-level Kconfig
- `.overlay` for board-specific DT overrides

## Interrupt Safety
- ISR handlers: minimal work, use `k_msgq_put` or `k_sem_give` to signal threads
- Never call blocking APIs from ISR context
- Use `irq_lock()` / `irq_unlock()` for critical sections (sparingly)

## RRAM Storage (ZMS)
- ZMS: Zephyr Memory Storage for both shot events and calibration/config
- RRAM does not require erase-before-write (unlike NOR Flash)
- ~10,000 write cycle endurance per cell — design for wear distribution
- Always check storage write return codes
