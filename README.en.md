# embedded-smart-home

English | [中文](README.md)

An initial graduation-project implementation for **ESP32-S31-Korvo-1 V1.1**, targeting environmental monitoring, manual control, threshold-based automation and hardware/software integration. Start with a demonstrable local system; add networking and voice later.

**Select `esp32s31`, NOT `esp32s3`. These are different chips.**

By default, clearly labelled simulated temperature, humidity and light readings drive the real onboard RGB LED and the shared button/touch controls. No environmental sensors are required for this first demonstration. The board has no onboard room-temperature/humidity/light sensor or fan. Internal chip temperature is not room temperature.

## Peripheral examples

| Resource | Implemented example | Requirements / limits |
| --- | --- | --- |
| WS2812 RGB LED | Follows the light command | GPIO37 owned by the official BSP; BSP default color |
| Four ADC keys | SET: Auto; PLAY: toggle light; VOL+: toggle fan command; VOL−: Manual | Single click; BSP `MODE` corresponds to PLAY |
| LCD + GT1151 touch | Dashboard, command buttons and diagnostic buttons | Original SUB3 800×480 panel; UI can be disabled |
| ES8389 / speakers | 1-second low-volume stereo test tone, 16 kHz | Connect your speaker; runs only on request |
| Dual microphones | Capture 1 second and report per-channel peaks | Raw amplitude, no calibrated dB or speech recognition |
| microSD | Optional `home.csv` append every 10 seconds | Disabled by default; FAT card; never auto-formats |
| DVP / OV3660 | Optional sensor initialization and capability/format probe | Disabled by default; no frame capture, preview or recording |
| Wi-Fi | On-demand scan, print up to 8 APs, stop radio | No network connection, Web server or MQTT |
| PSRAM | 64 KiB pattern write/read test and memory report | A small functional check, not exhaustive validation |
| I2C | Scan shared BSP bus for responding devices | No duplicate bus initialization or guessed pins |
| External SHT3x + BH1750 | Optional real environmental acquisition | Disabled by default; CRC, conversion and range validation |
| External MCP23017 | Optional GPA0 light / GPA1 fan commands | Disabled by default; requires load drivers and separate power |

USB Host applications, Bluetooth, speech recognition and camera streaming are future extensions. Without an external output circuit, the fan exists only as a software command and is shown as unavailable. An ON command is not physical load feedback.

## Environment

- Board: ESP32-S31-Korvo-1 V1.1 and original ESP32-S3-LCD-EV-Board-SUB3 display.
- Host: Windows 10/11, Linux or macOS; use an ESP-IDF-enabled terminal or the official VS Code extension. Console: 115200 baud.
- SDK: **ESP-IDF master preview with `esp32s31` support**. The official support page currently directs S31 developers to master. Older SDK releases are not sufficient just because the BSP declares a low minimum version.
- SDK baseline: `d35ea023029363d97c42565f85e0db23486f26b2` (6.2-dev). See [validation](docs/VALIDATION.md) for actual checks.
- Dependencies: official `espressif/esp32_s31_korvo_1` **1.0.1**, LVGL 9, resolved by IDF Component Manager. `dependencies.lock` records exact resolved versions.
- Toolchain, Python, CMake and Ninja are managed by ESP-IDF. No Arduino, PlatformIO, Linux port or API key is needed.

### Install ESP-IDF

Install host prerequisites from the [official S31 getting-started guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/get-started/index.html), then on Linux/macOS:

```bash
git clone https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout d35ea023029363d97c42565f85e0db23486f26b2
git submodule update --init --recursive
./install.sh esp32s31
. ./export.sh
```

On Windows use the official ESP-IDF setup with S31 support. With a source checkout, select the same commit and update submodules, then run `install.ps1 esp32s31` and `export.ps1` in PowerShell. If the installer has no S31-capable release, use the source checkout rather than choosing the S3 target. Activate the SDK in each new terminal.

### Configure and build

```bash
git clone https://github.com/xuan511/embedded-smart-home.git
cd embedded-smart-home
python tools/check_environment.py
idf.py --preview set-target esp32s31
idf.py --preview menuconfig
idf.py --preview build
```

Defaults are sufficient for the first run. `Smart Home` in menuconfig controls the display, real sensors, SD logging, camera probe and external outputs. `sdkconfig.defaults` seeds new configurations; changing it does not override an existing `sdkconfig`. Back up personal settings before cleaning and moving an old `sdkconfig` to regenerate it.

### Connect and flash

1. Power off before attaching the LCD, optional speaker or card. Start without external loads.
2. Connect a data-capable USB cable to **UART Type-C** and switch the board ON. **Power Type-C is power-only**; Type-A is USB Host.
3. Follow the board guide for a separate power connection when using speakers or other larger loads. Never power a motor from a GPIO.
4. Replace the example port with your actual port:

```powershell
idf.py --preview -p COM5 flash monitor
```

Linux example:

```bash
idf.py --preview -p /dev/ttyUSB0 flash monitor
```

If automatic download fails, hold BOOT, press/release RESET, release BOOT and retry. Exit the monitor with `Ctrl+]`. Expect a `SIMULATED SENSORS` dashboard and a `home>` prompt.

## First demonstration

Commands are queued; wait roughly one second before inspecting their results. A `Queued` message acknowledges receipt only.

```text
home help
home status
home light on
home light off
home mode auto
home sim 30 55 50
home status
home sim 27 55 150
home status
home sim 25 50 300
home status
home fault
home status
home sim 25 50 300
```

Expected results:

- Manual light commands toggle the real RGB LED and switch the entire controller to MANUAL.
- In AUTO, 30°C / 50 lux turns both commands ON.
- 27°C / 150 lux preserves previous outputs inside both hysteresis bands.
- 25°C / 300 lux turns both commands OFF.
- `fault` marks simulated channels invalid and turns their AUTO outputs OFF; a new `sim` restores valid samples.
- MANUAL keeps the requested outputs even when sensor faults are displayed. This is a desktop demonstration policy.

### Commands

| Command | Behavior |
| --- | --- |
| `home status` | Readings, mode, thresholds, faults, output availability and rejected requests |
| `home mode auto/manual` | Select mode; AUTO recalculates on the next controller iteration |
| `home light on/off`, `home fan on/off` | Set manual output and switch the controller to MANUAL |
| `home thresholds 29 27 80 180` | Fan on/off °C, light on/off lux; apply in RAM |
| `home save` | Persist thresholds to NVS; mode/output states are not persisted |
| `home sim 30 55 50` | Inject temperature, humidity and lux; simulated builds only; stays until changed |
| `home fault` | Inject invalid simulated readings |
| `home test i2c` | Scan shared I2C bus |
| `home test memory` | PSRAM functional check |
| `home test tone`, `home test mic` | Speaker tone / microphone peaks |
| `home test camera` | Sensor/format probe; requires `HOME_CAMERA` |
| `home test wifi` | Scan nearby APs; no Wi-Fi password needed |

Buttons and touch use the same bounded message queue. Diagnostics run serially in a separate lower-priority worker, keeping audio/network operations out of UI callbacks. Full queues report failure rather than growing indefinitely.

## Connect real sensors and loads

Read [hardware notes](docs/HARDWARE.md) first. There is no assumed free GPIO header: the LCD, camera, SD and audio already use most routed pins.

1. Verify access to **GPIO0/SDA, GPIO1/SCL, 3.3V and GND** against your actual board schematic. Share this bus with 3.3V SHT3x-DIS and BH1750 modules; do not confuse module pin numbers with GPIO numbers.
2. Use `home test i2c`. SHT defaults to `0x44` (alternative `0x45`); BH1750 to `0x23` (alternative `0x5C`).
3. Enable real sensors under `Smart Home`, configure addresses and rebuild/flash. The UI should say `EXTERNAL SENSORS`, and the console `REAL`.
4. Disconnect one sensor and check its fault/output behavior. The other rule should continue. Real builds reject simulation commands and never silently replace failed readings with simulated values.
5. For a physical fan, add the optional MCP23017, MOSFET driver, flyback protection and independent low-voltage DC power as described in the hardware notes.

## Source layout

| Path | Responsibility |
| --- | --- |
| `docs/ARCHITECTURE.md` | Tasks, data flow, rules and extension interfaces |
| `main/app_main.c` | Startup sequence |
| `components/home_core/` | Portable C state, hysteresis, validity, timeouts and SHT CRC |
| `main/home_service.c` | Controller task, request queue, snapshot, NVS, CSV and console |
| `main/board_io.c` | RGB, ADC keys and optional expander |
| `main/sensors.c` | Optional real SHT3x / BH1750 acquisition |
| `main/ui.c` | LVGL 9 dashboard |
| `main/diagnostics.c` | On-demand peripheral diagnostics |
| `tests/`, `tools/` | Host tests and environment checks |

Suggested reading order: `home_core.h → home_core.c → tests/test_core.c → app_main.c → home_service.c`. For each new device, establish trustworthy serial data first, then automation, then UI.

## Verification and development

With Python 3 and a C compiler, without ESP-IDF or the board:

```bash
python tools/test_host.py
```

Tests cover startup OFF, hysteresis boundaries, independent faults, 5-second expiry, modes, invalid numbers and SHT CRC/decoding. Host tests do not prove that physical hardware works. Build results, hardware acceptance and the next development stages are recorded separately in [validation](docs/VALIDATION.md) and [roadmap](docs/ROADMAP.md).

Recommended sequence: **onboard demonstration → real sensors → real low-voltage loads → threshold/fault integration → continuous-run testing → optional Web/MQTT/voice**. No custom PCB, OTA, cloud platform or AI dependency is required for this baseline.

Troubleshooting:

- **Unknown target**: activate the correct SDK and run `python tools/check_environment.py`; do not substitute `esp32s3`.
- **Dependency download fails**: check access/proxy/certificates for Espressif Component Registry and GitHub before changing constraints.
- **Black screen/resets**: check original panel, connector orientation, power and PSRAM settings; disable UI to isolate with the console.
- **Unexpected key mapping**: verify V1.1 hardware and BSP mapping; do not copy ADC thresholds from another Korvo board.
- **Fan ON but stationary**: the default build has no external driver; check MCP availability. There is no speed/current feedback.
- **No SD logging**: enable the option and insert a FAT card. Write failures stop logging until reboot; the firmware does not format cards.

## References and license

- [Official board guide / schematic links](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)
- [ESP32-S31 software support status](https://developer.espressif.com/hardware/esp32s31/)
- [Official BSP 1.0.1](https://components.espressif.com/components/espressif/esp32_s31_korvo_1/versions/1.0.1)
- [BSP source and examples](https://github.com/espressif/esp-bsp/tree/master/bsp/esp32_s31_korvo_1)

Project code is [MIT licensed](LICENSE). External components retain their own licenses; BSP source is not vendored here.
