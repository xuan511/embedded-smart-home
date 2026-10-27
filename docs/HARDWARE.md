# 硬件与接线 / Hardware and wiring

核查日期：2026-10-27。**以下是接线设计与官方资料核对结果，尚未连接用户实物验证。** 先完成 [分阶段验收](ACCEPTANCE.md) 的通信/LED 闭环，再接传感器和负载。历史方案的纠正见 [硬件核查记录](HARDWARE_AUDIT.md)。

This is a wiring design checked against primary sources, not a physical validation result. Complete the communication/LED stages before adding sensors and loads.

## 固定双板方案 / Selected two-board link

```text
PC --USB data--> Korvo UART Type-C  (flash + console)
5V supply ----> Korvo Power Type-C
Korvo USB-A Host --USB data--> CP2102/CP2102N module
                                  TXD --> STM32 PA10 / USART1_RX
                                  RXD <-- STM32 PA9  / USART1_TX
                                  GND --- STM32 GND

STM32 PB6/PB7 ---> SHT3x-DIS + BH1750
STM32 PB0/PB1 ---> two MOSFET driver inputs ---> 5V light / 5V fan
```

**只实现 CP2102/CP2102N USB 转 3.3V TTL UART 这一种。** 固定单口 `VID:PID = 10C4:EA60`、接口 0；不把 CH340、FT232、PL2303 或定制 VID/PID 当成兼容设备。Korvo 使用 Espressif 的 `usb_host_cp210x_vcp` 驱动，通过 `cdc_acm_host` 收发 UART 字节。USB 插入/枚举成功并不代表 STM32 已握手；协议心跳仍须正常。

The only implemented transport is a single-port CP2102/CP2102N USB-to-UART adapter with **3.3V TTL logic**, default VID/PID 10C4:EA60, interface 0. Other bridge families need different drivers and are outside this build.

相比 STM32 原生 USB CDC，这条路径不依赖学习板 USB 插座是否接 PA11/PA12、D+ 上拉电路、外部晶振及 48MHz USB 时钟条件。STM32 使用内部 HSI 8MHz 和普通 USART，不需要 USB 栈。板上 USB 插座可否供电仍须核实，不能据此推断有数据通信。

This choice avoids unknown USB socket routing, pull-up circuitry and USB clock requirements on the STM32 learning board. Its USB connector is not used for data.

## Korvo 接口与保留资源 / Korvo interfaces and preserved resources

以 **ESP32-S31-Korvo-1 V1.1 + 原配 SUB3 屏**为准，目标是 `esp32s31`。保留所有 LCD/触摸和 SD 引脚，Korvo 不接外部传感器或负载的 GPIO 线。

| 实物接口 / Connector | 连接 / Use |
| --- | --- |
| UART Type-C，原理图 J1 | PC 数据线，烧录、115200 串口调试 / PC data cable for flashing and console |
| Power Type-C，J5 | 5V 电源，只供电 / Power only |
| USB-A Host，J2 | 插 CP2102/CP2102N 模块或其 USB 数据线 / Selected bridge only |
| LCD 连接器 J14 | 保持原配屏连接 / Keep original LCD connected |
| microSD 槽 J7 | 保持 SD 功能；不从卡槽取 GPIO / Keep SD; no GPIO breakout |

V1.1 原理图第 2 页：J2 的 1/2/3/4 分别为 VBUS、D−、D+、GND；它们已在板内接好，**直接插 USB，不把裸 USB 信号接到杜邦线**。Host 使用 S31 专用 USB_DP/USB_DM；BSP 头文件遗留的 `BSP_USB_POS=GPIO20`、`BSP_USB_NEG=GPIO19` 与本板原理图不符，不用于本工程。GPIO20 属于 SD，GPIO19 属于 LCD。依据：[V1.1 官方原理图](https://dl.espressif.com/schematics/esp32-s31-korvo-1-schematics.pdf)。

J2 is the existing USB-A host connector. Use a USB cable/plug; no rewiring of dedicated USB pins, LCD, touch or SD is needed.

Host 供电经过 U14 TPS2051C，官方标称最大 500mA；EN 在板上被拉高，不需要软件 GPIO 使能。`bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true)` 启动 Host 和事件任务；本板实现未使用这两个电源参数。不要据 `true` 声称软件实现了限流，也不要把 `stop` 当成已验证的 VBUS 断电。外部风扇/灯不从 Host 口供电。依据：[官方 BSP USB 实现](https://github.com/espressif/esp-bsp/blob/master/bsp/esp32_s31_korvo_1/src/bsp_usb.c)。

The host port has a hardware current limiter. The BSP power arguments are unused on this board; software start/stop is not a verified power switch. Power the loads separately.

## 最小模块清单 / Minimum parts

| 数量 / Qty | 模块 / Part | 明确要求 / Requirement |
| --- | --- | --- |
| 1 | ESP32-S31-Korvo-1 V1.1 | 原配 LCD/触摸；可选 FAT microSD |
| 1 | STM32F103C8T6 学习板 | 引出 PA9/10、PB0/1/6/7、PA13/14、3V3/GND；64KB Flash、20KB SRAM |
| 1 | 单口 CP2102 或 CP2102N USB-UART | 3.3V TTL；10C4:EA60；带 TXD/RXD/GND 排针；不买 RS232 电平模块 |
| 1 | ST-Link + 对应杜邦线 | 用 SWD 烧录；目标电压检测/供电脚必须分清 |
| 1 | SHT30/31/35-**DIS** 模块 | 3.3V 工作、I2C；优先 SHT31-DIS；默认 0x44 |
| 1 | BH1750 模块 | 3.3V 供电与 I2C 上拉；默认 0x23 |
| 2 | MOSFET 驱动模块 | 本文固定接线参考 **Adafruit MOSFET Driver PID 5648**（AO3406）及配套 JST-PH 2mm 三芯转杜邦线 |
| 1 | 5V 低压灯模块 | 已限流的两线灯，演示建议额定 ≤100mA；不用裸大功率 LED |
| 1 | 5V 两线直流小风扇 | 演示建议额定 ≤200mA，启动电流在驱动与电源能力内；不用市电/三线调速风扇 |
| 1 | 独立稳定 5V 负载电源 | 建议至少 1A，带可接线端子；只给灯/风扇驱动供电 |
| 按需 | 面包板、杜邦线、端子转接件 | 短线、绝缘；传感器模块须预焊排针或带可用线缆 |

PID 5648 的原厂资料明确支持 3.3V 输入、高有效，带栅极下拉和续流二极管；负载连接使用端子，输入使用成品线缆，无需修改 Korvo。参考：[模块引脚](https://learn.adafruit.com/adafruit-mosfet-driver/pinouts)、[厂家原理图](https://github.com/adafruit/Adafruit-MOSFET-Driver-STEMMA-PCB)。若改用已有模块，先核对其输入有效电平、3.3V 导通能力、栅极/输入下拉和续流保护，不能照搬其它模块的端子顺序。

The specified MOSFET module provides a clear solderless reference. Substitute modules require checking their actual schematic and pin labels; “5V module” alone does not prove 3.3V compatibility.

## STM32 信号连接表 / STM32 signal table

表中是 **MCU GPIO 名称**，不是学习板排针序号。LQFP48 引脚号仅用于对照官方 C8T6 引脚图，不能替代实物板丝印或导通核查。

| STM32 信号 / Signal | C8T6 LQFP48 脚 / Pin | 连接 / Connect to | 方向 / Direction |
| --- | --- | --- | --- |
| PA9 / USART1_TX | 30 | CP210x RXD | STM32 → 模块 |
| PA10 / USART1_RX | 31 | CP210x TXD | 模块 → STM32 |
| GND | VSS | CP210x GND | 共地 / Common ground |
| PB6 | 42 | 两传感器 SCL | 软件 I2C 开漏时钟 / Open-drain clock |
| PB7 | 43 | 两传感器 SDA | 软件 I2C 开漏数据 / Bidirectional data |
| 3V3 | 板上 3V3 | 两传感器 VCC | 仅传感器供电 / Sensor power only |
| GND | VSS | 两传感器 GND | 共地 |
| PB0 | 18 | 灯驱动 In（白线） | 3.3V 高有效 / Active high |
| PB1 | 19 | 风扇驱动 In（白线） | 3.3V 高有效 / Active high |
| PC13 | 2 | 只使用板上已有 LED 电路 | 默认低有效，**板型待确认** / Default active low, board-dependent |
| PA13 / SWDIO | 34 | ST-Link SWDIO | SWD |
| PA14 / SWCLK | 37 | ST-Link SWCLK | SWD |
| NRST（建议接） | 7 | ST-Link NRST | 复位 / Reset |

CP210x 的 **VCC/5V/3V3/DTR/RTS 不接 STM32**。模块由 Korvo USB-A 供电，UART 只接 TXD/RXD/GND；模块的“3.3V/5V 跳帽”有时只切换供电输出而非 UART 电平，应查模块说明或测 TXD 空闲高电平。不要把 RS232 的正负电压接 MCU。

Connect only TXD, RXD and GND between bridge and STM32. A voltage selector may change a power output without changing UART logic; verify the specific adapter.

SHT3x ADDR 接 GND 选 0x44；BH1750 ADDR 接 GND 选 0x23。本版驱动固定这两个地址，选购时确认地址；已固定其他地址的模块不能直接代入。两路 SDA/SCL 共线，上拉到 **3.3V**；短线起步（建议各线 ≤20cm）。模块带上拉时先核实，整条总线需要有效上拉；没有时在 SDA/SCL 各加约 4.7kΩ 至 3.3V，勿拉到 5V。固件每约 1 秒采一组，SHT 测温湿度、BH 测环境照度。

Use 3.3V sensor power and pull-ups. Both sensors share PB6/PB7; check address straps and fitted pull-ups. A bus ACK is not proof of correct measurements.

## 灯和风扇接线 / Light and fan wiring

下面两路分别使用一个 PID 5648，不能把风扇和灯并成同一控制输出。

| 连接位置 / Point | 灯驱动 / Light driver | 风扇驱动 / Fan driver |
| --- | --- | --- |
| JST 白线 In | STM32 PB0 | STM32 PB1 |
| JST 黑线 GND | 独立负载电源负极，并接 STM32 GND | 同一公共 GND |
| JST 红线 V+ | **独立负载电源 +5V** | 同一 +5V |
| 输出端子 `+` | 灯正极 | 风扇正极 |
| 输出端子 `−`（Out） | 灯负极 | 风扇负极 |

**红线接负载电源 +5V，不接 STM32 的 3V3；黑线必须共地。** 模块将 Out 低端接地实现开关，Out 不是固定 GND。PB0/PB1 只给控制信号，不能给负载供电。两线风扇只做断电/通电；显示 ON 仅表示 STM32 已写输出寄存器，没有转速/电流/光学反馈。

The red JST wire supplies the load and connects to the separate 5V supply. The white wire is the 3.3V control signal. Out is a switched ground. ON confirms the MCU output state only; observe the actual light/fan during acceptance.

参考模块已有硬件下拉与续流保护。若用其它 N-MOSFET 模块，须另按实际原理图确认续流二极管（阴极接负载正极，阳极接开关负端）与输入下拉；不假设任意模块兼容。

## 供电、烧录和共地 / Power, flashing and ground

1. Korvo UART Type-C 接电脑数据线，Power Type-C 接稳定 5V 电源；首次不接音频负载。原配屏和 SD 保留。
2. STM32 **使用已核实的板上供电入口单独供电**。可以是已确认的 5V 稳压入口或已确认可供电的板 USB 口，但本次未获板型，不能指定未知插座或把 5V 接到 3V3。MCU 本体工作在 3.3V。
3. CP210x VCC 输出不接 STM32。风扇/灯使用独立 5V 电源；其负极、两驱动 GND、STM32 GND、CP210x GND 共地。Korvo 地经 USB 与模块相连。
4. ST-Link 接 PA13/SWDIO、PA14/SWCLK、GND，建议接 NRST；真品探针的 VTref 接目标 3V3 用于电压检测。廉价探针标“3.3V”的脚可能是供电输出，应查说明。目标已独立供电时不再接另一个供电输出。
5. BOOT0 保持 0，使用 SWD 下载。不要同时把模块插 PC 和 Korvo 两个 Host；要在 PC 上检查 CP210x 时先从 Korvo 拔下。
6. 断电接线；确认电源和 GND 后再上电。第一次只接通信三线，后续按阶段加外设。记录负载电源电压、上电/复位默认关闭和实际动作。

Power the STM32 through a verified board input and flash it via SWD. Keep one intentional power source per rail. The bridge and load supply share the STM32 ground; do not join their positive rails. The learning-board power connector, regulator and LED circuit remain board-specific.

## 最少待确认信息 / Minimum information still needed

| 信息 / Information | 影响 / Why it matters | 未确认时 / Until confirmed |
| --- | --- | --- |
| STM32 板型号或原理图，PA/PB/SWD 排针丝印、供电入口、PC13 LED 电路 | 正确接电、烧录及 LED 极性 / Power, SWD and LED polarity | 不指定未知插座；只按 MCU 信号设计；先不接外部负载 |
| USB-UART 芯片及硬件 ID（Windows 设备管理器属性→详细信息→硬件 ID），UART 电平 | 驱动和 TX/RX 电平 / Driver and electrical levels | 只接受 CP2102/CP2102N 10C4:EA60、3.3V TTL |
| 实际灯/风扇电压、额定/启动电流，驱动模块型号/端子定义 | 选电源与接线 / Load supply and driver limits | 按固定 5V 参考 BOM 准备，未知模块不接 |

这些未知项不阻止协议、界面、控制逻辑和固件构建。它们影响实物接线/烧录，确认后才能把对应验收项填 PASS。

## 官方依据 / Primary references

- [Korvo V1.1 用户指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)；[V1.1 原理图](https://dl.espressif.com/schematics/esp32-s31-korvo-1-schematics.pdf)，页 2 电源/USB、页 3 MCU/LCD/SD。
- [BSP 1.0.1](https://components.espressif.com/components/espressif/esp32_s31_korvo_1/versions/1.0.1)；[USB Host CDC 官方示例](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/cdc)；[CP210x Host 驱动 2.2.0](https://components.espressif.com/components/espressif/usb_host_cp210x_vcp/versions/2.2.0/readme)。
- [Silicon Labs CP2102N 数据表](https://www.silabs.com/documents/public/data-sheets/cp2102n-datasheet.pdf)；[Windows VCP 驱动](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers)。
- [ST STM32F103C8 数据表 DS5319](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)，表 5 引脚；[RM0008 寄存器手册](https://www.st.com/resource/en/reference_manual/cd00171190.pdf)。学习板 PCB 不属于 ST 官方板，MCU 手册不能证明其插座/LED 电路。
- [Sensirion SHT3x-DIS 数据表](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf)。
- BH1750 原 README 所给 ROHM 数据表链接本次返回 404；命令和换算另核对了 [Espressif 官方 BH1750 驱动](https://github.com/espressif/esp-iot-solution/tree/master/components/sensors/light_sensor/bh1750)。模块电源/上拉/ADDR 仍须按所购模块电路核查，不能声称已重新取得原厂数据表。
- [Adafruit 5648 引脚与接线](https://learn.adafruit.com/adafruit-mosfet-driver/pinouts)；[厂家电路源文件](https://github.com/adafruit/Adafruit-MOSFET-Driver-STEMMA-PCB)。
