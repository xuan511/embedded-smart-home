# MQTT 与微信小程序 / MQTT and WeChat mini program

本轮按新需求增加网络功能，固定 ESP-IDF/BSP、STM32 接线和 USB 协议不变。**网络代码已实现；主机集成测试通过不等于 ESP32 联网、微信真机或实物负载验证。** 实测分项见 [VALIDATION](VALIDATION.md)。

CI 另有 `network` 编译配置，启用 LCD/USB/SD/Wi-Fi/MQTT 的完整链接，使用明显的编译占位凭据和保留文档 IP。它用于避免空 SSID 导致网络路径被优化掉；**该产物不能直接当成你的联网演示固件**，需按下文填入自己的配置并重新编译。

## 数据流与边界

```text
SHT31-DIS / BH1750 -> STM32 -> USART1 -> CP2102N -> Korvo USB-A Host
                        |                           |
               PB0/PB1 -> MOSFET -> 灯/风扇        LCD/触摸、NVS、可选 SD
                                                    |
                                  Wi-Fi -> 局域网 MQTT Broker（PC）
                                                    |
                           同网段手机微信小程序 --HTTP--> Korvo
```

Korvo 是统一操作与显示中枢；所有本地/手机控制经过同一个队列和 USB 会话。STM32 负责采集、GPIO 输出和自动滞回，断网不影响本地自动规则。手机不直接连接 STM32，也不必安装 MQTT 库。MQTT 只上传状态；小程序使用 Korvo 的带令牌 HTTP 接口，这样无需另外部署后端或 MQTT WebSocket 服务。

**“STM32 通过 USB 连接”在本项目中明确指 USB-A → CP2102/CP2102N → UART 转接链路，并非使用 STM32 学习板的 USB 插座。** 只支持默认 `10C4:EA60` 单口、接口 0 的 CP210x，UART 为 3.3V。完整模块清单、实际连接器和供电见 [HARDWARE](HARDWARE.md)。不加 Hub、不占 Korvo 的 LCD/SD 引脚。

手机和 Korvo 必须在同一网段，连接普通路由器 Wi-Fi；关闭该网络的 AP/客户端隔离。IP 从 LCD 或串口 `net status` 查看。官方允许同网段非本机 IP 的 HTTP 连接，见 [微信网络文档](https://developers.weixin.qq.com/miniprogram/dev/framework/ability/network.html)、[局域网通信](https://developers.weixin.qq.com/miniprogram/dev/framework/ability/mDNS.html)。本项目手动输入 IP，不依赖 mDNS。**电脑开发者工具模拟器可用不代表手机预览可用**，应在手机上按下表实测；局域网权限被系统拒绝时需在系统设置允许。

本版访问令牌、Wi-Fi 密码和 MQTT 密码从本机 `menuconfig` 配置，`sdkconfig` 不提交。令牌提供访问控制；本版 HTTP/MQTT TCP 不提供传输加密，设计范围是本人可信局域网，不配置公网端口映射。

## Windows：启动 MQTT 服务器

安装 [Eclipse Mosquitto 官方 Windows 版本](https://mosquitto.org/download/)，使用其 `mosquitto.exe`、`mosquitto_passwd.exe`、`mosquitto_sub.exe`。本项目不要求安装 Windows 服务。在仓库根目录的 PowerShell 执行：

```powershell
Copy-Item network/mosquitto.conf.example network/mosquitto.conf
# 编辑 password_file，改成你的仓库实际绝对路径。这里假定 C:\esp\embedded-smart-home。
& 'C:\Program Files\mosquitto\mosquitto_passwd.exe' -c network/passwd home
# 交互输入并记住 MQTT 密码；不要提交 network/passwd。
& 'C:\Program Files\mosquitto\mosquitto.exe' -c network/mosquitto.conf -v
```

如果安装时已启动系统服务占用 1883，先使用已有服务的正确配置，或停止该服务后运行上述前台进程，不同时开两个同端口 Broker。在 Windows 防火墙提示中仅允许所用的专用局域网；用 `ipconfig` 找 PC 的 Wi-Fi/Ethernet IPv4，例如 `192.168.1.10`。不要把 `127.0.0.1` 填进 Korvo，那个地址是 Korvo 自己。

另开 PowerShell 观察上传（`YOUR_MQTT_PASSWORD` 换成本机密码；注意终端历史中会出现该参数）：

```powershell
& 'C:\Program Files\mosquitto\mosquitto_sub.exe' -h 127.0.0.1 -p 1883 -u home -P 'YOUR_MQTT_PASSWORD' -t 'home/korvo01/#' -v
```

配置依据：[Mosquitto 认证](https://mosquitto.org/documentation/authentication-methods/)、[配置文件说明](https://mosquitto.org/man/mosquitto-conf-5.html)。

## Windows：配置并烧录 Korvo

先按主 README 安装并激活固定 SDK，构建目标仍为 `esp32s31`。未填 SSID 时网络不启动，原有 LCD/USB 功能可独立工作。

```powershell
python tools/check_environment.py
idf.py --preview menuconfig
```

在 **Smart Home → Network** 填写：

| 配置 | 示例（都需要替换成本机值） |
| --- | --- |
| Compile Wi-Fi, LAN phone API and MQTT upload | 开启 |
| Wi-Fi SSID / password | 路由器 Wi-Fi；建议先用普通 2.4GHz 网络联调 |
| LAN API token | 16～64 位英文字母、数字、`_`、`-`；手机填相同值 |
| Unique MQTT device ID | `korvo01`，同一 Broker 每台设备用不同 ID |
| LAN broker URI | `mqtt://192.168.1.10:1883`；留空只禁用 MQTT 上传 |
| MQTT username / password | `home` 和刚才设置的密码 |

保存后：

```powershell
idf.py --preview build
idf.py --preview -p COM5 flash monitor
```

COM5 替换成 Korvo **UART Type-C** 的端口；USB-A 留给 CP2102N。启动日志和 LCD 应显示取得的 IP；`net status` 显示 Wi-Fi/API/MQTT 状态，不输出密码/令牌。SSID 非空但令牌不合规时网络拒绝启动并报错。`home test wifi` 在网络启用后会拒绝扫描自检，避免停止正在工作的 Wi-Fi。

STM32 先完成原 [阶段 ①～⑤](ACCEPTANCE.md)，实际演示使用 `-DSTAGE=4`；阶段 2 只有明确标记的测试数值和 LED，阶段 3 仍不驱动风扇。网络功能不改变这些限制。

## 微信开发者工具与手机

1. 安装[官方微信开发者工具](https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html)，导入仓库的 `miniprogram` 目录。无 npm 安装步骤，无 AppSecret，无云开发环境。
2. 本地代码检查可用测试模式；**真机预览需自己的小程序 AppID 和相应开发/体验权限**。把导入界面的 AppID 替换成自己的，`touristappid` 不是可发布的真实身份。不要把 AppSecret/私钥写入工程。
3. 使用基础库 3.7.0 或更高的可用版本。保持合法域名校验开启；同网段 IP 按上述官方局域网例外访问。若仅开发者工具能访问，检查手机网段、权限和路由器隔离，不能把“关闭校验后模拟器可用”记为手机验证成功。
4. 让手机与 Korvo 连接同一 Wi-Fi，输入 Korvo 屏幕显示的 IP 和配置的令牌，点连接。先看数据来源是“真实传感器 / 测试数据 / 离线模拟”，并确认 USB 在线、参数已同步。
5. 页面每秒刷新温湿度、光照、样本年龄、模式、灯/风扇回报、参数同步、MQTT 和 SD 状态。点击“开风扇”先显示“已入队”，取得对应请求 ID 的 STM32 ACK 后才报告应答。实际旋转必须现场观察。
6. 修改四个阈值后点保存，先保存 Korvo NVS，再同步 STM32；NVS 失败则不改变阈值。`saved` 表示保存成功，仍要检查 `synced`。断开/重启 STM32 后由 Korvo 重新下发，不读取 STM32 的旧参数覆盖 NVS。

手机退出、失去 Wi-Fi 或 MQTT Broker 停止，**不会停止 Korvo 与 STM32 的心跳，也不会自动清除已有手动输出**；本项目的手动 5 秒关断针对 Korvo↔STM32 失联。需停止负载时先点击关闭；要测试失联保护，拔掉 Korvo 的 USB-A 数据链路。自动模式继续根据本地有效传感器运行。

## HTTP 接口（也是小程序的接口）

端口 80，所有请求带 `Authorization: Bearer <token>`；响应 `Cache-Control: no-store`。无跨域浏览器接口和公网后台。

| 请求 | 行为 |
| --- | --- |
| `GET /api/state` | JSON 快照；坏/旧传感器数值为 `null`，未知输出为 `null` |
| `POST /api/control` | 最大 512 bytes 的严格 JSON；只接受下列完整字段集合 |

```json
{"id":101,"action":"mode","mode":"auto"}
{"id":102,"action":"fan","on":true}
{"id":103,"action":"light","on":false}
{"id":104,"action":"thresholds","fan_on_c":30,"fan_off_c":28,"light_on_lux":100,"light_off_lux":150}
```

`id` 为 1～2147483647 的整数。`on` 必须是 JSON 布尔值；无 toggle、未知字段、重复键、字符串转义、嵌套/数组、尾随垃圾或超范围数值均拒绝。温度量化到 0.01℃，照度到 0.1 lx；量化后仍需保持滞回间隔。HTTP 层完整收取 body 才提交，接收超时不执行半包。

返回 `202 {"id":102,"queued":true,"duplicate":false}` 只说明本地入队。轮询状态中的 `request_id`、`request_result`、`stm32_result`；`ack` 才是 STM32 成功应答，`rejected` / `timeout` / `save_error` 分别报告错误，`simulated` 明确不是硬件应答。并发冲突也可能在 `rejected_id` 中报告。没有执行器反馈时 `physical_feedback` 永远为 false。

HTTP 服务器在当前上电周期内记住最近 8 个已入队 ID：相同 ID+相同原始 body 不重复入队；相同 ID 不同 body 返回 409。**客户端仍不自动重试超时操作**，缓存不是跨重启事务记录。所有网络负载命令设置绝对开关状态，避免重发 toggle 反转。控制繁忙返回 409，STM32 未就绪或队列满返回 503，非法输入 400，无令牌/错误令牌 401。

## MQTT 状态约定

固定官方 managed component **`espressif/mqtt == 1.0.0`**；IDF 6.x 已将 MQTT 移出 SDK，见[官方说明](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/mqtt.html)和[版本源码](https://github.com/espressif/esp-mqtt/tree/v1.0.0)。本版只实现 LAN `mqtt://`，不加入云平台或 MQTT 控制入口。

| Topic | 载荷与行为 |
| --- | --- |
| `home/<device_id>/state` | 每秒与 HTTP 相同的 JSON；QoS 0、不 retained、断网不积压历史遥测 |
| `home/<device_id>/availability` | 连接时 retained `online`；异常断开遗嘱 retained `offline`，QoS 1，keepalive 15 秒 |

`availability` 仅指 Korvo 到 Broker 的连接；STM32 是否在线看 JSON 的 `online`，传感器是否有效看 `stale`/fault/null。订阅端还应记录收到消息的时间，3 秒以上没有新遥测就标记 MQTT 数据过期；不能只依赖可能延迟生效的遗嘱。USB 掉线仍继续上传明确的离线/过期快照，不切换模拟。

## 复现主机测试与真机验收

需要本机 GCC（Windows 可用 MinGW-w64）、Python、Node.js 22+：

```powershell
python tools/fetch_test_deps.py
python tools/test_host.py
python tools/build_bridge_fixture.py
npm ci --prefix tools/network-test
npm test --prefix tools/network-test
```

测试依赖 cJSON 1.7.19 源码校验 SHA256；MQTT 测试使用锁定的 Aedes/MQTT.js，仅用于本机随机端口。测试实际运行小程序 client.js、生产 C 解析器/状态机和本机 MQTT 数据包；HTTP 适配器、传感器值、时间和 UART 是测试夹具。**没有运行 ESP-IDF Wi-Fi、USB Host、httpd、ESP-MQTT、NVS 闪存或微信运行时。** 不用该测试宣布真机成功。

| 真机步骤 | 预期现象与应保留的依据 | 当前 |
| --- | --- | --- |
| 已完成 USB/传感器/驱动阶段后联网 | LCD 持续更新；串口有 IP；保存启动日志 | NOT RUN |
| 订阅 MQTT 30 次以上 | 每秒收到 source=sensor、有效读数；遮挡 BH1750 与实际参考比较，留 JSONL | NOT RUN |
| 微信手机开/关风扇与灯 | 页面请求 ID→ACK→状态变化；另录实际旋转/灯亮视频 | NOT RUN |
| 修改并保存阈值、分别重启两板 | 阈值恢复、synced=true；保留前后串口/状态 | NOT RUN |
| Broker 停止/重启，手机离线 | LCD/STM32 规则继续，MQTT 后续恢复；记录日志 | NOT RUN |
| 拔 USB、拔单个传感器 | 手机/MQTT 显示离线/故障/空值；无模拟替换；按模式保护 | NOT RUN |
| 至少 30 分钟连续运行 | 记录起止、复位数、断线数、丢帧/故障、触摸/手机延迟；不能只写稳定 | NOT RUN |

用 `python tools/live_check.py --ip <Korvo_IP> --seconds 1800 --output <日志路径>` 可采集实机 HTTP 状态（令牌由交互输入），要求阶段 4/真实数据；这只核对回报及数据新鲜度，风扇实际运行仍需现场确认。

## English quick start

Keep the pinned S31 SDK/BSP and the selected **Korvo USB-A → CP2102N → STM32 USART1** link. Wire SHT31/BH1750 to PB6/PB7 and active-high MOSFET inputs to PB0/PB1 using [HARDWARE](HARDWARE.md). Complete the staged hardware acceptance before adding networking.

Install the official Windows Mosquitto distribution, create `network/passwd` interactively with `mosquitto_passwd -c network/passwd home`, copy/edit the example configuration, and run `mosquitto -c network/mosquitto.conf -v`. Use the PC's LAN IPv4 address in Korvo, not localhost. In `idf.py --preview menuconfig`, open Smart Home → Network and set Wi-Fi credentials, a 16–64 character API token, a unique device ID, `mqtt://<PC-IP>:1883`, and the MQTT credentials. Build/flash using the main README. An empty SSID leaves local LCD/USB operation available without networking.

Import `miniprogram` into the official WeChat developer tool. Replace `touristappid` with your own AppID for device preview and use a supported base library >=3.7.0. No AppSecret, cloud backend or npm install is needed in the mini program. Connect phone and Korvo to the same subnet, allow LAN access, enter the LCD's IP and the matching token. Official LAN rules allow HTTP to another device on the same subnet; this does not prove any specific phone has been tested. Test on the actual phone with domain checks enabled.

The phone polls `/api/state` every second and submits absolute commands to `/api/control`; every call uses a Bearer token. HTTP 202 means queued, `ack` means STM32 response, and `physical_feedback:false` means the actual fan must be observed separately. Threshold edits save to Korvo NVS before updating the STM32 runtime copy. Wi-Fi/MQTT loss does not interrupt USB/local automation; phone departure does not cancel manual outputs. The five-second manual shutdown applies to loss of the Korvo–STM32 link.

MQTT uploads non-retained QoS0 JSON to `home/<id>/state` and uses retained online/offline Last Will on `home/<id>/availability`. It does not accept control commands. This LAN demonstration uses plaintext HTTP/MQTT with access credentials, without public port forwarding. Run the commands above for host tests. The host fixture is explicitly not physical validation; USB, ESP networking runtime, LCD/touch, NVS, sensors/fan and WeChat preview remain untested until logged on real devices.
