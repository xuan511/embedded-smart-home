# 开源项目对照与本工程取舍

核对日期：2026-11-12，检查基线 `a1300be`。本轮实际读取了下面六个项目的固定源码与许可证。目标是吸收容易解释和验证的小做法；不是把别人的智能家居平台搬进本工程，也不把“开源项目可以运行”写成本项目真机通过。

资料角色分开：器件电气、引脚、传感器时序以[官方手册对应表](OFFICIAL_REFERENCE.md)为依据；开源示例用于核对 API 使用和实现模式；ASCII 协议、自动滞回、状态权威、超时和手机展示是本项目设计。具体获取记录见 [open-source-reference-manifest.json](open-source-reference-manifest.json)。

## 1. 读过哪些项目，保留什么

| 作者仓库及固定版本 | 实际检查的文件 | 可用的小模式 | 本工程的对应处与处理 | 不直接照搬的内容 |
| --- | --- | --- | --- | --- |
| [espressif/esp-idf](https://github.com/espressif/esp-idf/tree/d35ea023029363d97c42565f85e0db23486f26b2) | [Wi-Fi station](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/examples/wifi/getting_started/station/main/station_example_main.c)、[CDC Host](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/examples/peripherals/usb/host/cdc/main/usb_cdc_example_main.c) | 用事件管理连接；得到 IP 才视为联网；USB 先选专用桥驱动、设置串口参数，再处理收发/断线 | `main/home_network.c` 的 Wi-Fi 事件与 `main/home_usb.c` 基线已有相应结构，本轮复核保留；网络失败要区分获取 IP、Broker 和 STM32，而不能只有一个“离线” | 示例的启动阻塞、最大重试次数、多个桥/多设备、退出 GPIO、默认引脚不是本毕设配置；保留固定 S31/BSP，不换 S3 |
| [espressif/esp-usb](https://github.com/espressif/esp-usb/tree/ebfa6f84ee7c7c070dcc244c79573eeab228298e) | [CP210x C API](https://github.com/espressif/esp-usb/blob/ebfa6f84ee7c7c070dcc244c79573eeab228298e/host/class/cdc/usb_host_cp210x_vcp/include/usb/vcp_cp210x.h) | 依据 VID/PID 与接口选择具体设备，不能把不同 USB-UART 当成同一类驱动 | 复核 `cp210x_vcp_open(CP210X_PID, 0, ...)`；继续只选默认 `10C4:EA60` 单口桥，基线 managed component 2.2.0 不变 | 不扩 CH340/FTDI/PL2303；不因仓库支持多芯片就声称现有未知模块兼容；不加入 Hub |
| [espressif/esp-iot-solution](https://github.com/espressif/esp-iot-solution/tree/7a813417274c663226601f390668b9c8e5eec768) | [BH1750 驱动](https://github.com/espressif/esp-iot-solution/blob/7a813417274c663226601f390668b9c8e5eec768/components/sensors/light_sensor/bh1750/bh1750.c) | 先查 I2C 操作结果，再按测量模式等待和换算；失败不当成正常测量 | 对照 `stm32/src/sensors.c` 的单次 H 模式、最长等待和 `/1.2`；本工程每通道有效位仍独立。这里是对既有驱动的复核，不是本轮首次接入真实传感器 | ESP 的 I2C/FreeRTOS 实现不直接放进 STM32；不增加自适应量程和传感器平台；原厂 ROHM 原文仍是指令/电气依据 |
| [espressif/esp-mqtt 1.0.0](https://github.com/espressif/esp-mqtt/tree/2c3e1e0942afcc5b3cea2710ffe220fe699da02a) | [TCP 示例](https://github.com/espressif/esp-mqtt/blob/2c3e1e0942afcc5b3cea2710ffe220fe699da02a/examples/tcp/main/app_main.c)、[事件核心](https://github.com/espressif/esp-mqtt/blob/2c3e1e0942afcc5b3cea2710ffe220fe699da02a/mqtt_client.c) | 区分已连接、连接拒绝和传输错误；保留错误类型、Broker 返回码与 socket errno，再给用户排错方向 | 本轮网络诊断落点为 `main/home_network.c` 的 MQTT 事件、串口/JSON 与教程；不能仅由 `mqtt=false` 猜“密码错”。具体结果以本轮实现与测试为准 | 示例里的订阅/测试发布不作为执行器控制；不照搬 stdin Broker 配置、云服务或大型 outbox；保持每秒状态上传和手机 LAN HTTP |
| [Sensirion/embedded-sht](https://github.com/Sensirion/embedded-sht/tree/94967774f25985bf65356bad3689fec4e8fef2b8) | [SHT3x 厂家 C 驱动](https://github.com/Sensirion/embedded-sht/blob/94967774f25985bf65356bad3689fec4e8fef2b8/sht3x/sht3x.c) | 命令、测量等待、带返回码的读取分开；SHT3x 测量包含温湿度与 CRC，失败不能伪装正常 | 对照 `stm32/src/sensors.c` 与 `components/home_core/home_core.c`，继续用不拉伸命令 `0x2400`、两字节数据各自 CRC 检查、异常标志 | 不引入整套厂家 HAL/Makefile 或第二套工具链，不把 SHT4x/SHTC1 命令当 SHT3x；具体等待/电压取决于对应器件数据表 |
| [esphome/esphome](https://github.com/esphome/esphome/tree/0240f570699283f2fa3b30211b5f3caded261335) | [BH1750](https://github.com/esphome/esphome/blob/0240f570699283f2fa3b30211b5f3caded261335/esphome/components/bh1750/bh1750.cpp)、[SHT3xD](https://github.com/esphome/esphome/blob/0240f570699283f2fa3b30211b5f3caded261335/esphome/components/sht3xd/sht3xd.cpp) | 采集失败显式发布无效/警告，而不是让上次数字看似正常；测量阶段与发布分离的思路可供阅读 | 本工程已经用独立故障位、过期年龄和 HTTP/MQTT 的 `null`；对照后保留此简单实现，继续测试“失败仍有旧数值”不会显示 OK | 不复制 C++ runtime，不引入 ESPHome/Home Assistant、YAML、Python 生成器、自适应量程或其默认引脚；它不提供本项目 S31 双板实测证明 |

所有引用都固定到实际读取的提交。没有因社区项目出现更新而升级本仓库的 ESP-IDF/BSP 或 STM32 构建方式。

## 2. 这轮应该如何落到可验证改进

网络排错首先回答三个不同问题：

1. Korvo 是否连到 AP 并取得 IP？Wi-Fi 断线事件及原因码可提供线索。官方 station 示例的通用 fail 日志本身不能证明密码错误；原因值应再对照固定 SDK 的 Wi-Fi 类型定义。
2. Korvo 是否连到 MQTT Broker？TCP 连接错误与 Broker 拒绝是不同层；保留官方事件中的错误类型/返回码/errno，再由教程指导检查地址、进程、认证和防火墙。
3. STM32 是否握手并正常上报？即使 Wi-Fi/MQTT 正常，USB 与传感器仍可能断开。继续保留 USB online、synced、来源、故障与年龄字段，不能把 MQTT connected 当成全屋设备正常。

对于从零复现，本轮应把这些层对应到一张可勾选的进度表和明确的停止门槛。编译只证明固件可生成；电脑识别 CP210x 只证明电脑端识别桥；HTTP 202 只证明请求入队；STM32 ACK 只证明节点应答。传感器真实变化和风扇转动仍需现场观察。

对应回归应该覆盖：已知错误类型能保留、未知数字码不编造解释、断网后旧 IP/正常标签失效、恢复连接后当前错误不残留；以及断网不改变 STM32 输出权威。没有实物时，这些只记为代码/主机验证，实际 ESP 网络事件和电气行为仍为 NOT RUN。

## 3. 许可证怎么处理

本轮没有导入新的第三方源码或运行平台，只阅读实现并按本工程结构做小改动。既有 managed components 的许可证仍随依赖保留。

| 项目 | 已实际核对的许可证位置 | 处理方式 |
| --- | --- | --- |
| ESP-IDF | [仓库 LICENSE](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/LICENSE) 为 Apache-2.0；被读的 Wi-Fi 文件声明 Public Domain/CC0，CDC 文件 SPDX 为 CC0-1.0 | 区分仓库总许可与具体示例的许可，不把整个仓库一律称 CC0 |
| esp-usb CP210x | [组件 LICENSE](https://github.com/espressif/esp-usb/blob/ebfa6f84ee7c7c070dcc244c79573eeab228298e/host/class/cdc/usb_host_cp210x_vcp/LICENSE) 与 API 文件 SPDX 为 Apache-2.0 | 仓库根 LICENSE 查找未返回内容，已改核对实际使用组件的许可；不伪造一个根文件 |
| esp-iot-solution | [LICENSE](https://github.com/espressif/esp-iot-solution/blob/7a813417274c663226601f390668b9c8e5eec768/LICENSE) 与被读 BH1750 文件 SPDX 为 Apache-2.0 | 仅作指令/等待/返回值交叉核对，无整套平台复制 |
| ESP-MQTT | [LICENSE](https://github.com/espressif/esp-mqtt/blob/2c3e1e0942afcc5b3cea2710ffe220fe699da02a/LICENSE) 为 Apache-2.0；被读 TCP 示例声明 Public Domain/CC0 | 只采用事件分层方式，不导入示例应用和控制主题 |
| Sensirion embedded-sht | [LICENSE](https://github.com/Sensirion/embedded-sht/blob/94967774f25985bf65356bad3689fec4e8fef2b8/LICENSE) 为 BSD-3-Clause，SHT3x 文件带对应许可文字 | 厂家驱动作为核对来源；未把其 HAL/构建文件并入本仓库 |
| ESPHome | [LICENSE](https://github.com/esphome/esphome/blob/0240f570699283f2fa3b30211b5f3caded261335/LICENSE) 明确 runtime 的 C/C++ 为 GPLv3，Python/其他部分为 MIT | 本轮不复制 runtime C++。不能把全仓库一律称 MIT 后搬代码；如以后引入源码应先单独评估并保留相应许可 |

没有许可证、没有实际读取源码、或仅有演示视频的项目，不作为本轮可复制代码来源。星标数不证明硬件接线正确、驱动适配本板或符合本项目验收。

## 4. 从零学习时怎样使用这份对照表

先按[完整入门教程](BEGINNER_GUIDE.zh-CN.md)完成自己的通信闭环，再阅读一小段相应来源，不要先下载六套工程一起安装：

| 当前进度 | 只读哪一小段 | 应能用自己的话说明 |
| --- | --- | --- |
| USB-A 与 STM32 通信 | IDF CDC 示例的 CP210x 打开、串口配置和 disconnect case | 为什么需要特定桥芯片驱动，为什么 TX/RX/GND 三线与 USB 插座不是一回事 |
| 温湿度和光照 | Sensirion SHT3x 的测量读取、Espressif BH1750 的读取/换算 | 为什么要等待、CRC/返回值为何不能忽略，哪个通道无效 |
| Korvo 联网 | IDF station 的 START/DISCONNECTED/GOT_IP 三类事件 | 连接 AP、取得 IP、连接 Broker 是哪几步，为什么断线不能继续标正常 |
| MQTT 上传 | ESP-MQTT TCP 示例的 CONNECTED/DISCONNECTED/ERROR | 什么是连接成功、拒绝、传输错误，为什么上报开关不代表负载反馈 |
| 故障与界面 | 对照 ESPHome 的 warning/无效值思想，再看自己代码的 null/stale | 为什么保留旧数字但标正常会误导，以及本项目如何避免 |

每步的“通过”只由自己当前准确提交的构建日志、测试日志与实物记录支持。复用开源思路的目的，是让自己的实现更清楚、更容易查错；毕设材料仍须区分引用来源、本项目设计和实测结果。
