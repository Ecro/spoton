# /test - Run Tests

Run tests and verification for the SpotOn project.

## Model Configuration

```
model: opus
thinking: ultrathink
```

## Usage

```
/test $ARGUMENTS
```

## Arguments

`$ARGUMENTS` - Test mode: "build", "flash", "resource", "unit", "app", "full", or blank for build test

## Test Modes

### build (default)

Build firmware and check for errors:

```bash
west build -b nrf54l15dk/nrf54l15
echo "Build exit code: $?"
```

### pristine

Clean build from scratch:

```bash
west build -b nrf54l15dk/nrf54l15 --pristine
```

### resource

Check Flash and RAM usage against budgets:

```bash
west build -b nrf54l15dk/nrf54l15
west build -t rom_report
west build -t ram_report
```

**Budget Check:**
```markdown
| Resource | Used | Budget | Status |
|----------|------|--------|--------|
| RRAM (Slot 0) | {N} KB | 400 KB | OK/WARN/OVER |
| RAM | {N} KB | 256 KB | OK/WARN/OVER |
| ML Models | {N} KB | ~14 KB | OK/WARN |
```

### unit

Run Zephyr unit tests (native_posix):

```bash
west build -b native_posix -t run
```

### flash

Build and flash to DK:

```bash
west build -b nrf54l15dk/nrf54l15 && west flash
```

Then monitor:
```bash
minicom -D /dev/ttyACM0 -b 115200
```

### app

Run Flutter app tests:

```bash
cd spoton-app && flutter test
cd spoton-app && flutter analyze
```

### full

Run everything:

```bash
# 1. Firmware build
west build -b nrf54l15dk/nrf54l15 --pristine

# 2. Resource check
west build -t rom_report
west build -t ram_report

# 3. Unit tests (if available)
west build -b native_posix -t run 2>/dev/null || echo "No unit tests configured"

# 4. Flutter (if app exists)
if [ -d "spoton-app" ]; then
    cd spoton-app && flutter test && flutter analyze
fi
```

### hardware

Generate a hardware test checklist based on current changes:

```markdown
## Hardware Test Checklist

### Sensor Tests
- [ ] ICM-42688-P SPI communication (read WHO_AM_I register)
- [ ] ADXL372 SPI communication (read DEVID register)
- [ ] ICM-42688-P FIFO watermark interrupt fires
- [ ] ADXL372 FIFO watermark interrupt fires
- [ ] 800Hz ODR verified (timing measurement)

### Impact Detection
- [ ] Tap racket → impact detected in serial log
- [ ] Threshold appropriate (no false positives from handling)
- [ ] Capture window timing correct (150ms)

### State Machine
- [ ] SLEEP → ARMED on motion
- [ ] ARMED → ACTIVE on first impact
- [ ] ACTIVE → SYNC after 5min idle
- [ ] SYNC → SLEEP after transfer

### BLE
- [ ] Advertising visible on phone
- [ ] Connection successful
- [ ] Session data transfer complete
- [ ] Time sync received

### Power
- [ ] SLEEP current measurement: target TBD (pending nRF54L15 datasheet verification)
- [ ] ACTIVE current: target TBD (pending nRF54L15 datasheet verification)
- [ ] Battery voltage reading correct
- [ ] NTC temperature reading reasonable
```

## Output Format

```markdown
## Test Results: {mode}

| Test | Result | Details |
|------|--------|---------|
| {test} | PASS/FAIL | {details} |

### Next Steps
{recommendations based on results}
```
