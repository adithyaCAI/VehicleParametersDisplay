# Requirement traceability

Generated 2026-11-07T11:24:54.393583400+00:00
Specification frozen 2026-11-03T07:23:29.050Z

53 of 53 requirements have code recorded against them — 0 with tests passing, 0 with tests written but not passing, 53 implemented with no test at all. 0 have nothing recorded.

**Host unit tests.** No host unit-test results have been imported for this project; nothing here rests on host test evidence.

Everything below is derived from what the agent recorded with `requirement_trace` and from the frozen specification. A blank cell means nothing was recorded, which is not the same as nothing being needed.

## STM32 Board & Shared I2C Bring-up

gate: reviewed · tasks: 3/3 done · **acceptance criteria were never derived for this category** · recorded in: c1073e05-07b9-4ec5-85ae-6afef95311ac

| Req | Statement | Verified by | Acceptance criteria | Files | Symbols | Tests | Status |
|---|---|---|---|---|---|---|---|
| `HW-001` | The system shall use an STM32-NUCLEO-G070RB development board. | — | — | CMakeLists.txt<br>firmware/bsp/board.h | app_start | — | implemented, untested |
| `HW-005` | The system shall receive 5 V power via micro-USB from a 12 V automotive adapter. | — | — | sources/PRD.md | — | — | implemented, untested |
| `IF-009` | The system shall prevent overlapping transactions on the shared PB8/PB9 communication bus from interfering with MPU6050 or OLED communications. | — | — | firmware/app/app.c<br>firmware/interfaces/i2c_bus.h<br>firmware/platforms/stm32/platform_i2c1.c | app_start, i2c_bus_read_register, i2c_bus_scan, i2c_bus_write, i2c_bus_write_register, platform_i2c1_try_lock, platform_i2c1_unlock | — | implemented, untested |
| `IF-010` | The MPU6050 and OLED SCL pins shall both connect to PB8. | — | — | firmware/configs/app_config.h<br>firmware/platforms/stm32/platform_i2c1.c | HAL_I2C_MspInit, i2c_bus_init | — | implemented, untested |
| `IF-011` | The MPU6050 and OLED SDA pins shall both connect to PB9. | — | — | firmware/configs/app_config.h<br>firmware/platforms/stm32/platform_i2c1.c | HAL_I2C_MspInit, i2c_bus_init | — | implemented, untested |

Observed on the board:
- `HW-001` — NUCLEO-G070RB identified by the connected ST-LINK/device detection; latest deploy targeted this board.
- `IF-009` — The shared I2C bus scan ran; simultaneous MPU6050/OLED client transfers were not demonstrated. Current MPU reads NACK at 0x68 while 0x3C ACKs.
- `IF-010`, `IF-011` — Source config and STM32 adapter configure I2C1 SCL=PB8 and SDA=PB9; the latest bus sweep reported an OLED ACK at 0x3C. Physical sensor wiring was not independently measured.

Notes recorded with these traces:
- `HW-001` — The project configuration and active tool target identify NUCLEO-G070RB.
- `HW-005` — PRD specifies 5 V via micro-USB from a 12 V automotive adapter. The physical external supply path was not inspected.
- `IF-009` — The lock serializes individual bus transactions; both peripheral client services have not yet been integrated together.
- `IF-010`, `IF-011` — PB8/PB9 assignments are grounded in sources/PRD.md and the board pin mapping.

## DHT22 Climate Sensing

gate: reviewed · tasks: 3/3 done · **acceptance criteria were never derived for this category** · recorded in: c1073e05-07b9-4ec5-85ae-6afef95311ac

| Req | Statement | Verified by | Acceptance criteria | Files | Symbols | Tests | Status |
|---|---|---|---|---|---|---|---|
| `HW-002` | The system shall include a DHT22 climate sensor for temperature and humidity. | — | — | firmware/modules/dht22_decoder.c<br>firmware/platforms/stm32/platform_dht22.c<br>firmware/services/climate_service.c | climate_port_init, climate_port_read, climate_service_init, dht22_decoder_decode | — | implemented, untested |
| `IF-001` | The DHT22 VCC shall connect to the STM32-NUCLEO-G070RB 3.3 V pin. | — | — | firmware/configs/app_config.h<br>firmware/platforms/stm32/platform_dht22.c<br>sources/PRD.md | HAL_TIM_Base_MspInit, platform_dht22_set_line_mode | — | implemented, untested |
| `IF-002` | The DHT22 GND shall connect to GND. | — | — | firmware/configs/app_config.h<br>firmware/platforms/stm32/platform_dht22.c<br>sources/PRD.md | HAL_TIM_Base_MspInit, platform_dht22_set_line_mode | — | implemented, untested |
| `IF-003` | The DHT22 OUT shall connect to STM32 pin PB4. | — | — | firmware/configs/app_config.h<br>firmware/platforms/stm32/platform_dht22.c<br>sources/PRD.md | HAL_TIM_Base_MspInit, platform_dht22_set_line_mode | — | implemented, untested |
| `FR-003` | The system shall read temperature in degrees Celsius from the DHT22 every 2 seconds. | — | — | firmware/app/app.c<br>firmware/modules/dht22_decoder.c<br>firmware/platforms/stm32/platform_dht22.c<br>firmware/services/climate_service.c | app_poll_services, climate_service_poll, dht22_decoder_decode, platform_dht22_read_frame | — | implemented, untested |
| `FR-004` | The system shall read humidity in percent from the DHT22 every 2 seconds. | — | — | firmware/app/app.c<br>firmware/modules/dht22_decoder.c<br>firmware/platforms/stm32/platform_dht22.c<br>firmware/services/climate_service.c | app_poll_services, climate_service_poll, dht22_decoder_decode, platform_dht22_read_frame | — | implemented, untested |

Observed on the board:
- `HW-002` — Latest normal deploy monitor reported repeated DHT22 climate samples at 26.9 °C and 100.0% RH. Exact sensor package markings were not inspected.
- `IF-001`, `IF-002`, `IF-003` — DHT22 data I/O on PB4 operated in the latest deploy; PRD assigns VCC=3.3 V and GND=GND, but external power and ground connections were not independently inspected.
- `FR-003`, `FR-004` — Latest 12-second capture showed repeated valid readings of 26.9 °C and 100.0% RH at approximately two-second intervals. Earlier review captured 24.4 °C and 60.6–60.9% RH at the same cadence.

Notes recorded with these traces:
- `IF-001`, `IF-002`, `IF-003` — The DHT22 OUT mapping to PB4 is documented in sources/PRD.md.

## MPU6050 Motion Sensing & Event Detection

gate: reviewed · tasks: 6/6 done · **acceptance criteria were never derived for this category** · recorded in: c1073e05-07b9-4ec5-85ae-6afef95311ac

| Req | Statement | Verified by | Acceptance criteria | Files | Symbols | Tests | Status |
|---|---|---|---|---|---|---|---|
| `HW-003` | The system shall include an MPU6050 IMU with a 3-axis accelerometer and gyroscope. | — | — | firmware/drivers/mpu6050/mpu6050.c<br>firmware/platforms/stm32/platform_motion.c | motion_port_init, mpu6050_init | — | implemented, untested |
| `IF-004` | The MPU6050 VCC shall connect to the STM32-NUCLEO-G070RB 3.3 V pin. | — | — | firmware/configs/app_config.h<br>sources/PRD.md | — | — | implemented, untested |
| `IF-005` | The MPU6050 GND shall connect to GND. | — | — | firmware/configs/app_config.h<br>sources/PRD.md | — | — | implemented, untested |
| `FR-001` | The system shall monitor interior cabin climate and vehicle ride dynamics using the STM32-NUCLEO-G070RB. | — | — | firmware/app/app.c<br>firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | app_report_motion, motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-005` | The system shall read the MPU6050 X, Y, and Z acceleration axes at a ±4 g range and a 100 Hz rate. | — | — | firmware/configs/app_config.h<br>firmware/drivers/mpu6050/mpu6050.c<br>firmware/platforms/stm32/platform_motion.c<br>firmware/services/motion_service.c | motion_port_init, motion_service_poll, mpu6050_init | — | implemented, untested |
| `FR-006` | The system shall convert MPU6050 acceleration readings to G-force. | — | — | firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-007` | At power-up, the system shall keep the board flat and still for 1 second while zeroing the acceleration readings to X = 0 g, Y = 0 g, and Z = 1 g. | — | — | firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-008` | The system shall map the X acceleration axis to lateral motion (cornering). | — | — | firmware/app/app.c<br>firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | app_report_motion, motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-009` | The system shall map the Y acceleration axis to longitudinal motion (acceleration/braking). | — | — | firmware/app/app.c<br>firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | app_report_motion, motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-010` | The system shall map the Z acceleration axis to vertical motion (bumps). | — | — | firmware/app/app.c<br>firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | app_report_motion, motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-013` | The system shall display "Unseen SpeedBreaker" when the Z-axis changes suddenly by 0.30 g or more. | — | — | firmware/app/app.c<br>firmware/configs/app_config.h<br>firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | app_report_motion, motion_processor_event_name, motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-014` | The system shall display "Wide Turn" when the X-axis changes suddenly by 0.25 g or more. | — | — | firmware/app/app.c<br>firmware/configs/app_config.h<br>firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | app_report_motion, motion_processor_event_name, motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-015` | The system shall display "Speeding" when the Y-axis changes suddenly by +0.25 g or more. | — | — | firmware/app/app.c<br>firmware/configs/app_config.h<br>firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | app_report_motion, motion_processor_event_name, motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-016` | The system shall display "Sudden Braking" when the Y-axis changes suddenly by the stated 0.25 g threshold or more. | — | — | firmware/app/app.c<br>firmware/configs/app_config.h<br>firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | app_report_motion, motion_processor_event_name, motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-034` | Before acquiring acceleration data, the system shall configure the MPU6050 to provide readings using the specified ±4 g range and 100 Hz sampling rate. | — | — | firmware/configs/app_config.h<br>firmware/drivers/mpu6050/mpu6050.c<br>firmware/platforms/stm32/platform_motion.c<br>firmware/services/motion_service.c | motion_port_init, motion_service_poll, mpu6050_init | — | implemented, untested |
| `FR-035` | The system shall not evaluate sudden-change event thresholds until the 1-second power-up acceleration calibration is complete. | — | — | firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-036` | The system shall classify an axis value change as sudden when it reaches or exceeds the applicable threshold within a 100 ms detection window. | — | — | firmware/app/app.c<br>firmware/configs/app_config.h<br>firmware/modules/motion_processor.c<br>firmware/services/motion_service.c | app_report_motion, motion_processor_event_name, motion_processor_process, motion_service_poll | — | implemented, untested |
| `FR-033` | For the stated expected use, the system shall stream cabin comfort and ride data to a dashboard for driver-behavior review or ride-comfort tracking. | — | — | firmware/app/app.c<br>firmware/services/motion_service.c | app_report_climate, app_report_motion, app_start | — | implemented, untested |

Observed on the board:
- `HW-003` — Latest full deploy for category 4 still showed I2C ACK at 0x68, WHO_AM_I=0x68 and configured MPU6050 ready; after one-second startup calibration, calibrated motion samples were streamed. The debug probe at adjacent address 0x69 NACKed, as expected for this one-address scan attempt. Earlier NACK-only observations remain unexplained.
- `IF-004`, `IF-005` — User reported VCC about 3.3 V and PRD power/ground wiring. No new independent measurement in category-4 deploy.
- `FR-001`, `FR-008`, `FR-009`, `FR-010` — UART output during the category-4 deploy carried lateral X, longitudinal Y, vertical Z, and DHT22 climate readings.
- `FR-005`, `FR-034` — Deploy reported accel +/-4g, 100Hz, divider 9, and produced motion samples; exact rate was not independently measured with external instrumentation.
- `FR-006`, `FR-007`, `FR-035` — Monitor showed calibration messages through sequence 91 and calibrated samples starting around sequence 101 near X=0, Y=0, Z=1 g; no event appeared during the calibration messages.
- `FR-013`, `FR-014`, `FR-015`, `FR-016`, `FR-036` — Stable motion capture repeatedly reported event=none; no threshold stimulus was applied. Event detection is not hardware-demonstrated.
- `FR-033` — UART capture included motion and climate telemetry; this demonstrates serial output, not a separate graphical dashboard.

## Alert Configuration & LED Signaling

gate: reviewed · tasks: 4/4 done · **acceptance criteria were never derived for this category** · recorded in: c1073e05-07b9-4ec5-85ae-6afef95311ac

| Req | Statement | Verified by | Acceptance criteria | Files | Symbols | Tests | Status |
|---|---|---|---|---|---|---|---|
| `IF-008` | The built-in user LED LD4 shall be connected to PA5. | — | — | firmware/bsp/board.h<br>firmware/configs/app_config.h<br>firmware/platforms/stm32/platform_indicator.c<br>sources/PRD.md | board_led_level, indicator_port_init, indicator_port_set | — | implemented, untested |
| `FR-012` | The system shall make all alert thresholds configurable as macros in config.h. | — | — | firmware/configs/app_config.h<br>firmware/modules/motion_processor.c | motion_processor_process | — | implemented, untested |
| `FR-018` | When any of the four specified events occurs, the system shall blink the built-in LED LD4 with a 500 ms on interval and a 500 ms off interval. | — | — | firmware/app/app.c<br>firmware/configs/app_config.h<br>firmware/interfaces/indicator_port.h<br>firmware/modules/alert_indicator.c<br>firmware/modules/alert_indicator.h<br>firmware/platforms/stm32/platform_indicator.c | alert_indicator_init, alert_indicator_update, app_start, indicator_port_init, indicator_port_set | — | implemented, untested |
| `FR-022` | For the demo threshold, the system shall use 32 degC as the temperature-high limit. | — | — | firmware/configs/app_config.h<br>firmware/modules/motion_processor.c | motion_processor_process | — | implemented, untested |
| `FR-023` | For the demo threshold, the system shall use 75% as the humidity-high limit. | — | — | firmware/configs/app_config.h<br>firmware/modules/motion_processor.c | motion_processor_process | — | implemented, untested |
| `FR-024` | For in-vehicle use, the system shall use G thresholds raised to about 0.4 to 0.5 g. | — | — | firmware/configs/app_config.h<br>firmware/modules/motion_processor.c | motion_processor_process | — | implemented, untested |
| `FR-025` | For in-vehicle use, the system shall use a temperature limit of 45 degC. | — | — | firmware/configs/app_config.h<br>firmware/modules/motion_processor.c | motion_processor_process | — | implemented, untested |
| `FR-026` | For in-vehicle use, the system shall use a humidity limit of 80%. | — | — | firmware/configs/app_config.h<br>firmware/modules/motion_processor.c | motion_processor_process | — | implemented, untested |
| `FR-037` | The event message and LED blinking shall continue for 2 seconds after the last event. | — | — | firmware/app/app.c<br>firmware/configs/app_config.h<br>firmware/interfaces/indicator_port.h<br>firmware/modules/alert_indicator.c<br>firmware/modules/alert_indicator.h<br>firmware/platforms/stm32/platform_indicator.c | alert_indicator_init, alert_indicator_update, app_start, indicator_port_init, indicator_port_set | — | implemented, untested |

Observed on the board:
- `IF-008` — Board BSP code maps LED4 to GPIOA/GPIO_PIN_5 and implements on with GPIO_PIN_SET; user report confirms LED was visibly blinking during the test event. User-visible blinking confirms the LED was driven, but does not independently measure the MCU pin or establish the exact board connector mapping.
- `FR-018`, `FR-037` — User reports a motion event caused LD4 to blink and the indication remained for about two seconds after the last event. The 500/500 ms cadence and two-second duration were judged visually, not timed instrumentally; no event log from that stimulus was captured.

Notes recorded with these traces:
- `FR-012`, `FR-022`, `FR-023`, `FR-024`, `FR-025`, `FR-026` — Default demo profile macros set 32.0 °C, 75.0% RH and demo motion thresholds (0.30/0.25 g); selectable in-vehicle macros set 45 °C, 80% RH and 0.45 g motion thresholds. Profile thresholds are visible in source; profile behavior was not switched/tested at runtime.

## OLED Display & Alert Rendering

gate: reviewed · tasks: 4/4 done · **acceptance criteria were never derived for this category** · recorded in: c1073e05-07b9-4ec5-85ae-6afef95311ac

| Req | Statement | Verified by | Acceptance criteria | Files | Symbols | Tests | Status |
|---|---|---|---|---|---|---|---|
| `HW-004` | The system shall include a 1.3-inch OLED display using the SH1106 driver. | — | — | firmware/configs/app_config.h<br>firmware/drivers/sh1106/sh1106.c<br>firmware/platforms/stm32/platform_oled.c<br>firmware/services/oled_service.c | oled_port_write, oled_service_refresh, sh1106_init | — | implemented, untested |
| `IF-006` | The OLED VDD shall connect to the STM32-NUCLEO-G070RB 3.3 V pin. | — | — | firmware/configs/app_config.h<br>sources/PRD.md | — | — | implemented, untested |
| `IF-007` | The OLED GND shall connect to GND. | — | — | firmware/configs/app_config.h<br>sources/PRD.md | — | — | implemented, untested |
| `FR-002` | The system shall print the monitored data on the OLED display. | — | — | firmware/app/app.c<br>firmware/drivers/sh1106/sh1106.c<br>firmware/services/oled_service.c | app_make_oled_snapshot, app_poll_oled, oled_service_refresh, sh1106_draw_text, sh1106_flush_step | — | implemented, untested |
| `FR-011` | The OLED shall display the heading "Vehicle Parameters Display". | — | — | firmware/app/app.c<br>firmware/drivers/sh1106/sh1106.c<br>firmware/services/oled_service.c | app_make_oled_snapshot, app_poll_oled, oled_service_refresh, sh1106_draw_text, sh1106_flush_step | — | implemented, untested |
| `FR-017` | When any of the four specified events occurs, the system shall show the event message immediately on the OLED while keeping the temperature and humidity values visible. | — | — | firmware/app/app.c<br>firmware/modules/alert_indicator.c<br>firmware/services/oled_service.c | alert_indicator_update, app_poll_oled, oled_service_refresh, oled_service_render | — | implemented, untested |
| `FR-019` | When specified events overlap, the system shall apply this message priority, highest first: Sudden Braking, Unseen SpeedBreaker, Wide Turn, Speeding. | — | — | firmware/app/app.c<br>firmware/modules/alert_indicator.c<br>firmware/services/oled_service.c | alert_indicator_update, app_poll_oled, oled_service_refresh, oled_service_render | — | implemented, untested |
| `FR-020` | If temperature goes above its limit, the system shall show "!" next to the temperature value. | — | — | firmware/configs/app_config.h<br>firmware/services/oled_service.c | oled_service_draw_climate, oled_service_refresh | — | implemented, untested |
| `FR-021` | If humidity goes above its limit, the system shall show "!" next to the humidity value. | — | — | firmware/configs/app_config.h<br>firmware/services/oled_service.c | oled_service_draw_climate, oled_service_refresh | — | implemented, untested |
| `FR-027` | The OLED shall display the heading as "Vehicle Parameters" and "Display" on separate lines, as shown in the OLED display format. | — | — | firmware/app/app.c<br>firmware/drivers/sh1106/sh1106.c<br>firmware/services/oled_service.c | app_make_oled_snapshot, app_poll_oled, oled_service_refresh, sh1106_draw_text, sh1106_flush_step | — | implemented, untested |
| `FR-028` | The OLED display format shall include the temperature value from the DHT22 followed by the unit degC and the label "Temp:". | — | — | firmware/app/app.c<br>firmware/drivers/sh1106/sh1106.c<br>firmware/services/oled_service.c | app_make_oled_snapshot, app_poll_oled, oled_service_refresh, sh1106_draw_text, sh1106_flush_step | — | implemented, untested |
| `FR-029` | The OLED display format shall include the humidity value from the DHT22 followed by the unit % and the label "Humidity:". | — | — | firmware/app/app.c<br>firmware/drivers/sh1106/sh1106.c<br>firmware/services/oled_service.c | app_make_oled_snapshot, app_poll_oled, oled_service_refresh, sh1106_draw_text, sh1106_flush_step | — | implemented, untested |
| `FR-030` | The OLED display format shall include the X-axis acceleration value with the label "X:". | — | — | firmware/app/app.c<br>firmware/drivers/sh1106/sh1106.c<br>firmware/services/oled_service.c | app_make_oled_snapshot, app_poll_oled, oled_service_refresh, sh1106_draw_text, sh1106_flush_step | — | implemented, untested |
| `FR-031` | The OLED display format shall include the Y-axis acceleration value with the label "Y:". | — | — | firmware/app/app.c<br>firmware/drivers/sh1106/sh1106.c<br>firmware/services/oled_service.c | app_make_oled_snapshot, app_poll_oled, oled_service_refresh, sh1106_draw_text, sh1106_flush_step | — | implemented, untested |
| `FR-032` | The OLED display format shall include the Z-axis acceleration value with the label "Z:" and unit g. | — | — | firmware/app/app.c<br>firmware/drivers/sh1106/sh1106.c<br>firmware/services/oled_service.c | app_make_oled_snapshot, app_poll_oled, oled_service_refresh, sh1106_draw_text, sh1106_flush_step | — | implemented, untested |

Observed on the board:
- `HW-004` — Fresh build/flash scan ACKed 0x3C, 0x68 and 0x77. Prior debug deploy reported successful SH1106 initialization writes to 0x3C, but an earlier user logic-analyzer capture reported a NACK at the OLED pins for 0x3C. Latest read-only monitor showed no OLED-specific log/visual evidence. The physical panel/rendering is not confirmed and observations conflict.
- `IF-006`, `IF-007` — PRD specifies OLED VDD=3.3 V and GND=GND; user reports these module wiring checks match. No independent voltage measurement was recorded in this observation.
- `FR-002`, `FR-011`, `FR-027`, `FR-028`, `FR-029`, `FR-030`, `FR-031`, `FR-032` — Firmware is live and continues to stream motion/climate over UART. No visual OLED frame has been observed. The selected heading is two lines, “Vehicle Parameters” and “Display”; source includes Temp/Humidity labels and values, X/Y/Z values and units.
- `FR-017`, `FR-019` — Latest motion capture reported event=none; no OLED visual confirmation or event stimulus occurred. Source consumes the held/priority-selected event and retains climate rows while showing the event label.
- `FR-020`, `FR-021` — Latest monitor climate was 28.5–28.7 °C and 100.0% RH under demo limits of 32 °C/75%; no OLED visual output was observed, so threshold marker rendering is not hardware-confirmed.

## Gaps

Listed rather than omitted. A matrix showing only what was built cannot be told apart from one showing full coverage.

### Implemented, never tested (53)

Code exists and no test is recorded against it.

- `HW-001` (STM32 Board & Shared I2C Bring-up)
- `HW-005` (STM32 Board & Shared I2C Bring-up)
- `IF-009` (STM32 Board & Shared I2C Bring-up)
- `IF-010` (STM32 Board & Shared I2C Bring-up)
- `IF-011` (STM32 Board & Shared I2C Bring-up)
- `HW-002` (DHT22 Climate Sensing)
- `IF-001` (DHT22 Climate Sensing)
- `IF-002` (DHT22 Climate Sensing)
- `IF-003` (DHT22 Climate Sensing)
- `FR-003` (DHT22 Climate Sensing)
- `FR-004` (DHT22 Climate Sensing)
- `HW-003` (MPU6050 Motion Sensing & Event Detection)
- `IF-004` (MPU6050 Motion Sensing & Event Detection)
- `IF-005` (MPU6050 Motion Sensing & Event Detection)
- `FR-001` (MPU6050 Motion Sensing & Event Detection)
- `FR-005` (MPU6050 Motion Sensing & Event Detection)
- `FR-006` (MPU6050 Motion Sensing & Event Detection)
- `FR-007` (MPU6050 Motion Sensing & Event Detection)
- `FR-008` (MPU6050 Motion Sensing & Event Detection)
- `FR-009` (MPU6050 Motion Sensing & Event Detection)
- `FR-010` (MPU6050 Motion Sensing & Event Detection)
- `FR-013` (MPU6050 Motion Sensing & Event Detection)
- `FR-014` (MPU6050 Motion Sensing & Event Detection)
- `FR-015` (MPU6050 Motion Sensing & Event Detection)
- `FR-016` (MPU6050 Motion Sensing & Event Detection)
- `FR-034` (MPU6050 Motion Sensing & Event Detection)
- `FR-035` (MPU6050 Motion Sensing & Event Detection)
- `FR-036` (MPU6050 Motion Sensing & Event Detection)
- `FR-033` (MPU6050 Motion Sensing & Event Detection)
- `IF-008` (Alert Configuration & LED Signaling)
- `FR-012` (Alert Configuration & LED Signaling)
- `FR-018` (Alert Configuration & LED Signaling)
- `FR-022` (Alert Configuration & LED Signaling)
- `FR-023` (Alert Configuration & LED Signaling)
- `FR-024` (Alert Configuration & LED Signaling)
- `FR-025` (Alert Configuration & LED Signaling)
- `FR-026` (Alert Configuration & LED Signaling)
- `FR-037` (Alert Configuration & LED Signaling)
- `HW-004` (OLED Display & Alert Rendering)
- `IF-006` (OLED Display & Alert Rendering)
- `IF-007` (OLED Display & Alert Rendering)
- `FR-002` (OLED Display & Alert Rendering)
- `FR-011` (OLED Display & Alert Rendering)
- `FR-017` (OLED Display & Alert Rendering)
- `FR-019` (OLED Display & Alert Rendering)
- `FR-020` (OLED Display & Alert Rendering)
- `FR-021` (OLED Display & Alert Rendering)
- `FR-027` (OLED Display & Alert Rendering)
- `FR-028` (OLED Display & Alert Rendering)
- `FR-029` (OLED Display & Alert Rendering)
- `FR-030` (OLED Display & Alert Rendering)
- `FR-031` (OLED Display & Alert Rendering)
- `FR-032` (OLED Display & Alert Rendering)

### Categories with no acceptance criteria (5)

Criteria were never derived here, so there is nothing these requirements were judged against. Absent, which is not the same as "nothing to check".

- STM32 Board & Shared I2C Bring-up
- DHT22 Climate Sensing
- MPU6050 Motion Sensing & Event Detection
- Alert Configuration & LED Signaling
- OLED Display & Alert Rendering
