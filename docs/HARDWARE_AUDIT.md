# 硬件核查记录 / Hardware audit

日期：2026-10-27。基线：仓库提交 `bd7967a`。核查了代码、README、依赖锁和旧验证记录；结合 [官方用户指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)、[V1.1 原理图](https://dl.espressif.com/schematics/esp32-s31-korvo-1-schematics.pdf)（视觉检查第 2/3 页）及 BSP/USB 驱动源码。未连接实物板。

## 原工程实际状态 / Original repository state

| 原内容 / Original content | 核查结论 / Finding |
| --- | --- |
| 纯 C `home_core` | 已有阈值合法性、独立故障、滞回、过期判断和 SHT CRC；适合复用 / Existing reusable control and validation code |
| `home_service`、UI、NVS、队列、SD | 有实现；原架构在 Korvo 本地执行规则 / Implemented for previous single-board ownership |
| 默认温湿度/光照 | 明确模拟，无真实传感器数据 / Explicit simulation |
| SHT3x、BH1750 可选驱动 | 有真实 I2C 事务与换算，非空函数；未证明传感器已接通 / Real transactions exist, hardware unverified |
| MCP23017 可选驱动 | 有寄存器写入，但没有确认可用且免改板的 Korvo 外接接口 / Code exists without a verified physical wiring path |
| “GPIO0/1 可共享 I2C” | 电气总线事实不足以构成可接线方案；没有可直接腾出的扩展接口 / Sharing a bus does not establish an available connector |
| LCD/触摸/SD、RGB、按键、自检 | 依赖官方 BSP；旧验证记录明确未真机烧录，不能算实测完成 / BSP-backed code, not physically tested |
| USB Host、STM32 工程 | 原基线尚未实现 / Absent in the baseline |
| 原 `VALIDATION.md` 的 BUILD/主机测试 PASS | 是旧版本历史报告，不是当前修改的复验；本轮须单列结果 / Historical claims, not a new verification |

旧文档提出的 Korvo 外接 I2C 与 MCP23017 接线不再作为本系统硬件方案。新的唯一外设扩展路径为 USB-A → CP210x → STM32 UART。Korvo 的 LCD/触摸、SD 均保留。所有自动规则由 STM32 执行，Korvo 不再写外部负载 GPIO。

## 官方资料核实与边界 / Primary-source checks and limits

- V1.1 的 USB-A 是 J2，使用 S31 专用 USB_DP/USB_DM。第 2 页 U14 的 EN 接硬件上拉；BSP USB start 只安装 Host、创建事件任务。[BSP 实现](https://github.com/espressif/esp-bsp/blob/6db6acb86a7dd56d17bf42093d2119dd9b0c14f5/bsp/esp32_s31_korvo_1/src/bsp_usb.c)与原理图相符，不使用 GPIO20/19 旧 USB 宏。
- [USB CDC 官方示例](https://github.com/espressif/esp-idf/tree/d35ea023029363d97c42565f85e0db23486f26b2/examples/peripherals/usb/host/cdc)列出 ESP32-S31 并覆盖 CP210x；另下载并检查 registry 的 **CP210x 2.2.0 实际发布包**，确认包含 C API `cp210x_vcp_open`，不是只按 master 猜接口。驱动处理 CP210x 厂商控制请求，不把模块当通用原生 CDC。
- 选定默认单口 10C4:EA60，为了收敛采购与枚举范围。芯片可改 VID/PID，故必须读实物硬件 ID；“CP210x 家族”不代表本项目支持任意配置。
- [ST 数据表](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)规定 C8 资源按 64KB Flash、20KB SRAM；表 5 的 PA9/10 为 USART1、PA13/14 为 SWD。软件不使用网上所谓 C8 隐藏 128KB。PC13 LED 接法由学习板决定；PC13-15 输出能力有限，不外接大电流负载。
- [SHT3x-DIS 原厂数据表](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf)核对了 I2C、单次测量、CRC 与单位换算。选数字 DIS 模块，不能混用 ARP 模拟版本。
- BH1750 的旧 ROHM 官方数据表链接目前 404，故不声称重新取得该文件；交叉核对 [Espressif 官方驱动](https://github.com/espressif/esp-iot-solution/tree/7a813417274c663226601f390668b9c8e5eec768/components/sensors/light_sensor/bh1750)的单次高分辨率 `0x20`、180ms 等待、16 位读数除以 1.2。具体模块电路和实测精度未核实。
- 负载接线固定参考 [Adafruit PID 5648](https://learn.adafruit.com/adafruit-mosfet-driver/pinouts)。检查厂家 Eagle 电路：AO3406、10kΩ 输入下拉、续流二极管，JST 三芯输入和端子负载接口。3.3V 输入支持有厂家依据，仍须上板观察实际动作。

## 尚不能从资料推出的事实 / What remains unverified

不知道用户 STM32 学习板具体 PCB，因此不能确定 USB 插座供电/数据走向、稳压器能力、SWD 插针顺序、板载 LED 引脚与极性。也未取得 USB-UART 模块和负载型号，不能确定丝印、UART 电平或实际电流。最少待确认列表和可落实的信号表集中在 [HARDWARE.md](HARDWARE.md)，无需逐项阻塞与这些信息无关的开发。

The documentation identifies signal names, direction, power rails and one supported bridge/driver path. Physical connector order on the unknown STM32 PCB and adapter remains a hardware acceptance item. Firmware ACKs confirm MCU-reported output states; real load movement needs observation. No tachometer/current feedback is implemented.

## USB 控制器源码复核 / USB controller source review

另核对 registry 实际发布包 BSP 1.0.1、usb 1.6.0、usb_host_cdc_acm 2.4.1 与 CP210x 2.2.0。BSP 的 `peripheral_map` 默认 0；usb 1.6.0 将 0 映射为控制器 0。固定 SDK 提交 `d35ea023029363d97c42565f85e0db23486f26b2` 的 S31 HAL 将控制器 0 定义为唯一的 USB_OTGHS，使用 UTMI PHY，`internal_phy_io=NULL`，不映射普通 GPIO。由此可以从源码确认 Host 初始化不会抢占 GPIO19/20。该检查是静态源码审核，仍不等于 USB 插拔已在板上通过。

Code references: [S31 USB controller table](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/components/esp_hal_usb/esp32s31/usb_dwc_periph.c), [S31 USB LL mapping](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/components/esp_hal_usb/esp32s31/include/hal/usb_dwc_ll.h), [USB 1.6.0 release](https://components.espressif.com/components/espressif/usb/versions/1.6.0/readme). The selected APIs and callbacks match the released headers. This is source review, not a runtime USB test.
