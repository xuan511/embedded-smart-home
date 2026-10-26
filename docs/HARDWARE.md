# 硬件与外设 / Hardware and peripherals

## 板级依据 / Board baseline

仅面向 **ESP32-S31-Korvo-1 V1.1 + 原配 SUB3 LCD**。引脚由官方 BSP 1.0.1 定义，应用直接调用 BSP API。不要套用 ESP32-S3-Korvo、ESP32-S3-BOX 或普通 DevKit 的接线。

Target: **ESP32-S31-Korvo-1 V1.1 + original SUB3 LCD**. The official BSP owns peripheral pins. Other S3/Korvo boards are not pin-compatible assumptions.

| 资源 / Resource | 官方分配 / Assignment | 应用处理 / Usage |
| --- | --- | --- |
| I2C | SDA GPIO0, SCL GPIO1 | `bsp_i2c_init()` / `bsp_i2c_get_handle()` |
| I2S | BCLK 3, LRCLK 4, DOUT 5, DIN 6; PA 7 | `bsp_audio_codec_*_init()` |
| Audio MCLK | BSP marks it NC | 16 kHz codec sessions; do not drive GPIO2 yourself |
| RGB | GPIO37 | BSP LED indicator API |
| Function keys | GPIO42 ADC resistor ladder | BSP button component handles thresholds |
| UART0 | TX58 / RX59 | USB-to-UART console and flashing |
| microSD | GPIO20–25; enable39 | BSP SDMMC mount; no GPIO reuse |
| LCD | GPIO8–19, 33–36, 38, 40, 43–45, 60–61 | BSP initializes RGB timings, panel and touch |
| Camera | GPIO46–57; GPIO38 shared LCD/boot function | Optional BSP camera initialization only |
| USB HS | Dedicated USB_DP / USB_DM | No application GPIO override; USB app deferred |

注：官方硬件指南的音频时钟表会列 GPIO2，但官方 BSP 将 MCLK 设为 NC，并建议 16 kHz；本工程跟随 BSP API。USB 相关头文件中存在与板级指南不一致的旧 GPIO 宏，本工程不使用这些宏。上板时以相应硬件版本的原理图和已验证 BSP 为准。

The hardware guide lists GPIO2 for audio MCLK, while the BSP declares it unconnected and recommends 16 kHz. This project follows the BSP. Legacy USB pin macros differ from the hardware guide and are not used here.

## 可直接试的板载功能 / Built-in functions

- LCD/触摸：显示读数并切换输出；关闭 `HOME_UI` 可先验证串口。LCD/touch displays readings and dispatches commands; disable UI to isolate console operation.
- 按键：SET 自动、PLAY 灯、VOL+ 风扇、VOL− 手动。Firmware follows BSP enum order, not a guessed ADC conversion.
- 音频：`home test tone` / `mic`。扬声器连接到板上专用 2.00 mm 插座，按官方规格使用 4Ω/3W 扬声器；本工程输出低音量 1 秒测试音。Use the dedicated speaker connector; microphone amplitudes are not SPL measurements.
- SD：断电插卡，开启 `HOME_SD_LOG`；每 10 秒追加 CSV，未挂载不影响其他功能。Insert a FAT card while powered off. Logging failure stops further writes until reboot.
- 相机：开启 `HOME_CAMERA`，运行 `home test camera`。只探测，不取帧。Camera initialization and format query only.
- Wi-Fi：`home test wifi` 只扫描，无凭据，无自动联网。AP names are printed to the local console, not saved or uploaded.

## 环境传感器：共享 I2C / External sensors on shared I2C

**这块板没有预留可随意使用的大量空闲 GPIO。接入外设之前先确认实物可访问点。** GPIO0/1 是已被音频/触摸/相机控制使用的 I2C，总线共享是为了避免占用 RGB LCD 与相机数据线，不等于板上必有现成扩展插座。

There is no assumed ready-to-use GPIO header. Identify accessible board pads/connectors from the actual schematic before connecting anything. Adding devices to the existing I2C bus preserves LCD/camera data pins, but still requires a verified physical connection.

| 外接模块 / Module | 模块信号 / Signal | 连接 / Connection |
| --- | --- | --- |
| SHT30/31/35-DIS | VCC, GND | 3.3V, common GND |
| SHT3x-DIS | SDA, SCL | GPIO0/SDA, GPIO1/SCL |
| SHT3x-DIS | ADDR | GND: 0x44; high: 0x45 |
| BH1750 | VCC, GND | 3.3V, common GND |
| BH1750 | SDA, SCL | Same GPIO0/SDA, GPIO1/SCL |
| BH1750 | ADDR | GND: 0x23; high: 0x5C |

- 使用 3.3V 供电/逻辑模块；确保总线没有被拉到 5V。检查模块是否自带上拉，避免并接过多上拉电阻。Use 3.3V logic and inspect pull-ups.
- 不要重新创建 GPIO0/1 的 I2C 主机；用 BSP bus handle 添加设备。IDF I2C 驱动串行化总线事务；传感器等待转换期间不占着总线。Do not create a second I2C controller on those pins.
- SHT3x 使用 `0x2400` 单次测量，等待 20ms，验证温度和湿度各自 CRC。BH1750 使用 `0x20` 单次高分辨率，等待 180ms，默认 MTreg=69，照度为 raw/1.2。Readings refresh approximately once per second; failed reads remain invalid.
- 若可访问焊点不方便，先保留纯板载演示。另一个简单后续方案是 USB 串口接外部传感器控制器，但本版没有实现该桥接驱动。A USB-UART sensor bridge is an alternative future adapter, not an implemented feature.

## 可选真实输出 / Optional real outputs

为了不抢占现有 GPIO，本版提供 MCP23017 I2C 扩展器方案：地址固定 **0x20**，BANK=0 复位布局，GPA0 灯光、GPA1 风扇，均为高有效。此选项默认关闭。

The optional external expander is **MCP23017 at 0x20**, reset BANK=0 register layout. GPA0 is the light command; GPA1 is the fan command; active high. Disabled by default.

| MCP23017 信号 / Signal | 接法 / Connection |
| --- | --- |
| VDD / VSS | 3.3V / common GND |
| SDA / SCL | BSP GPIO0/SDA / GPIO1/SCL |
| A0/A1/A2 | GND for 0x20 |
| RESET | Pull up to 3.3V per module/datasheet; ensure reset at power-up |
| GPA0 | Light driver logic input |
| GPA1 | Fan MOSFET/driver logic input |

先给 OLATA 写 0，再把 GPA0/1 配置为输出，减少初始化误动作。软件输出不是转速、电流或灯光反馈。复位时 I/O 是输入，驱动级还需要栅极/输入下拉以保持负载关闭。

The driver writes the OFF latch before changing direction. Provide hardware pull-downs so loads stay OFF while the expander/MCU resets. Command success is not actuator feedback.

负载只选桌面演示所需的低压直流风扇/灯。MOSFET 或驱动模块应匹配负载电压、电流和 3.3V 控制逻辑；感性负载加续流保护，负载独立供电，与控制端共地。板载 LED/MCU/扩展器 I/O 不直接给电机供电。本版没有 PWM 调速、转速检测、继电器市电控制或负载闭环。

Use low-voltage DC demonstration loads with suitable 3.3V-compatible driver stages, flyback protection and separate load power/common ground. No PWM speed control, tachometer, mains switching or actuator feedback is implemented.

## 原始资料 / Primary references

- [Espressif Korvo-1 V1.1 guide and schematics](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)
- [BSP header and pin ownership](https://github.com/espressif/esp-bsp/blob/master/bsp/esp32_s31_korvo_1/include/bsp/esp32_s31_korvo_1.h)
- [BSP audio implementation](https://github.com/espressif/esp-bsp/blob/master/bsp/esp32_s31_korvo_1/src/bsp_audio.c)
- [Sensirion SHT3x-DIS datasheet](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf)
- [ROHM BH1750FVI datasheet](https://fscdn.rohm.com/en/products/databook/datasheet/ic/sensor/light/bh1750fvi-e.pdf)
- [Microchip MCP23017 product / datasheet](https://www.microchip.com/en-us/product/MCP23017)
