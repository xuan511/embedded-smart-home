# embedded-smart-home

English | [中文](README.md)

A two-board graduation project for **Design and Implementation of a Smart Home Environmental Monitoring and Control System**. The goal is a real demonstration: **measure → display → operate → actuate → return status**, with simple wiring and understandable code.

| Board | Responsibility |
| --- | --- |
| ESP32-S31-Korvo-1 V1.1 | Original LCD/touch, debug console, NVS thresholds, optional microSD log |
| STM32F103C8T6 learning board | SHT3x temperature/humidity, BH1750 light, lamp/fan drivers, local automation |
| Link | Korvo **USB-A Host → specified CP2102N USB-UART module → STM32 USART1**, 115200/8N1 |

Select **`esp32s31`**. Do not substitute `esp32s3` configuration or pins. The STM32 linker uses the official **64 KB Flash / 20 KB SRAM** limits. This design does not depend on the unidentified STM32 board's USB circuit. LCD, touch and microSD stay connected; no Korvo GPIO extraction or main-board modification is required.

**No physical boards are connected in this development session. Hardware verification has not been performed.** [VALIDATION](docs/VALIDATION.md) records code completion, successful builds, host tests and hardware results separately. A successful build or test-data display is not a physical measurement. Pass each hardware acceptance stage before connecting the next devices.

## Before connecting hardware

Use the [parts, connectors, power and wiring tables](docs/HARDWARE.md) and [official-source audit](docs/HARDWARE_AUDIT.md). Only three groups of information remain essential for safe, correct wiring/flashing:

1. The actual STM32 board schematic or clear identification: accessible 3V3/GND, PA9/PA10, PB6/PB7, PB0/PB1 and SWD headers; onboard LED pin and polarity.
2. The module's **CP2102N identity, USB VID/PID, UART voltage and header labels**. This implementation does not use CH340 or FT232 drivers. A 3.3V supply pin does not itself prove 3.3V TXD logic.
3. The STM32 board's power input/regulator, lamp/fan rated voltage/current, and whether the drivers guarantee operation with 3.3V control. Avoid simultaneous supplies that can backfeed one another.

Keep the original LCD/touch connected. A FAT microSD is optional. Module TXD connects to STM32 PA10/RX; RXD to PA9/TX; GND is common. Leave module VCC disconnected from STM32 by default and use a confirmed independent board supply. Lamp/fan require driver stages and suitable load power; never supply a load from an MCU GPIO. Follow the complete hardware tables.

## Windows setup

Use short paths without spaces, for example `C:\esp\esp-idf` and `C:\esp\embedded-smart-home`. Install [Git for Windows](https://git-scm.com/download/win), Python 3.11/3.12, and the prerequisites from the [official ESP-IDF Windows setup](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/get-started/windows-setup.html). EIM users can open the matching IDF Terminal from Manage Installations.

The project retains the original repository's SDK baseline rather than upgrading for the new features. In PowerShell:

```powershell
New-Item -ItemType Directory -Force C:\esp
Set-Location C:\esp
git -c core.longpaths=true clone https://github.com/espressif/esp-idf.git
Set-Location esp-idf
git -c core.longpaths=true checkout d35ea023029363d97c42565f85e0db23486f26b2
git -c core.longpaths=true submodule update --init --recursive
.\install.ps1 esp32s31
. .\export.ps1
```

If script execution is blocked, `Set-ExecutionPolicy -Scope Process Bypass` applies only to the current shell. Activate this SDK in each new terminal. The first build needs access to GitHub and Espressif Component Registry. Component Manager resolves `main/idf_component.yml` and `dependencies.lock`, including official **BSP 1.0.1**, LVGL 9 and USB Host drivers. Do not replace the BSP with one for another Korvo board.

For `Filename longer than 260 characters`, first use short paths for the SDK, tools and project. The optional configurations in this local deep directory use the [official Ninja 1.13.2 release](https://github.com/ninja-build/ninja/releases/tag/v1.13.2) for long paths; ESP-IDF and BSP remain pinned.

STM32 uses only **Arm GNU Toolchain + CMake + Ninja**. Install the Windows `arm-none-eabi` package from [Arm](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) and add its `bin` directory to PATH. CMake/Ninja from the activated IDF environment can be reused. Install [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) and use ST-Link for flashing. No Keil, Arduino, PlatformIO or RTOS is required. STM32 source has no external SDK dependency.

```powershell
arm-none-eabi-gcc --version
cmake --version
ninja --version
git clone https://github.com/xuan511/embedded-smart-home.git C:\esp\embedded-smart-home
Set-Location C:\esp\embedded-smart-home
```

## Build and flash each board

### Korvo: real USB link by default

In the repository root, with the correct SDK activated:

```powershell
python tools/check_environment.py
idf.py --preview set-target esp32s31
idf.py --preview build
idf.py --preview -p COM5 flash monitor
```

Replace `COM5` with the **Korvo UART Type-C** port in Device Manager. Power Type-C supplies power only; USB-A is Host. If automatic download fails, hold BOOT, press/release RESET, release BOOT, then retry. Exit monitor with `Ctrl+]`.

Without STM32, the default firmware displays disconnected/waiting and **does not invent measurements**. Use `idf.py --preview menuconfig → Smart Home` to explicitly select simulation (`CONFIG_HOME_SIMULATION`, disabled by default), optional SD logging or LCD. `configs/simulation.defaults` provides an explicit simulation profile. Defaults do not override an existing `sdkconfig`; rebuild and flash after changing settings. SD failures do not format the card or stop core control.

Korvo is the supervisory control, configuration and display center; STM32 handles sensing, actuation and the specified local automatic rules. See the [official support evidence](docs/SUPPORT_EVIDENCE.md).

The environment check requires the exact documented SDK commit and GCC, unchanged SDK sources, matching recursive submodules, and no submodule-check override. It verifies setup, not physical hardware.

### STM32: start with stage 1

Run from the repository root; CMake loads the Arm toolchain file automatically:

```powershell
cmake -S stm32 -B stm32/build-stage1 -G Ninja -DSTAGE=1
cmake --build stm32/build-stage1
```

Outputs are `home_stm32.elf`, `.bin`, `.hex` and `.map`. The project includes startup, vectors and the 64KB/20KB linker script. It uses internal 8 MHz HSI and does not require the board's external crystal.

Power off before wiring ST-Link: **SWDIO→PA13, SWCLK→PA14, GND→GND, VTref→target 3V3**; connect NRST if available. VTref is a voltage reference, not an assumed power output. Power the board through the confirmed supply arrangement; keep BOOT0 low. Do not use the STM32 USB socket in this design.

In STM32CubeProgrammer, select **ST-LINK / SWD**, Connect and read the chip, select `stm32/build-stage1/home_stm32.elf`, then Download, Verify and Reset. With the CLI on PATH:

```powershell
STM32_Programmer_CLI -c port=SWD -w stm32/build-stage1/home_stm32.elf -v -rst
```

For connection failure, check power, ground, SWD, BOOT0 and chip identity first. If needed, connect NRST and use Under Reset. A USB socket alone does not establish a working flashing path.

## Five hardware acceptance steps

Use a separate STM32 build directory for each stage, then flash it. Keep Korvo on real USB firmware. Connect only the hardware required by the current stage. Detailed actions and blank observation tables are in [ACCEPTANCE](docs/ACCEPTANCE.md).

| Step | STM32 option | Expected behavior |
| --- | --- | --- |
| 1. Minimal link | `-DSTAGE=1` | Only USB-UART/power; handshake, configuration response and heartbeat; no readings; unplug reports disconnection, reconnect synchronizes again |
| 2. Test data and LED | `-DSTAGE=2` | Explicit TEST DATA: 30°C, 55%, 50 lux; `home light on/off` changes onboard LED with a returned state; fan requests are rejected |
| 3. Real sensors | `-DSTAGE=3` | SHT3x/BH1750 readings about once per second, STM32 SENSORS label; shade/warm sensors, test individual faults; only LED demonstration, fan requests rejected |
| 4. Real loads | `-DSTAGE=4` | Add driver circuits and independent low-voltage load power; manual first, then automatic hysteresis; record physical actions |
| 5. Persistence/faults | Stage 4 | Save/reboot/reconnect, communication loss, expiry, sensor faults and continuous running |

For example, stage 2:

```powershell
cmake -S stm32 -B stm32/build-stage2 -G Ninja -DSTAGE=2
cmake --build stm32/build-stage2
STM32_Programmer_CLI -c port=SWD -w stm32/build-stage2/home_stm32.elf -v -rst
```

The default LED is **PC13, active low**. Confirm the actual board before using it. Configure with `-DLED_PORT=C -DLED_PIN=13 -DLED_ACTIVE_LOW=1` if appropriate; ports A/B/C are supported. Stages 1–2 perform no real environmental acquisition. Do not prefill hardware PASS results.

## Operation, thresholds and state ownership

Touch, keys and console share one bounded request queue. The home screen shows readings, mode, returned lamp/fan output states and essential buttons. Change thresholds through the Korvo console:

```text
home help
home status
home light on
home light off
home fan on
home fan off
home mode auto
home mode manual
home thresholds 29 27 80 180
home save
```

Wait about one second after each operation before inspecting `home status`. If `pending=1`, wait for its response; retry a busy rejection later. Threshold arguments are fan ON/OFF °C, then lamp ON/OFF lux. Require fan OFF < ON and lamp ON < OFF. Defaults are 28/26°C and 100/200 lux. Temperature thresholds use two decimal places and lux one; hysteresis must survive rounding. Humidity is display-only. `save` persists thresholds in Korvo NVS; STM32 holds a runtime copy. Report persistence errors rather than assuming success.

- A manual load request selects MANUAL. STM32 alone executes AUTO rules.
- High temperature turns the fan on; low light turns the lamp on; hysteresis retains outputs between thresholds. An invalid/stale channel turns off its corresponding AUTO demonstration load.
- Boot/reconnect: new HELLO session → READY → Korvo sends authoritative thresholds → successful response enables operations. STM32 never writes old thresholds back into Korvo NVS; old manual requests are not replayed.
- A USB disconnect event or about three seconds without valid messages marks Korvo disconnected/stale. Old numbers do not retain an OK label. STM32 MANUAL shuts both loads off after about five seconds without link activity, or earlier on a fresh-session handshake; AUTO continues using valid local measurements.
- A request without its response after about 1.5 seconds has an unknown result and starts a new synchronization session; it is never optimistically shown as executed.
- **Queued request, returned GPIO output state and physical load action are separate evidence.** No current/tachometer feedback is fitted, so ON does not prove a spinning fan. Record physical action during acceptance.

For work without hardware, explicitly select simulation in menuconfig. The screen must say OFFLINE SIMULATION. `home sim 30 55 50` and `home fault` are simulation-only; real USB disconnection never activates simulation.

## Tests, source and troubleshooting

With Python and a host C compiler:

```powershell
python tools/test_host.py
```

Tests exercise control rules, framing and endpoint state handling; they do not prove hardware operation. See [protocol](docs/PROTOCOL.md), [architecture](docs/ARCHITECTURE.md), [acceptance](docs/ACCEPTANCE.md), [validation](docs/VALIDATION.md) and [roadmap](docs/ROADMAP.md).

| Path | Responsibility |
| --- | --- |
| `main/` | Korvo service, USB Host, LCD, NVS, console, optional SD/diagnostics |
| `components/home_core/` | Portable C validity, hysteresis and SHT CRC |
| `components/home_link/` | Shared ASCII-line protocol |
| `components/home_remote/` | Portable Korvo session, synchronization, request and expiry handling |
| `stm32/` | Main loop, real UART/I2C/GPIO, startup/link/build files |
| `tests/`, `tools/` | Host tests and environment checks |

For an unknown target, activate the correct SDK. For download failures, inspect network/certificates. If USB does not enumerate, verify chip/VID/PID; if it enumerates but does not handshake, check UART crossing, 3.3V logic and STM32 stage. Verify LED pin/polarity before blaming communication. For LCD issues, check the original panel, FPC direction and power. Disconnect loads and return to the minimum link when troubleshooting.

Project code is [MIT licensed](LICENSE); dependencies retain their own licenses. Official references are in [hardware documentation](docs/HARDWARE.md). This scope excludes AI, voice recognition, cloud, phone apps, MQTT, Matter, OTA, custom PCB and camera video. Existing diagnostics remain optional console tools.
