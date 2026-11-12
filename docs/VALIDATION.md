# 验证记录 / Validation record

## 2026-11-12 教程与可靠性复核 / Beginner documentation and reliability review

本轮在 `a2cb09e` 上继续开发。该基线的 [六项 CI](https://github.com/xuan511/embedded-smart-home/actions/runs/43064970263) 已实际通过（ESP 默认/SD/模拟/完整网络配置、STM32 四阶段、主机测试），此处只作为基线记录，不替代下面修订的准确提交 checks。

新增零基础完整教程、文档导航、官方手册与实现对应表、资料获取 manifest、毕设需求/演示/实验模板；厂商资料和工程自定规则分别归属。失效/重定向的资料不伪造已下载结果，ROHM 原文通过 Adafruit 官方指向的分销商镜像核对，获取方式保留在 manifest。

修复小程序换设备暂沿用旧状态、迟到回调影响当前请求、退出页面后回调和在途 POST 的并发状态；保守计入 HTTP 往返/等待造成的样本与链路增龄，3 秒无新状态不保留可控假象。JSON 增加兼容字段 `stale_ms`、`link_age_ms`；服务器读取快照也真实更新链路年龄。错误 bool/source/读数类型不作为有效快照。

只读实机记录工具现在严格拒绝布尔/非有限/空/超范围读数、故障、错误阶段、TEST/模拟、过期样本和链路，保守计入 HTTP 耗时，保留观察到的 Korvo uptime 复位、停滞、失败类型与主动中断。它仍不能验证传感器精度、STM32 复位次数或实际风扇动作。

本地实际验证：六套 C 测试通过；8 个 Python 记录工具回归通过；新增小程序生命周期/切换/类型/增龄/应答测试通过；本机真实 MQTT 发布/订阅/遗嘱与协议/控制集成通过；JS/JSON/WXML 结构、文档本地链接及围栏检查通过。这些是主机验证，**微信官方编译器、手机、两块实物板、I2C/风扇及连续实机运行仍为 NOT RUN**。新代码已加入 CI 的主机步骤，最终构建结果以 [当前 PR checks](https://github.com/xuan511/embedded-smart-home/pull/1/checks) 的准确 SHA 为准。

本地补全官方 SDK 子模块的尝试仍遇到 GitHub 网络错误，未冒充完整 Windows SDK 安装通过；ESP 固件复验继续使用 CI 中固定 SDK、完整子模块和官方安装/构建流程。

## 2026-11-12 网络扩展 / Network extension

新增 Wi-Fi STA、官方 ESP-MQTT 1.0.0、带令牌 LAN HTTP 接口、微信小程序及共享 JSON 校验。没有升级 ESP-IDF/BSP、没有改变 USB-UART 接线。实现提交 `61275fabba536838bd9bf751f1b75e866a1fba5d` 的 [CI 实际结果](https://github.com/xuan511/embedded-smart-home/actions/runs/43062818208) 五个 job 全部 success：默认/SD/模拟 ESP 配置、STM32 四阶段、主机测试。随后平面 JSON 预校验、HTTP 错误关闭连接及依赖锁文件的收尾修订，必须以对应新提交的 PR checks 为准，不借用前一提交的成功。

已读取 job 原始日志核对：SDK `d35ea023029363d97c42565f85e0db23486f26b2`、完整递归子模块 OK、GCC 16.1.0、环境检查 PASS、cJSON 1.7.19、MQTT 1.0.0、Project build complete。新依赖锁文件取自本次外设配置 CI 产物，其 ZIP SHA256 为 `98529653731dbcc639437505af3d81278250def5896f1a3c88746714edb5f04b`。

| 本轮范围 | 代码完成 | 编译通过 | 主机测试通过 | 真机验证通过 |
| --- | --- | --- | --- | --- |
| Korvo Wi-Fi/HTTP/ESP-MQTT/LCD 扩展 | YES | CI 三配置 PASS（以上提交） | JSON 与状态交互 PASS；ESP 网络运行时未运行 | NOT RUN |
| STM32 传感器、风扇/灯及规则 | YES，沿用真实驱动 | 本地 Arm GNU 14.2.1 阶段 4 PASS，CI 四阶段 PASS | 控制/协议 PASS；电气驱动未测试 | NOT RUN |
| 微信小程序客户端 | YES | JS 语法/JSON 静态检查；未在微信工具编译预览 | 实际 client.js 经本机 HTTP/MQTT 夹具 PASS | NOT RUN |
| MQTT 数据传输 | YES | 已进入 ESP 固件构建 | 实际本机 TCP 发布/订阅、异常断线遗嘱 PASS | NOT RUN（ESP-MQTT 运行时） |
| 手机阈值保存 | YES，NVS 成功后再修改/同步 | ESP CI PASS | 量化/校验/运行时同步 PASS；NVS 不模拟为已验证 | NOT RUN |
| 连续运行、手机实机、负载实际动作 | 有验收方法和只读日志工具 | 工具可语法检查 | 不据夹具宣布实物成功 | NOT RUN |

本地已实际运行六套 C 测试，GCC 14.2.0、C11、Wall/Wextra/Werror/pedantic；CI 同时执行 ASan/UBSan。新增 `test_gateway` 检查重复键、非法字段、类型、溢出、嵌套、过长体、NUL、尾随数据、阈值量化、坏/过期值 null 和 physical_feedback=false。`tools/network-test` 使用真实 MQTT 连接、真实小程序客户端和实际生产 C 编解码/状态机；传感器输入始终为 TEST，HTTP/UART/时钟属于主机夹具。

主机未运行 USB Host 枚举、Wi-Fi 射频、ESP httpd/ESP-MQTT 任务、LCD/触摸、NVS 掉电、传感器 I2C、实际风扇和微信运行时。本机串口枚举无设备，未烧录。`live_check.py` 提供真实 HTTP 状态抓取，但当前未对实物运行；不把语法检查当成实机验收。

## 以下为此前双板实现的历史记录 / Earlier two-board history

日期 / Date: **2026-10-27**。本页对应双板版本；原提交 `bd7967a` 的 2026-10-26 编译记录是旧单板架构的历史结果，不能用作本轮双板编译或硬件证据。

This record is for the two-board revision. Historical build claims in `bd7967a` do not validate this revision or any physical hardware.

## 本次支持性复核与修订

官方能力与精确源码映射见 [SUPPORT_EVIDENCE](SUPPORT_EVIDENCE.md)，发布包及原理图字节标识见 [evidence-sources.json](evidence-sources.json)。下方原始大小/哈希专属实现提交 `7c69b1b26b6e7930b22a171229bde06f165ccb85`，不冒充本次时间修复后的新固件字节。

已重新查询该提交 [PR CI](https://github.com/xuan511/embedded-smart-home/actions/runs/40497571500)：三个 ESP 配置、主机测试、STM32 共五个 job 及其编译/测试步骤均 success。实际 job 日志确认固定 SDK、完整递归子模块、官方 install/export/idf.py 命令；host 日志显示 SANITIZE=1 和五套测试通过。

本修订修复 USB 队列丢失真实接收时间而可能把旧 STATUS 再次视为新数据的问题，新增延迟 STATUS/READY/ACK 与时钟回绕测试，五套本地主机测试通过。STM32 代码和线上格式未变；本次固件复验采用 CI 的完整官方安装流程。新提交结果按其准确 SHA 查看 [PR checks](https://github.com/xuan511/embedded-smart-home/pull/1/checks)，不能由旧提交的成功替代。

严格环境检查已加入 CI：精确 SDK 提交、未改动的 SDK 跟踪源码、完整匹配的递归子模块、禁用检查绕过、GCC 16.1.0。本机先前的下载优化仅保留为历史记录，不充当完整官方安装验证。所有真机项目仍为 NOT RUN。

## 原始实现提交的分项状态 / Implementation revision results

| 范围 / Scope | 代码完成 / Implemented | 编译通过 / Built | 主机测试通过 / Host-tested | 真机验证通过 / Hardware-tested |
| --- | --- | --- | --- | --- |
| 共享协议、滞回、有效性 / Protocol and control | YES | YES, host + STM32 | PASS | NOT RUN |
| Korvo 会话、阈值同步、过期状态 / Session, sync, stale handling | YES | PASS, ESP + host | PASS | NOT RUN |
| STM32 阶段 1 / Link-only stage | YES | PASS | Endpoint PASS | NOT RUN |
| STM32 阶段 2 / Test data + LED | YES | PASS | Endpoint PASS | NOT RUN |
| STM32 阶段 3 / Actual sensor drivers + LED | YES | PASS | Rules/CRC PASS; electrical I/O not host tested | NOT RUN |
| STM32 阶段 4 / Real sensor and GPIO drivers | YES | PASS | Endpoint/integration PASS; electrical I/O not host tested | NOT RUN |
| Korvo USB/LCD/NVS/optional SD integration | YES | PASS, 3 configurations | State/protocol PASS; USB/LCD/NVS/SD not host tested | NOT RUN |
| 连续运行 / Continuous operation | Test procedure provided | — | — | NOT RUN |

## 本轮已实际运行 / Checks actually run

`python tools/test_host.py` 在 Windows 的 GCC 14.2.0 上，以 C11、`-Wall -Wextra -Werror -pedantic` 编译并运行五套生产代码测试：

- `test_core`：启动、滞回边界、通道独立故障、过期、模式、配置、SHT CRC/换算。
- `test_link`：分包/粘包、超长、字段数量与范围、整数溢出、半帧超时、换行恢复、往返编码。
- `test_stm32_controller`：握手/CONFIG 顺序、ACK、会话与重放、手动失联、自动运行、阶段限制、新会话手动关断。
- `test_remote`：旧 ACK、配置在途更新、请求不提前改状态、数据年龄、毫秒回绕、序号耗尽、阈值量化。
- `test_integration`：生产编解码连接两端状态机，覆盖读数、操作/应答、断线、重连、自动独立运行、单向断线。

All five suites pass. These execute actual shared C code on the host; they do not emulate USB enumeration, sensor electrical behavior, LCD/touch, NVS flash or load movement. Sanitizers were **not run in this Windows check**; the old container sanitizer result is not reused.

STM32 实际交叉编译器 / Actual compiler: **Arm GNU 14.2.1**。四阶段均 CMake + Ninja、`-Wall -Wextra -Werror`，Flash 链接上限 65536 bytes、SRAM 20480 bytes：

| Stage | Flash bytes | Static data/BSS bytes | Reserved stack bytes | Result |
| --- | --- | --- | --- | --- |
| 1 | 5260 | 528 | 2048 | PASS |
| 2 | 5648 | 528 | 2048 | PASS |
| 3 | 6652 | 528 | 2048 | PASS |
| 4 | 6692 | 528 | 2048 | PASS |

栈预算来自链接预留及编译 `.su` 静态分析，不是实测运行时栈峰值；无堆分配。默认 LED 配置 PC13 低有效仅为待核默认，编译通过不证明学习板 LED 接在这里。

The stack reserve is a build budget, not a measured high-water mark. The default LED mapping remains unconfirmed for the user's learning board.

## ESP32 固定构建环境 / ESP32 build baseline

- Target: `esp32s31`.
- ESP-IDF: `d35ea023029363d97c42565f85e0db23486f26b2` (6.2-dev), unchanged from the repository baseline.
- RISC-V toolchain: `esp-16.1.0_20260609`, GCC 16.1.0.
- BSP 1.0.1, LVGL 9.6.0~1, USB 1.6.0, CP210x VCP 2.2.0, CDC ACM 2.4.1. Existing locked dependencies retained; only the two selected USB driver dependencies were added.
- Python 3.11.0; CMake 3.31.5; Ninja 1.13.2 for the final builds. Initial Ninja 1.12.1 builds hit the Windows 260-character path limit in optional configurations; no firmware/SDK source patch was used to bypass it.

本机使用工作目录中的独立 SDK/tool 环境，没有改动用户原 SDK。必要 SDK 子模块均按 gitlink 精确提交获取；本机仅跳过无关目标子模块的自动下载检查，未改官方驱动或替换硬件函数。正常用户按 README 完整初始化所有子模块即可，无需该本地下载优化。

The local SDK/tool environment is isolated. Required submodules use exact pinned commits. Only automatic fetching/checking of unrelated target submodules is skipped locally; SDK sources/drivers are not patched. Normal users should initialize all submodules as documented.

## ESP32 实际构建结果 / Actual ESP32 builds

所有配置均保留 LCD/触摸，`BSP_SD_FORMAT_ON_MOUNT_FAIL=n`。USB 配置不使用 Korvo 的外接 GPIO；模拟配置明确关闭真实链路。

| Profile | LCD/touch | USB | SD logging | Simulation | App bytes | Result |
| --- | --- | --- | --- | --- | --- | --- |
| Default | ON | ON | OFF | OFF | 1444496 | PASS |
| external.defaults | ON | ON | ON | OFF | 1547008 | PASS |
| simulation.defaults | ON | OFF | OFF | ON | 1436544 | PASS |

应用 BIN SHA-256 / Application hashes:

- Default: `8bdef836383c9b44d917e3bec93296fb0175cc1cb2b835c37aba5720dae1771f`
- SD: `9119b07d0d315d2aedc184d0b80297633add965f420dd19b1b466bbc3c544732`
- Simulation: `77b5153ce1489f75d33ea0e03dae48cd1f23cc6088bbdccce2e54e84191f9276`

本轮最终三个 Ninja 构建进程退出码均为 0，应用和 bootloader 分区大小检查通过。交付包 evidence 目录保留主机测试及最终编译日志；firmware 目录提供对应配置和固件。不同目录/构建时间可能产生不同 BIN 字节，哈希只标识本次产物，不是跨环境可重复哈希保证。

All three build processes exited 0 and passed partition-size checks. Hashes identify those artifacts; paths/build times may change rebuilt bytes. The implementation commit CI was independently rechecked: [PR run 40497571500](https://github.com/xuan511/embedded-smart-home/actions/runs/40497571500) and [push run 40497527431](https://github.com/xuan511/embedded-smart-home/actions/runs/40497527431) completed successfully. These are results for that exact commit, not proof of hardware operation or of later revisions.

复现（已激活固定 SDK 的工程根目录）/ Reproduce from the activated fixed SDK:

```powershell
idf.py --preview set-target esp32s31
idf.py --preview build
idf.py --preview -B build-sd -D SDKCONFIG=build-sd/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;configs/external.defaults" build
idf.py --preview -B build-sim -D SDKCONFIG=build-sim/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;configs/simulation.defaults" build
python tools/test_host.py
cmake -S stm32 -B stm32/build-stage1 -G Ninja -DSTAGE=1
cmake --build stm32/build-stage1
```

STM32 阶段 2/3/4 用独立目录和对应 `-DSTAGE=N`。验收仍须在实物完成，不能由上表替代。

## 真机验证与下一步 / Hardware and next action

**本轮没有执行实物烧录或联调；所有真机结果 NOT RUN。** 未验证 USB 枚举/拔插、LCD/触摸、microSD 并行使用、UART 电平/时序、学习板 LED、真实传感器、驱动/负载动作、断电默认状态、NVS 掉电保存和连续运行。

No physical flashing or hardware integration was performed. All hardware checks remain NOT RUN. Do not describe an ON report as verified physical fan rotation.

下一步只做 [ACCEPTANCE 阶段①](ACCEPTANCE.md)：核实转串口芯片、电平及 STM32 供电/SWD，烧录 link-only 固件，记录握手、断线、重连。通过后再推进测试数据/LED、传感器、负载和保存/连续运行。
