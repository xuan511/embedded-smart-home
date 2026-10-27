# embedded-smart-home

[English](README.en.md) | 中文

《智能家居环境监测与控制系统的设计与实现》双板工程。目标是完成一次真实的 **采集 → LCD 显示 → 触摸/串口操作 → 执行 → 状态返回** 演示，优先保持接线和代码简单。

| 开发板 | 分工 |
| --- | --- |
| ESP32-S31-Korvo-1 V1.1 | 原配 LCD/触摸、调试串口、NVS 阈值保存、可选 microSD 日志 |
| STM32F103C8T6 学习板 | SHT3x 温湿度、BH1750 光照、灯/低压直流风扇开关、自动规则 |
| 两板连接 | Korvo **USB-A Host → 指定 CP2102N USB 转串口模块 → STM32 USART1**，115200/8N1 |

ESP 目标必须为 **`esp32s31`**，不套用 `esp32s3` 的引脚和配置。STM32 按官方 **64 KB Flash / 20 KB SRAM** 设计。本方案不使用学习板上尚未确认的 USB 接口；不从 Korvo 的 LCD、SD 或其他 GPIO 引出传感器，不改主板。

**当前没有连接实物板，所有真机联调均未进行。** 代码完成、编译通过、主机测试通过、真机验证通过分别记录于 [VALIDATION](docs/VALIDATION.md)。编译或测试数据不等于实测。前一步在板上验收通过，再增加下一步外设。

Korvo 是**人机操作、参数和显示的上位控制中枢**；STM32 是**采集与执行节点**，按既定需求独立执行本地自动规则。官方能力、固定源码和社区证据见 [支持性与验证溯源](docs/SUPPORT_EVIDENCE.md)。

## 接线前先看

[硬件清单、实际接口、供电与接线表](docs/HARDWARE.md)是接线依据；[硬件核查](docs/HARDWARE_AUDIT.md)区分官方事实和待确认项。仅需集中确认这些影响接线/烧录的信息：

1. STM32 学习板正反面/原理图：3V3、GND、PA9/PA10、PB6/PB7、PB0/PB1、SWD 是否有可插接排针；板载 LED 的 GPIO 和有效电平。不能仅凭“蓝色板”判断。
2. 转串口模块的**芯片型号、USB VID/PID、UART 电平和排针定义**。本版只实现 CP2102N 路径，不把 CH340、FT232 等当成同一驱动；“3.3V 电源输出”不等于 TXD 一定是 3.3V。
3. STM32 的供电入口与稳压电路，以及灯/风扇额定电压、电流、驱动模块的 3.3V 输入能力。先确认再上电，不同时接入可能互相回灌的电源。

Korvo 原配 LCD/触摸保持连接，可插入 FAT microSD。模块 TXD 接 PA10/RX，RXD 接 PA9/TX，GND 共地；模块 VCC 默认不接 STM32。STM32 使用已确认的独立合适电源。灯和风扇均经驱动级，禁止 GPIO 直接供电。完整信号表见硬件文档。

## Windows 安装环境

建议将 SDK 和工程放在没有空格的短路径，例如 `C:\esp\esp-idf` 和 `C:\esp\embedded-smart-home`。安装 [Git for Windows](https://git-scm.com/download/win)、Python 3.11/3.12，以及 [ESP-IDF 官方工具和前置环境](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/get-started/windows-setup.html)。若使用 EIM，在 Manage Installations 中打开对应的 IDF Terminal。

本工程继续固定原仓库的 SDK 基线，没有为了新增功能升级 SDK。在 PowerShell 中：

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

若脚本被阻止，可只为当前终端执行 `Set-ExecutionPolicy -Scope Process Bypass`，再安装/激活。每个新终端都需激活这一份 SDK。首次构建需访问 GitHub 和 Espressif Component Registry。依赖由 `main/idf_component.yml`、`dependencies.lock` 获取，包括官方 **BSP 1.0.1**、LVGL 9 和 USB Host 驱动；不要换成另一款 Korvo BSP。

若提示 `Filename longer than 260 characters`，先把 SDK、工具目录和工程移到上述短路径后重新构建。本轮本地深目录的可选配置使用了 [Ninja 1.13.2 官方发行版](https://github.com/ninja-build/ninja/releases/tag/v1.13.2)处理长路径；ESP-IDF/BSP 版本保持不变。

STM32 只采用 **Arm GNU Toolchain + CMake + Ninja**。安装 [Arm 官方工具链](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)，选择 Windows 的 `arm-none-eabi`，把其 `bin` 加入 PATH；CMake/Ninja 可复用 ESP-IDF 终端中的版本。烧录安装 [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)，使用 ST-Link。无需 Keil、Arduino、PlatformIO、FreeRTOS；STM32 源码无外部 SDK 下载依赖。

```powershell
arm-none-eabi-gcc --version
cmake --version
ninja --version
git clone https://github.com/xuan511/embedded-smart-home.git C:\esp\embedded-smart-home
Set-Location C:\esp\embedded-smart-home
```

## 分别构建与烧录

### Korvo：默认 USB 实机模式

在激活 SDK 的工程根目录执行：

```powershell
python tools/check_environment.py
idf.py --preview set-target esp32s31
idf.py --preview build
idf.py --preview -p COM5 flash monitor
```

将 `COM5` 换成设备管理器中 **Korvo UART Type-C** 对应端口。Power Type-C 仅供电，USB-A 为 Host；三个接口用途不同。自动下载失败时，按住 BOOT，按下并松开 RESET，再松开 BOOT 后重试。`Ctrl+]` 退出 monitor。

默认未连接 STM32 时显示断开/等待，**不会生成模拟环境数据**。`idf.py --preview menuconfig → Smart Home` 可显式选择模拟模式（`CONFIG_HOME_SIMULATION`，默认关闭）、SD 日志和 LCD 开关。`configs/simulation.defaults` 提供显式模拟配置。修改默认配置不会覆盖已有 `sdkconfig`，更改后需重新构建/烧录。日志失败不格式化 SD 卡，不阻止核心控制。

`check_environment.py` 会拒绝 SDK 提交不符、SDK 跟踪源码被修改、递归子模块缺失/不符、跳过子模块检查或 GCC 版本不符的环境。先完成官方安装流程，再编译；检查通过只表示环境一致，不表示真机通过。

### STM32：先只构建阶段 1

在工程根目录执行；工具链文件由 CMake 自动加载：

```powershell
cmake -S stm32 -B stm32/build-stage1 -G Ninja -DSTAGE=1
cmake --build stm32/build-stage1
```

产物为 `home_stm32.elf`、`.bin`、`.hex`、`.map`。工程包含启动文件、向量表、64KB/20KB 链接脚本及配置，使用内部 HSI 8 MHz，不依赖学习板外部晶振。

断电连接 ST-Link：**SWDIO→PA13、SWCLK→PA14、GND→GND、VTref→目标 3V3**；NRST 按实际板接入。VTref 是目标电压参考，不能假定它给目标供电。按硬件文档单独供电，BOOT0 保持 0；STM32 板上 USB 本方案不用。

打开 STM32CubeProgrammer，选择 **ST-LINK / SWD**，先 Connect 读取芯片，再选 `stm32/build-stage1/home_stm32.elf`，执行 Download、Verify、Reset。若 CLI 已加到 PATH：

```powershell
STM32_Programmer_CLI -c port=SWD -w stm32/build-stage1/home_stm32.elf -v -rst
```

连接失败先查供电、共地、SWD、BOOT0 和芯片型号；必要时接 NRST 并选择 Under Reset。不要把板上有 USB 插座当成能下载的依据。

## 按顺序完成五步实机验收

更换 STM32 阶段时使用独立构建目录，重新编译/烧录。Korvo 保持 USB 实机固件。每步只接相应硬件，预期和空白记录表见 [ACCEPTANCE](docs/ACCEPTANCE.md)。

| 步骤 | STM32 配置 | 操作与预期 |
| --- | --- | --- |
| ① 通信最小闭环 | `-DSTAGE=1` | 仅 USB-UART/供电；握手、配置应答、心跳；无环境读数；拔线显示断开，重插重新同步 |
| ② 测试数据 + LED | `-DSTAGE=2` | 明确标记 TEST DATA：30°C、55%、50 lux；`home light on/off` 改变板载 LED 并收到状态应答；风扇请求拒绝 |
| ③ 真实传感器 | `-DSTAGE=3` | SHT3x/BH1750 约每秒更新，STM32 SENSORS 标签；遮光/温度变化有趋势；单传感器断开显示故障；仅 LED 演示，风扇请求拒绝 |
| ④ 实际灯/风扇 | `-DSTAGE=4` | 接驱动和独立低压负载电源；先手动开关，再自动阈值/滞回；记录真实动作 |
| ⑤ 保存/异常 | 阶段 4 | 保存阈值、重启/重连、失联策略、数据过期、传感器故障、连续运行 |

例如第二阶段：

```powershell
cmake -S stm32 -B stm32/build-stage2 -G Ninja -DSTAGE=2
cmake --build stm32/build-stage2
STM32_Programmer_CLI -c port=SWD -w stm32/build-stage2/home_stm32.elf -v -rst
```

默认 LED 为 **PC13 低电平亮**，须先核对实际板。可用 `-DLED_PORT=C -DLED_PIN=13 -DLED_ACTIVE_LOW=1` 配置，端口支持 A/B/C。前两步没有真实采集；全部实机现象必须现场补记，不能预填 PASS。

## 操作、阈值与状态

触摸、实体按键和串口共用有界请求队列。LCD 主页显示读数、模式、灯/风扇返回状态和必要按钮；阈值用 Korvo 调试串口修改：

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

每条操作后等约 1 秒，再用 `home status` 看返回；`pending=1` 时继续等待，busy 拒绝则稍后重试。`thresholds` 依次为风扇开启/关闭温度（°C）、灯开启/关闭照度（lux）。要求风扇关闭阈值小于开启阈值、灯开启阈值小于关闭阈值；默认 28/26°C、100/200 lux。温度阈值保留 2 位小数，照度保留 1 位，舍入后滞回不能消失。湿度仅显示。`save` 存到 Korvo NVS，STM32 只持运行时副本；保存失败按日志处理。

- 手动负载命令切换到 MANUAL；所有 AUTO 规则由 STM32 执行。
- 高温开风扇、低照度开灯，滞回区保持状态。对应数据无效/过期时，AUTO 下对应演示负载关闭。
- 上电/重连：新会话 HELLO → READY → Korvo 下发有效阈值 → 成功应答后允许控制。STM32 旧阈值不回写 NVS，旧手动请求不重放。
- USB 拔出或约 3 秒无有效消息：Korvo 标记失联/过期，旧数字不能显示为正常。STM32 MANUAL 最迟约 5 秒失联后关闭两路负载，新会话重握手时可能提前关闭；AUTO 继续按本地有效数据运行。
- 请求约 1.5 秒无应答时结果为未知，并重建会话同步；不会把未确认请求显示为已执行。
- **请求排队、STM32 返回的 GPIO 输出状态、真实负载动作是三件事。** 无转速/电流采集时，ON 不证明风扇已经转动；实际动作单独验收。

无硬件时可在 menuconfig 显式选择模拟模式，页面标记 OFFLINE SIMULATION。`home sim 30 55 50`、`home fault` 仅模拟构建可用；USB 实机断开不会回退到模拟。

## 测试、源码与排错

有 Python 和主机 C 编译器时：

```powershell
python tools/test_host.py
```

主机测试覆盖控制逻辑、协议边界及端点状态处理，不能替代硬件测试。[协议](docs/PROTOCOL.md)规定帧边界、字段和重连；[架构](docs/ARCHITECTURE.md)说明状态归属；[验收](docs/ACCEPTANCE.md)提供上板记录模板；[ROADMAP](docs/ROADMAP.md)只包含本轮范围。

| 路径 | 职责 |
| --- | --- |
| `main/` | Korvo 服务、USB Host、LVGL、NVS、串口、可选 SD/诊断 |
| `components/home_core/` | 纯 C 有效性、滞回、SHT CRC |
| `components/home_link/` | 两端共用的 ASCII 行协议 |
| `components/home_remote/` | Korvo 纯 C 会话、同步、请求与过期处理 |
| `stm32/` | 主循环、实际 UART/I2C/GPIO、启动/链接/构建 |
| `tests/`、`tools/` | 主机测试、环境检查 |

目标找不到时激活正确 SDK；组件下载失败先查网络/证书。USB 未枚举先核对模块芯片/VID/PID；枚举但无握手再查 UART TX/RX 交叉、3.3V 电平、STM32 阶段。LED 无反应先查 GPIO/极性。LCD 黑屏检查原配屏、FPC 方向和供电。排错先断开负载，回到最小通信链路。

项目代码采用 [MIT](LICENSE)，外部依赖保留各自许可。官方资料见 [硬件文档](docs/HARDWARE.md)。本轮不增加 AI、语音、云平台、App、MQTT、Matter、OTA、自研 PCB 或相机视频；已有自检仅作可选串口工具。
