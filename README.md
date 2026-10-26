# embedded-smart-home

[English](README.en.md) | 中文

基于 **ESP32-S31-Korvo-1 V1.1** 的智能家居环境监测与控制毕设起步工程。题目对应《智能家居环境监测与控制系统的设计与实现》，优先完成采集、显示、控制、阈值联动和软硬件联调，再逐步增加联网与语音。

**这是 ESP32-S31（`esp32s31`），不是 ESP32-S3（`esp32s3`）。不要选错编译目标。**

首版默认用明确标注的模拟温湿度/光照数据，直接使用板载 RGB 灯、按键与 LCD/触摸交互。无需先购买环境传感器，就能练习完整控制流程。板载没有室内温湿度/光照传感器，也没有风扇；芯片内部温度不能当作室温。

## 已提供的示例

| 资源 | 本工程行为 | 前提与边界 |
| --- | --- | --- |
| RGB LED / WS2812 | 跟随灯光开关，作为第一路真实执行输出 | 官方 BSP 管理 GPIO37；默认颜色由 BSP 决定 |
| 四个 ADC 按键 | SET→自动；PLAY→灯光切换；VOL+→风扇命令切换；VOL−→手动 | BSP 的 `MODE` 对应板上 PLAY；按键短按 |
| 4.3 英寸 LCD + GT1151 触摸 | 环境数据、模式、阈值、命令状态与自检按钮 | 需要原配 SUB3 屏幕；可关闭 UI 用串口运行 |
| ES8389 + 扬声器 | 16 kHz 双声道、1 秒低音量测试音 | 自备连接扬声器；按需执行，无开机响铃 |
| 双麦克风 | 采集 1 秒并输出左右通道峰值 | 原始幅度，不是校准分贝，也没有语音识别 |
| microSD | 可选，每 10 秒追加 `home.csv` | 默认关闭；已格式化 FAT 卡；失败不自动格式化 |
| DVP / OV3660 摄像头 | 可选，初始化传感器并查询设备能力、分辨率与格式 | 默认关闭；这是探测示例，尚无取帧、预览或录像 |
| Wi-Fi | 按需扫描附近 AP 并输出前 8 条 | 不连接网络；扫描结束关闭 Wi-Fi；没有 Web/MQTT |
| PSRAM | 64 KiB 写入/读回自检和容量报告 | 小范围功能检查，不能替代完整内存测试 |
| I2C | 扫描板上/外接设备，复用 BSP 总线 | 不复制 LCD、音频的引脚初始化 |
| 外接 SHT3x + BH1750 | 可选真实温湿度、光照采集 | 默认关闭；包含 CRC、量纲换算、有效性检查 |
| 外接 MCP23017 | 可选 GPA0 灯 / GPA1 风扇输出 | 默认关闭；需要额外驱动级与负载供电；无转速反馈 |

USB Host、蓝牙、语音识别和相机视频留作后续扩展；本版不声称已实现。没有 MCP23017 时，风扇只有软件命令，屏幕和串口会显示外部输出不可用。`ON` 表示控制命令，不代表风扇已经转动。

## 运行环境

- 开发板：ESP32-S31-Korvo-1 V1.1，原配 ESP32-S3-LCD-EV-Board-SUB3（800×480）。
- 主机：Windows 10/11、Linux 或 macOS。Windows 推荐 ESP-IDF 官方命令行/VS Code 扩展；串口波特率 115200。
- SDK：**支持 `esp32s31` 的 ESP-IDF master 预览版本**。官方状态页目前仍要求 master；不要因为 BSP 的低版本下限就安装旧版 IDF。
- 本工程 SDK 基线：`d35ea023029363d97c42565f85e0db23486f26b2`，ESP-IDF 6.2-dev。详细验证结果见 [验证记录](docs/VALIDATION.md)。
- 依赖：官方 `espressif/esp32_s31_korvo_1` **1.0.1**、LVGL 9；由 IDF Component Manager 下载。生成的 `dependencies.lock` 固定解析结果。
- 工具链、Python、CMake、Ninja 由 ESP-IDF 安装流程管理。不需要 Arduino、PlatformIO、Linux 内核或 API Key。

### 安装 SDK（Linux / macOS）

先按[官方入门指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/get-started/index.html)安装系统依赖，再运行：

```bash
git clone https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout d35ea023029363d97c42565f85e0db23486f26b2
git submodule update --init --recursive
./install.sh esp32s31
. ./export.sh
```

Windows 使用支持该目标的官方 ESP-IDF 安装方式；如已有源码目录，在 PowerShell 中检出上述提交，更新子模块后执行 `install.ps1 esp32s31` 和 `export.ps1`。如果安装器提供的 SDK 列表没有 S31，使用源码安装，不要改成 S3 目标凑合编译。每个新终端都需要激活相应 SDK。

### 获取、配置、编译

在已激活的 ESP-IDF 终端中：

```bash
git clone https://github.com/xuan511/embedded-smart-home.git
cd embedded-smart-home
python tools/check_environment.py
idf.py --preview set-target esp32s31
idf.py --preview menuconfig
idf.py --preview build
```

首次可直接使用默认配置。`menuconfig → Smart Home` 中可以开关 LCD、真实传感器、SD 日志、相机探测和 MCP23017 输出。使用 `sdkconfig.defaults` 作为初始配置；修改它不会自动覆盖已经生成的 `sdkconfig`。重新应用默认配置前先备份个人配置，再运行 `idf.py --preview fullclean` 并移走 `sdkconfig`。

### 接线与烧录

1. 断电连接 LCD 和可选扬声器/SD 卡。先用裸板示例，不接外部负载。
2. 将有数据线的 USB 线接到 **UART Type-C**，打开电源开关。另一个 **Power Type-C 仅供电**，不能烧录；Type-A 是 USB Host。
3. 驱动扬声器等较大负载时，按官方指南另接独立电源线；不要让电机从 MCU GPIO 取电。
4. 查询串口号，执行下列命令，将 `COM5` 换成实际端口：

```powershell
idf.py --preview -p COM5 flash monitor
```

Linux 常见端口示例：

```bash
idf.py --preview -p /dev/ttyUSB0 flash monitor
```

若无法自动进入下载模式，按住 BOOT，按下并松开 RESET，再松开 BOOT 后重试。串口监视器用 `Ctrl+]` 退出。固件会显示 `SIMULATED SENSORS`，并在串口提供 `home>` 提示符。

## 第一次演示：五分钟验证闭环

输入命令后稍等约 1 秒，再读状态；命令先入队，`Queued` 只表示已接收。

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

预期结果：

- `light on/off` 能切换板载 RGB 灯，同时进入手动模式。
- 自动模式下，`30°C / 50 lux` → 风扇命令 ON、灯 ON。
- `27°C / 150 lux` 位于滞回区 → 保持先前开关状态，不反复抖动。
- `25°C / 300 lux` → 两个命令均 OFF。
- `fault` 注入传感器失效，自动模式下两路输出关闭，显示 FAULT；下一条 `sim` 恢复数据。
- 手动模式下，模拟故障仍显示故障，但不会替你改变手动命令；这只是桌面演示策略。

### 命令表

| 命令 | 用法 |
| --- | --- |
| `home status` | 数据、模式、阈值、故障、输出可用性、队列拒绝次数 |
| `home mode auto` / `home mode manual` | 自动/手动模式；切回自动立即按规则计算 |
| `home light on/off` / `home fan on/off` | 手动控制；会将整个控制器切到手动模式 |
| `home thresholds 29 27 80 180` | 风扇开/关温度，灯光开/关照度；修改后先保存在 RAM |
| `home save` | 将当前阈值保存到 NVS；模式和开关状态不保存，重启回到自动模式 |
| `home sim 30 55 50` | 注入温度、湿度、照度；仅模拟构建可用，数值持续直到再次修改 |
| `home fault` | 模拟传感器故障；仅模拟构建可用 |
| `home test i2c` | 扫描 7 位地址并打印 ACK |
| `home test memory` | PSRAM 小范围读写检查 |
| `home test tone` / `home test mic` | 扬声器测试音 / 双麦克风峰值 |
| `home test camera` | 相机传感器/格式探测；先启用 `HOME_CAMERA` |
| `home test wifi` | 扫描附近 Wi-Fi AP，无需 Wi-Fi 密码 |

UI 与实体按键走相同消息队列，不直接在回调里做音频/SD/网络操作。自检由独立低优先级任务串行执行；队列满会报告失败，不会无限排队。

## 从模拟接入真实硬件

先读 [硬件与外设说明](docs/HARDWARE.md)。默认不指定“空闲 GPIO”，因为 LCD、相机、音频和 SD 已占用大量引脚。

1. 在实际原理图上确认可访问的 **GPIO0/SDA、GPIO1/SCL、3.3V、GND**，再并接 3.3V 的 SHT3x-DIS 和 BH1750 模块。不要把模组引脚编号当成 GPIO 编号，也不要假设有杜邦排针。
2. 运行 `home test i2c` 确认地址。SHT 默认 `0x44`（可选 `0x45`）；BH1750 默认 `0x23`（可选 `0x5C`）。
3. 在 `menuconfig → Smart Home` 打开真实传感器开关，并填写地址；重新编译、烧录。屏幕应改成 `EXTERNAL SENSORS`、串口改成 `REAL`。
4. 拔掉其中一路传感器，验证对应故障与输出关闭；另一条规则继续运行。真实构建拒绝 `home sim`，不会自动拿模拟值补偿掉线。
5. 需要真实风扇时，再接可选 MCP23017 和 MOSFET/续流二极管/独立电源。只对低压直流灯和小风扇做桌面演示，详见接线文档。

## 代码结构与阅读顺序

| 路径 | 职责 |
| --- | --- |
| `docs/ARCHITECTURE.md` | 任务、数据流、规则与扩展接口 |
| `main/app_main.c` | 启动 NVS、控制服务、自检、UI、串口 |
| `components/home_core/` | 与硬件无关的 C 状态、阈值、滞回、数据有效性、SHT CRC |
| `main/home_service.c` | 控制任务、消息队列、快照、NVS、CSV、串口命令 |
| `main/board_io.c` | RGB、ADC 按键、可选 MCP23017 |
| `main/sensors.c` | 可选 SHT3x / BH1750 真实采集 |
| `main/ui.c` | LVGL 9 仪表盘与触摸操作 |
| `main/diagnostics.c` | I2C、PSRAM、音频、相机探测、Wi-Fi 自检 |
| `tests/`、`tools/` | 纯 C 控制逻辑测试与环境检查 |

建议从 `home_core.h → home_core.c → tests/test_core.c → app_main.c → home_service.c` 开始。每加一个传感器，优先让串口显示可信数据，再连入自动控制，最后增加界面；不必一次看完全部驱动。

## 测试与后续开发

无开发板也可以在有 C 编译器与 Python 3 的电脑上执行：

```bash
python tools/test_host.py
```

覆盖启动关闭、滞回边界、独立故障、5 秒数据超时、手动/自动切换、非法数值和 SHT CRC。它不证明硬件已经工作。固件编译、上板验收和稳定性记录分别列在 [验证记录](docs/VALIDATION.md) 与 [分阶段开发计划](docs/ROADMAP.md)。

后续顺序：**板载演示 → 真实环境传感器 → 真实低压灯/风扇 → 阈值与异常联调 → 连续运行测试 → 可选 Web/MQTT/语音**。暂不引入自研 PCB、OTA、云平台或 AI 依赖。

常见问题：

- **找不到 `esp32s31`**：当前激活的 SDK 太旧或路径不对；检查 `python tools/check_environment.py`，不能改选 S3。
- **组件下载失败**：首次构建需要访问 Espressif Component Registry 与 GitHub，检查代理/证书。不要随意删除版本约束。
- **LCD 黑屏/复位**：确认原配屏、连接方向、供电与 PSRAM 配置；在 menuconfig 关闭 UI，先用串口缩小问题范围。
- **按键不匹配**：本工程按 BSP V1.1 映射；通过串口观察并核对硬件版本，不复制其他 Korvo 板的 ADC 阈值。
- **显示 ON 但风扇不转**：默认没有外部执行电路；查看 `MCP=0`。本工程目前没有转速或电流反馈。
- **SD 不记录**：默认关闭；启用后需 FAT 卡，文件名 `home.csv`。失败后本次运行停止记录，排查后重启；不会自动格式化。

## 官方资料与许可

- [Korvo-1 V1.1 用户指南与原理图入口](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)
- [ESP32-S31 软件支持状态](https://developer.espressif.com/hardware/esp32s31/)
- [官方 BSP 1.0.1](https://components.espressif.com/components/espressif/esp32_s31_korvo_1/versions/1.0.1)
- [乐鑫 BSP 源码与示例](https://github.com/espressif/esp-bsp/tree/master/bsp/esp32_s31_korvo_1)

项目自身代码采用 [MIT](LICENSE)。外部组件保留各自许可，未将官方 BSP 源码复制进仓库。
