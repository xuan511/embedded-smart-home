# 验证记录 / Validation record

日期 / Date: **2026-10-27**。本页对应双板版本；原提交 `bd7967a` 的 2026-10-26 编译记录是旧单板架构的历史结果，不能用作本轮双板编译或硬件证据。

This record is for the two-board revision. Historical build claims in `bd7967a` do not validate this revision or any physical hardware.

## 分开记录的状态 / Separate completion states

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

All three final build processes exited 0 and passed partition-size checks. The delivery includes actual logs and artifacts. Hashes identify these artifacts; paths/build times may change rebuilt bytes. Hosted GitHub Actions has **not been observed passing**; a configured workflow is not a CI result.

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
