# 官方资料与实现对应表 / Official references and implementation

复核日期：**2026-11-12**。本表检查实际源码与原厂资料之间的对应关系，基础版本为 `a2cb09e`；后续修订应同时更新代码、测试和验证记录。它不是任何厂商对整套毕设的认证，也不代表连接过用户实物。

“严格参考官方资料”在本项目中的具体要求是：硬件接口和寄存器以器件/开发板原厂手册、原理图为依据；SDK 使用固定版本的官方接口；应用协议、自动规则和界面行为由工程明确设计并测试；资料未取得或板型未确认的部分保留为未知。不能把乐鑫文档当成 STM32、学习板 PCB 或传感器的原厂证明。

## 如何使用证据

1. 从下面的实现文件找到功能对应的官方来源和章节，再查 [接线表](HARDWARE.md) 或 [零基础教程](BEGINNER_GUIDE.zh-CN.md)。
2. GitHub 源码尽量固定提交，发布组件由 `dependencies.lock` 固定。在线 `latest` 指南可能更新，不能替代固定源代码。
3. [机器可读记录](official-reference-manifest.json)保存本次实际下载文件的 URL、日期、响应状态和 SHA256；获取失败的项目记录失败，**没有下载字节就没有伪造的哈希**。完整原厂手册不复制进仓库，避免把旧副本误当最新版。
4. 运行依据分别进入 [VALIDATION](VALIDATION.md)：代码存在、编译通过、主机测试通过、真机验证通过必须分别记录。厂商示例成功不等于本项目实物成功。

## 开发板、USB、LCD 和 SD

| 功能 / 实现文件 | 官方来源与定位 | 已核对结论 | 本工程的选择 | 仍需验证 |
| --- | --- | --- | --- | --- |
| Korvo 实际接口；`main/home_usb.c` | [S31 Korvo V1.1 用户指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)，Description of Components；[V1.1 原理图](https://dl.espressif.com/schematics/esp32-s31-korvo-1-schematics.pdf)，印刷页 2/3 | J2 为 USB-A Host；UART 与 Power Type-C 用途不同；原配显示/SD 有独立板内连接 | USB-A 插指定桥模块，保留 LCD/触摸/SD，不抽取 GPIO，不修改主板 | 实物板版本、数据线、供电与并行运行 |
| S31 Host 与 CP210x；`main/home_usb.c` | [固定 IDF CDC Host 示例](https://github.com/espressif/esp-idf/tree/d35ea023029363d97c42565f85e0db23486f26b2/examples/peripherals/usb/host/cdc)；[S31 HAL](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/components/esp_hal_usb/esp32s31/usb_dwc_periph.c)；[CP210x 2.2.0 源码](https://github.com/espressif/esp-usb/blob/ebfa6f84ee7c7c070dcc244c79573eeab228298e/host/class/cdc/usb_host_cp210x_vcp/include/usb/vcp_cp210x.h) | 官方 S31 Host 示例及专用 CP210x API 可用于该通路；不是 S3 GPIO19/20 接法 | 只实现 `10C4:EA60` 单口、接口 0、115200/8N1 | USB 枚举、双向流量、热插拔均未实测 |
| Host 供电和 LCD/SD；`main/home_service.c`、`main/ui.c` | [BSP 1.0.1 USB](https://github.com/espressif/esp-bsp/blob/6db6acb86a7dd56d17bf42093d2119dd9b0c14f5/bsp/esp32_s31_korvo_1/src/bsp_usb.c)、同提交 `bsp_display.c`/`bsp_storage.c` | BSP 提供 Host、LCD/触摸、SD 接口；该 USB 实现未使用 power mode/limit 参数 | 不据参数声称软件限流或 VBUS 开关已实现，灯/风扇独立供电 | 原配屏、触摸和 SD 在 USB/联网期间运行 |
| USB 桥电气条件；`docs/HARDWARE.md` | [Silicon Labs CP2102N Data Sheet Rev.1.5](https://www.silabs.com/documents/public/data-sheets/cp2102n-datasheet.pdf)，§1、§2、§3.1.7、§4.1/4.2/4.5 | 芯片为 USB Full-Speed UART 桥；身份可定制；UART 逻辑受电气配置影响 | 选默认身份、3.3V TTL 模块；TX/RX 交叉并共地，VCC 不接 STM32 | 用户所持模块芯片、VID/PID、TXD 电平和排针顺序 |
| Hub 限制；`main/home_usb.c` | [S31 USB Host 指南](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s31/usb_host.html#features-limitations)，Features & Limitations | 必须考虑 HS Host 经 Hub 连接 FS/LS 设备的 Transaction Translator 限制 | CP210x 直接插 Korvo USB-A，不加入 Hub | 当前选定连接仍需实物枚举 |

更完整的已固定 USB/BSP 版本映射、遗留 GPIO 宏纠正和社区材料取舍见 [SUPPORT_EVIDENCE](SUPPORT_EVIDENCE.md)。社区问题报告只能辅助定位，未经厂商核实的机器人回复或其它芯片经验不作为硬件事实。

## STM32、传感器和执行器

以下页码指手册印刷页；不同下载入口或 PDF 解析器的页序可能不同，优先按文档编号、版本、章节和表号查找。

| 功能 / 实现文件 | 官方手册与章节 | 已核对结论 | 工程选择 | 未验证边界 |
| --- | --- | --- | --- | --- |
| 资源与 GPIO；`stm32/stm32f103c8.ld`、`stm32/src/board.c` | [ST DS5319 Rev.20](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)，Device overview、§3/Table 5、§5.3.7/Table 24 | C8 按 64KB Flash、20KB SRAM；LQFP48 的 PA9/10、PB0/1/6/7、PA13/14 可按表核对；HSI 标称 8MHz | 链接上限按官方 C8 资源，保留 2KiB 栈空间；不用外部晶振假设 | 学习板插针、USB 插座、稳压和 LED 电路不在 MCU 手册证明范围 |
| RCC、GPIO、TIM2、USART；`stm32/src/board.c` | [ST RM0008 Rev.21](https://www.st.com/resource/en/reference_manual/cd00171190.pdf)，RCC 第7章、GPIO 第9章、通用定时器第15章、USART 第27章，特别是 §27.3.4/27.3.5；[ST 官方器件头文件 v4.3.5 固定提交](https://github.com/STMicroelectronics/cmsis-device-f1/blob/8a76309ed1250d817e9c888c4417171d2ba3ba63/Include/stm32f103xb.h) | 复核地址、寄存器位与 USART1 IRQ37；波特率需同时考虑分频和时钟偏差 | HSI 8MHz；TIM2 预分频为 1MHz 计时；BRR=69；RX 中断入环形缓冲 | HSI/电源/温度影响、真实 UART 误码与连续运行 |
| 启动、向量、SysTick；`stm32/startup_stm32f103x8.S`、`stm32/src/board.c` | [ST PM0056 Rev.7](https://www.st.com/resource/en/programming_manual/cd00228163.pdf)，§2.3.4 Vector table、NVIC/SCB、§4.5 SysTick；[ST 官方 GCC 启动模板固定提交](https://github.com/STMicroelectronics/cmsis-device-f1/blob/8a76309ed1250d817e9c888c4417171d2ba3ba63/Source/Templates/gcc/startup_stm32f103xb.s) | 初始 SP/Reset/异常向量顺序、Thumb 入口、VTOR 与 SysTick 有官方定义 | 自包含启动文件只完成本项目需要的初始化；SysTick 7999/8MHz 生成标称 1ms 节拍 | 未测最大栈用量、实际时钟精度；预留栈不是栈水位实测 |
| 软件 I2C；`stm32/src/board.c` | [NXP UM10204 Rev.7](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)，§3.1.1 SDA/SCL、§3.1.4/3.1.6/3.1.7、§3.1.16 Bus clear；RM0008 GPIO 配置 | 总线采用开漏与上拉，区分 START/STOP/ACK；总线清理可使用时钟脉冲 | PB6/PB7 单控制器软件 I2C、3.3V 上拉、保守延时、超时和至多 9 个恢复时钟；不用 F1 硬件 I2C 外设 | 上升时间、总线电容、电压、波形与真实恢复结果需测；未声称完整 I2C 合规认证 |
| SHT3x；`stm32/src/sensors.c`、`components/home_core/home_core.c` | [Sensirion SHT3x-DIS Version 7](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf)，Table 4/8/9/20、§4.3/4.12/4.13 | 0x44 地址选择、0x2400 单次高重复性命令、最大 15ms 转换、CRC 与换算对应手册 | 3.3V、等待 16ms、读取 6 字节并检查两段 CRC；约 1Hz 调度 | 实物器件、模块上拉/ADDR、精度与采样波形未测 |
| BH1750；`stm32/src/sensors.c` | [ROHM 原文 Rev.D（分销商镜像）](https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf)，正文印刷页 1/2/5/7/11/13；来源链为 [Adafruit 官方模块 Downloads](https://learn.adafruit.com/adafruit-bh1750-ambient-light-sensor/downloads)；[Espressif 固定驱动](https://github.com/espressif/esp-iot-solution/tree/7a813417274c663226601f390668b9c8e5eec768/components/sensors/light_sensor/bh1750) | 已浏览原文核对 VCC 2.4～3.6V、ADDR低时0x23、0x20单次高分辨率、最长180ms等待、默认MTreg=69时 `/1.2` 换算 | 地址固定 0x23，默认测量时间，读 2 字节；未实现扩量程或标定 | ROHM官网直链仍失败；镜像由模块厂商指向，非ROHM站点直接下载。模块排针/上拉/误差与饱和仍需实测 |
| 灯/风扇 MOSFET；`stm32/src/board.c` | [Adafruit PID 5648 Pinouts](https://learn.adafruit.com/adafruit-mosfet-driver/pinouts)、[厂家电路源文件](https://github.com/adafruit/Adafruit-MOSFET-Driver-STEMMA-PCB) | V+ 为负载电源、GND 共地、In 高有效、Out 为开关地；3.3V 控制在规定输入范围内 | PB0/PB1 只给控制，两个独立驱动模块，独立 5V 负载供电；按现有 BOM 小电流演示 | 具体模块/负载额定及启动电流、实际动作；无电流/转速反馈 |

**时钟与 LED 的必要边界：** `8,000,000/69≈115,942` 与 115200 相差约 +0.64%，这是本工程的分频计算，尚未包含 DS5319 Table 24 的 HSI 偏差。不能由该数值宣称所有温度/供电下都能可靠通信，应保存真机误码、断线和运行日志。PC13 的输出限制见 DS5319 Table 5 脚注，不能拿它直接带负载；默认低有效板载 LED 仍须核对实际板电路。

## 网络、参数与微信

| 功能 / 实现文件 | 官方来源 | 核对内容与工程取舍 | 仍需验证 |
| --- | --- | --- | --- |
| Wi-Fi STA；`main/home_network.c` | [固定 SDK station 示例](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/examples/wifi/getting_started/station/main/station_example_main.c) | netif/事件循环/STA 配置、获得 IP 和断线事件遵循官方接口；空 SSID 时本项目关闭网络 | 真实 AP、DHCP、断网重连和内存余量 |
| HTTP；`main/home_network.c` | [固定 SDK esp_http_server.h](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/components/esp_http_server/include/esp_http_server.h)；[S31 HTTP Server 指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/protocols/esp_http_server.html) | 启动服务、注册 URI、读 body/header 和响应使用官方 API；512-byte 平面 JSON、Bearer 令牌和请求 ID 是工程设计 | ESP HTTP 服务运行时、慢连接和手机网络 |
| MQTT；`main/home_network.c` | [ESP-MQTT 1.0.0 固定头文件](https://github.com/espressif/esp-mqtt/blob/2c3e1e0942afcc5b3cea2710ffe220fe699da02a/include/mqtt_client.h)、同提交 README | 官方配置和 publish/事件 API；本工程选择 LAN TCP、QoS0 遥测、不 retained、不积压历史数据、遗嘱在线状态 | ESP-MQTT 与真实 Broker 的运行；主机 MQTT 测试不替代该项 |
| 阈值保存；`main/home_service.c` | [固定 SDK NVS API](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/components/nvs_flash/include/nvs.h) | 使用 nvs_open/get_blob/set_blob/commit/close；NVS 权威副本、保存后下发和版本同步由本工程设计 | 真正 Flash 保存、掉电与重连顺序 |
| 微信同网段 HTTP；`miniprogram/utils/client.js` | [微信网络说明](https://developers.weixin.qq.com/miniprogram/dev/framework/ability/network.html)、[局域网通信](https://developers.weixin.qq.com/miniprogram/dev/framework/ability/mDNS.html) | 官方允许同网段非本机 IP 使用 HTTP；工程手动输入 Korvo IP，避免依赖 mDNS | 自己的 AppID、开发权限、手机系统局域网权限和关闭调试后的真机运行 |
| 页面与请求生命周期；`miniprogram/pages/home/home.js` | [微信 Page](https://developers.weixin.qq.com/miniprogram/dev/reference/api/Page.html)、[wx.request](https://developers.weixin.qq.com/miniprogram/dev/api/network/request/wx.request.html) | 依官方 onShow/onHide/onUnload、success/fail/timeout 语义管理轮询；迟到回调隔离、接收年龄和不自动重发属于本项目策略 | 微信编译器、实际手机生命周期和真实请求延迟 |

## 安装与烧录的官方入口

| 工具/环节 | 官方入口 | 教程应说明的边界 |
| --- | --- | --- |
| Git for Windows | [Git 官方 Windows 安装页](https://git-scm.com/install/windows) | 安装后新开终端，确认命令可用；保留实际版本 |
| Python | [Python 官方 Windows 下载](https://www.python.org/downloads/windows/) | 本工程沿用已验证的 3.11/3.12，不因页面推荐最新版就擅自升级 |
| ESP-IDF | [S31 Windows 安装指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/get-started/windows-setup.html)、[固定 install.ps1](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/install.ps1)、[固定 export.ps1](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/export.ps1) | 通用指南推荐的最新版本不等于本项目基线；须检出固定提交、完整子模块并通过环境检查 |
| Arm GNU Toolchain | [Arm 官方旧下载入口](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)、[官方 GitLab 发布仓库](https://gitlab.arm.com/tooling/gnu-toolchains-for-arm) | 官方页面索引说明发布位置迁移；沿用已验证 14.2.Rel1 Windows `arm-none-eabi`。GitLab 本次出现 JS 验证页，没有据此宣称安装包已下载校验 |
| CMake/Ninja | [CMake 下载](https://cmake.org/download/)、[Ninja 官方手册](https://ninja-build.org/manual.html) | STM32 仅维护 CMake+Ninja+Arm GNU；版本和 PATH 以终端实际输出为准 |
| STM32 烧录 | [STM32CubeProgrammer 官方产品页](https://www.st.com/en/development-tools/stm32cubeprog.html)、[固定 2.23.0 CLI 手册](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_Command_Lines.html) | SWD 的 `-c port=SWD`、`-w`、`-v`、`-rst` 有官方定义；VTref/供电仍按具体探针手册，不能把所有廉价 ST-Link 针脚当成一致 |
| 实机记录工具；`tools/live_check.py` | Python 3.11 官方 [json](https://docs.python.org/3.11/library/json.html)、[ipaddress](https://docs.python.org/3.11/library/ipaddress.html)、[urllib.request](https://docs.python.org/3.11/library/urllib.request.html)、[time.monotonic](https://docs.python.org/3.11/library/time.html#time.monotonic) | 标准库负责解析、地址、请求和计时；采集判据、日志字段及通过条件是本项目设计，不是 Python 官方认证 |

## 本次获取限制与更新规则

ST 部分 PDF 直接下载返回 HTTP 567，RM0008 的浏览提取遇到大小限制；本次按可访问的官方章节索引/摘录及 ST 官方 CMSIS 头文件、启动模板交叉核对，没有声称完整 PDF 都已下载。Sensirion PDF、NXP I2C 规范、ST CLI 网页及多份固定 SDK 源文件已实际下载并记录字节哈希。ROHM CDN 的 BH1750 数据表返回 404，旧入口返回 403；随后沿 Adafruit 官方模块下载页找到其指向的 Mouser 镜像，浏览读取了 ROHM 2011.11 Rev.D 原文及指令/时序页。原文作者、镜像托管方和来源链分别记录，不冒充 ROHM 官网直接获取；本机二进制下载超时，因而不生成该 PDF 的本地 SHA256。

后续修改驱动或接线时，先定位相应手册条目，记录所依据版本，再改代码并运行相关测试；涉及真实 GPIO、电源、传感器时按阶段重新验收。应用帧格式、3 秒/5 秒超时、自动滞回、样本有效期、手机请求去重和展示方案是毕设设计，不冠以“厂商强制要求”或“官方认证”。
