# 分阶段上板验收 / Staged hardware acceptance

**当前全部真机项目为 NOT RUN。** 此文给出运行方法和预期，空白栏必须填写实测，不能根据代码/编译/主机测试预填 PASS。先过当前门槛，再接下一阶段硬件。全部操作限低压直流演示负载。

**All hardware checks are currently NOT RUN.** Expected behavior is a test plan, not evidence. Fill observations only after running the hardware. Pass the current gate before connecting the next devices.

## 准备与记录 / Preparation and evidence

先按 [HARDWARE](HARDWARE.md)确认实际学习板排针、电源和 LED；已有 ST-Link 可用于 SWD，板上 USB 不参与通信或烧录。未确认转串口芯片时按本项目选定的 CP2102N 模块采购/核实，不把任意 USB-TTL 模块直接代入。断电改线，启动时先断开负载电源。

Confirm actual headers, board power and LED using [HARDWARE](HARDWARE.md). Use ST-Link/SWD; the STM32 USB socket is not used. Obtain/identify the specified CP2102N module rather than substituting an arbitrary USB-UART module. Rewire with power off and initially disconnect load power.

每次记录 / Record each run:

| 字段 / Field | 实测填写 / Fill in |
| --- | --- |
| 日期、操作者 / Date, operator | NOT RUN |
| 源码提交 / Git commit | NOT RUN |
| SDK、BSP、编译器 / SDK, BSP, compilers | NOT RUN |
| STM32 实际板型/芯片、阶段、LED 配置 / Actual board/chip, stage, LED | NOT RUN |
| CP2102N 模块型号、VID/PID、UART 电平 / Module, VID/PID, UART level | NOT RUN |
| 电源电压/电流能力、负载规格 / Supplies and load ratings | NOT RUN |
| 照片、串口日志、屏幕照片文件名 / Evidence filenames | NOT RUN |

Korvo 编译/烧录命令和 STM32 工具安装见两个 README。STM32 每步使用独立目录，命令中的 N 换成 1、2、3、4：

```powershell
cmake -S stm32 -B stm32/build-stageN -G Ninja -DSTAGE=N
cmake --build stm32/build-stageN
STM32_Programmer_CLI -c port=SWD -w stm32/build-stageN/home_stm32.elf -v -rst
```

以上 `N` 是占位符，不要原样输入。默认 LED 配置 PC13 低电平亮仅为待核默认；必要时为配置命令补上 `-DLED_PORT=C -DLED_PIN=13 -DLED_ACTIVE_LOW=1` 的实际值。

N is a placeholder, not a literal value. PC13 active-low is an unverified default; set the LED options for the identified board.

## ① USB/UART 最小通信 / Minimum link

**固件 / Firmware:** Korvo 默认真实 USB，STM32 `-DSTAGE=1`。

**只接 / Connect only:** Korvo 原配 LCD/触摸；Korvo USB-A → CP2102N；TXD→PA10，RXD→PA9，GND 共地；STM32 已确认电源，模块 VCC 不接；ST-Link 按需烧录。暂不接传感器/负载。

**运行 / Run:** 打开 Korvo monitor，复位两板，输入 `home status`，等待握手。拔掉 Korvo USB-A 的模块，观察至少 5 秒，再重插并查看状态。

| 检查 / Check | 预期 / Expected | 实测 / Observed |
| --- | --- | --- |
| 启动 / Boot | 无复位循环；LCD/触摸正常；串口有提示符 / Stable LCD/touch and console | NOT RUN |
| 握手/参数 / Handshake/config | stage=1、在线且已同步；有双向应答 / Stage 1, online, synchronized, bidirectional replies | NOT RUN |
| 数据 / Data | 无有效环境数据；不得显示正常 REAL 读数 / No valid environmental readings | NOT RUN |
| 控制请求 / Control | 阶段 1 拒绝输出请求，不改变物理 GPIO / Output requests unsupported | NOT RUN |
| 拔线 / Unplug | USB 检测或约 3 秒链路超时后断开；无模拟回退 / Disconnect, no simulation fallback | NOT RUN |
| 重插 / Replug | 新会话、重新下发阈值，成功后恢复；旧操作不重放 / Fresh session/config, no manual replay | NOT RUN |

**通过依据 / Gate evidence:** 保存启动/握手/断线/重连日志和屏幕照片；记录模块实际枚举身份。若失败，先查 USB 枚举再查 UART，不进入阶段 2。

## ② 明确测试数据与板载 LED / Labelled test data and onboard LED

**固件 / Firmware:** STM32 `-DSTAGE=2`；其他接线同①，先核实 LED 引脚/极性。

**运行 / Run:** 在串口依次输入，每条操作后等约 1 秒；看 `home status` 的应答与返回状态，pending 时继续等待，busy 拒绝则重试，不把 Queued 当执行成功：

```text
home status
home light on
home status
home light off
home status
home fan on
home status
```

| 检查 / Check | 预期 / Expected | 实测 / Observed |
| --- | --- | --- |
| 上传测试数据 / Upload | 明确 TEST 标记；约每秒更新 30°C、55%、50 lux / Labelled test values | NOT RUN |
| 灯请求 / Light request | 切 MANUAL，LED 随 on/off；返回相应输出状态 / MANUAL, LED changes, returned state matches | NOT RUN |
| 触摸 / Touch | 页面灯按钮走同一路径，有应答后状态更新 / Same acknowledged path | NOT RUN |
| 风扇 / Fan | 请求返回不支持；无实际风扇驱动 / Unsupported, no fan actuation | NOT RUN |
| 请求/返回 / Request vs report | 排队、ACK、超时可分辨；没有负载反馈声明 / Distinct queued/ACK/timeout | NOT RUN |
| 断线 / Disconnect | 旧测试读数标记断开/过期，不保持 OK / Old values not OK | NOT RUN |
| 单向返回故障（有条件时）/ One-way return fault | 仅 LED、已确认 3.3V 时，在 light ON 后断开 PA9→模块 RXD 跳线，保留反向线；结果未知/重握手，LED 关闭，不因反复 HELLO 保持 ON / LED closes during failed return/re-handshake | NOT RUN |

**通过依据 / Gate evidence:** 录下一次“按键/命令 → LED 实际变化 → 返回状态”，同时保存日志。若仅收到 ACK 但 LED 不亮，核查 LED 配置，不能称物理输出已验收。

## ③ 接真实 SHT3x、BH1750 / Real sensing

**固件 / Firmware:** STM32 `-DSTAGE=3`。断电增加 3.3V 兼容的 SHT3x-DIS（0x44）和 BH1750（0x23），按硬件表接 PB6/SCL、PB7/SDA 和电源地。不要接入 Korvo GPIO。

**运行 / Run:** 开机观察 1 分钟；用手靠近温湿度模块、遮挡/放开光照模块，记录趋势。分别断电断开某传感器后重上电测试，再恢复接线。热插拔不作为必要测试方法。

| 检查 / Check | 预期 / Expected | 实测 / Observed |
| --- | --- | --- |
| 来源/周期 / Source/rate | STM32 SENSORS，约 1 秒一组；不是阶段 2 常量 / Real source, about 1 Hz | NOT RUN |
| 合理性 / Plausibility | 温湿度/光照在实际环境范围，遮光照度下降 / Plausible values and light response | NOT RUN |
| SHT 失效 / SHT failure | 温湿度故障；光照仍正常 / T/H fault, light remains valid | NOT RUN |
| BH 失效 / BH failure | 光照故障；温湿度仍正常 / Light fault, T/H remain valid | NOT RUN |
| 恢复 / Recovery | 接回后恢复有效数据，不必切模拟 / Valid data recovers | NOT RUN |
| 阶段限制 / Stage limit | LED 可控制，PB0/PB1 负载未驱动，风扇请求拒绝 / LED only, no PB0/PB1 loads | NOT RUN |

记录参考温湿度计/照度计型号与样本（若有）；没有参考仪器就只报告趋势和功能，不编造精度指标。原始日志需包含有效位、样本年龄和来源。

Record reference instruments and comparison samples if available. Without one, report functional trends rather than invented accuracy. Keep source, validity and sample-age evidence.

## ④ 接灯和低压风扇 / Lamp and low-voltage fan

**固件 / Firmware:** STM32 `-DSTAGE=4`。断电接 PB0→灯驱动 IN、PB1→风扇驱动 IN、共地，以及合适的独立低压负载电源。MOSFET/驱动的 3.3V 门极能力、下拉和续流保护须按硬件文档落实。

**运行 / Run:** 上电先确认复位期间负载 OFF，再执行手动灯/风扇 on/off；之后执行 `home mode auto`。改变真实环境或暂时把阈值设在当前温度/照度附近，分别跨越开/关阈值和滞回区。勿用堵转、过热等方式测试。

| 检查 / Check | 预期 / Expected | 实测 / Observed |
| --- | --- | --- |
| 上电/复位 / Reset | 驱动输入下拉保持负载关闭；同步后按模式运行 / Loads off during reset | NOT RUN |
| 手动灯 / Manual lamp | 返回 light 状态与实际灯亮灭分别一致 / GPIO report and visible action agree | NOT RUN |
| 手动风扇 / Manual fan | 返回 fan 状态；另记目视旋转/停止 / Report plus separately observed movement | NOT RUN |
| AUTO 风扇 / Fan rule | ≥开启温度 ON，≤关闭温度 OFF，中间保持 / Both boundaries and hysteresis | NOT RUN |
| AUTO 灯 / Lamp rule | ≤开启照度 ON，≥关闭照度 OFF，中间保持 / Both boundaries and hysteresis | NOT RUN |
| 故障 / Sensor fault | 只关闭对应 AUTO 输出，另一规则继续 / Independent rule fault handling | NOT RUN |
| 显示/SD / LCD/SD | USB+控制运行中触摸/显示正常；启用日志时可读 CSV / LCD/touch and optional logging continue | NOT RUN |

当前无电流/转速反馈。软件返回 ON 只证明 MCU 报告的输出状态，不能作为风扇实际转动证据。验收照片/视频另存实际动作。

No current/tachometer feedback exists. An ON report is not proof of physical fan operation; retain separate observation/video.

## ⑤ 阈值、异常与连续运行 / Persistence, faults and running

保存/重启测试 / Save and reboot:

```text
home thresholds 29 27 80 180
home status
home save
home status
```

确认配置应答和 NVS 保存成功后，分别重启 STM32、重启 Korvo，比较阈值。再修改但不 save，保持 Korvo 供电重连，最后重启 Korvo，验证当前副本与持久副本的区别。

Verify configuration ACK and successful NVS save before rebooting each board. Then test an unsaved edit through reconnect and a Korvo reboot to distinguish current and persisted settings.

| 检查 / Check | 预期 / Expected | 实测 / Observed |
| --- | --- | --- |
| 保存与同步 / Save/sync | 保存成功值重启后恢复并同步；STM32 不反向覆盖 / Persisted values restored, no reverse overwrite | NOT RUN |
| 未保存值 / Unsaved edit | Korvo 不重启时重连继续下发；Korvo 重启恢复已保存值 / Reconnect keeps RAM edit; reboot restores saved | NOT RUN |
| MANUAL 失联 / Lost link | 手动开负载后拔 USB，STM32 最迟约 5 秒关闭；新会话重握手可提前关闭 / Both OFF by about 5 s, earlier on a fresh handshake | NOT RUN |
| AUTO 失联 / Lost link | 本地有效传感器规则继续；Korvo 显示断开 / Local rules continue, display disconnected | NOT RUN |
| 数据过期 / Stale data | 保持 CP2102N 插着，按住 STM32 RESET 至少 6 秒；旧值不再 OK，页面显示过期/断开 / Hold STM32 reset; old values stale/disconnected | NOT RUN |
| 请求超时 / Request timeout | 无对应 ACK 约 1.5 秒后显示结果未知并重同步，不乐观更新输出 / Unknown result, resynchronize | NOT RUN |
| 非法阈值 / Invalid thresholds | 输入 `home thresholds 26 28 200 100` 被拒绝，原阈值不变 / Reject inverted thresholds without mutation | NOT RUN |
| SD 故障 / SD failure | 无卡/写失败报告错误，不格式化；核心运行 / Report without formatting, continue control | NOT RUN |
| 连续运行 / Continuous run | 至少先做 30 分钟检查，再 2 小时；记录重启、断线、故障、操作延迟 / Record measured running behavior | NOT RUN |

超长帧、半帧、粘包及非法字段的可复现解析测试在 `python tools/test_host.py`，其结果属于主机验证；需要真机串口注入验证时另存原始收发日志，不混入本表实测结果。

连续运行记录必须写起止时间、实际时长、供电、采样/心跳周期、复位次数、断线次数和异常；没有完整记录就写未完成，不写“稳定运行”。如需论文更长测试，再做 8 小时并另存日志。

Record start/end time, actual duration, supply, sample/heartbeat rates, resets, disconnects and abnormalities. An unrun soak is incomplete, not stable. An optional later 8-hour run needs its own evidence.
