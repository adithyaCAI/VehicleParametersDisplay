# Vehicle Parameters Display

## Purpose and current implementation

The NUCLEO-G070RB firmware initializes I2C1 on PB8/SCL and PB9/SDA, scans legal 7-bit addresses, and reports ACKs through the ST-LINK Virtual COM Port at 115200 baud. The MPU6050 at 0x68 is configured for ±4 g and 100 Hz; calibrated X/Y/Z acceleration is streamed with DHT22 climate readings. The SH1106 OLED module configured at user-confirmed 0x3C, 128×64 pixels, shares the same serialized I2C1 bus. The user confirmed the module has no reset pin.

The OLED dashboard renders “Vehicle Parameters” and “Display” on separate lines, DHT22 temperature and humidity with independent threshold markers, calibrated acceleration axes in g, and the active event label. The implementation checks the display every 10 ms and sends at most one 32-byte data chunk per service step so sensor acquisition and other bus clients can run between transfers. New event messages update the event row before the rest of a pending frame. If the display does not acknowledge, the app keeps climate, motion, UART, and LD4 services running and retries the OLED at a paced interval.

The PRD specifies the DHT22 OUT signal on PB4 and MPU6050/OLED connections on PB8/PB9, with sensor power at 3.3 V/GND. It specifies a 5 V micro-USB supply from a 12 V automotive adapter. External module supply/ground and automotive supply wiring have not been independently inspected.

Alert configuration defaults to the PRD demo profile: temperature 32 °C, humidity 75%, and motion thresholds 0.30/0.25 g. Selecting `APP_ALERT_PROFILE_IN_VEHICLE` uses 45 °C, 80% RH, and 0.45 g motion thresholds. During an active motion event and for two seconds after the latest event, LD4 is driven with 500 ms on/off phases.

Previous category-3 deploy evidence showed the MPU6050 ACK at 0x68 and WHO_AM_I=0x68, configured at ±4 g and 100 Hz. Calibration messages persisted through approximately one second and calibrated samples followed near X=0, Y=0, Z=1 g. Earlier deploys intermittently showed no ACK at 0x68; the cause remains unknown. No deliberate event stimulus or OLED hardware display capture is recorded in this source overview yet.

Use the FirmGen STM32 build workflow for the CMake/Ninja firmware. Deploy through the NUCLEO-G070RB ST-LINK; monitor the VCP at 115200 baud. The external automotive supply path and simultaneous MPU6050/OLED I2C traffic are not independently inspected here.

## Module responsibilities

- `firmware/app/app.c` initializes the shared bus, motion/climate services and indicator, creates sensor snapshots, and periodically polls the OLED service.
- `firmware/interfaces/i2c_bus.h` defines the vendor-neutral serialized bus transaction and scan contract.
- `firmware/interfaces/oled_port.h` defines the vendor-neutral SH1106 command/data I2C operation.
- `firmware/platforms/stm32/platform_oled.c` frames SH1106 control bytes and sends bounded transactions through the shared I2C bus API.
- `firmware/drivers/sh1106/sh1106.c` initializes the controller, formats ASCII glyphs into the 128×64 framebuffer, and incrementally flushes the framebuffer.
- `firmware/services/oled_service.c` composes dashboard rows, threshold markers, validity placeholders and active event labels; it rate-limits attempts and recovers from display transfer errors.
- `firmware/modules/alert_indicator.c` retains event priority, the two-second post-event hold and the 500 ms LD4 blink phase without hardware dependencies.
- `firmware/interfaces/indicator_port.h` defines logical LED control; `firmware/platforms/stm32/platform_indicator.c` drives NUCLEO-G070RB LD4 on PA5 using the active-high polarity from the STM32CubeG0 NUCLEO BSP.
- `firmware/services/climate_service.c` enforces the two-second DHT22 sampling interval and maintains sample validity.
- `firmware/drivers/mpu6050/mpu6050.c` checks sensor identity/configuration and decodes coherent motion bursts.
- `firmware/services/motion_service.c` schedules 100 Hz acquisition and coordinates portable motion processing.
- `firmware/modules/motion_processor.c` converts raw acceleration to milli-g, calibrates axes and classifies configured motion events.
- `firmware/platforms/stm32/platform_i2c1.c` configures PB8/PB9 and serializes each I2C transaction.
- `firmware/platforms/stm32/platform_dht22.c` controls PB4 and uses TIM3 for microsecond pulse timing.
- `firmware/platforms/stm32/platform_uart2.c` and `platform_logger.c` route diagnostics to ST-LINK VCP.
- `firmware/configs/app_config.h` holds sensor pins/timings, alert profiles, OLED address/geometry and display pacing constants.

## Evidence status

Build and monitor results are recorded per category in the requirement traces. Category 5 has no derived acceptance criteria; firmware behavior should be reported as implementation and observed evidence, not formal verification. The display’s visible behavior still requires a board observation after deployment.
