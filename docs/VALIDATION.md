# 验证记录 / Validation record

日期 / Date: **2026-10-26**

## 本轮实际完成 / Checks actually performed

| 项目 / Check | 结果 / Result | 依据 / Evidence |
| --- | --- | --- |
| 纯 C 主机测试 / Portable C tests | PASS | GCC 13, C11, `-Wall -Wextra -Werror -pedantic` |
| AddressSanitizer + UBSan | PASS | `ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 python tools/test_host.py` |
| SDK 环境检查 / Environment check | PASS | Python 3.12.14; target sources and RISC-V compiler found |
| 默认固件 / Default firmware | BUILD PASS | `esp32s31`, LCD/touch + simulated sensors |
| 外部外设配置 / External profile | BUILD PASS | UI off; real sensors, SD, MCP23017 and OV3660 probe enabled |
| ESP32-S31 实物烧录 / Physical flashing | **未执行 / NOT RUN** | 当前执行环境未连接用户开发板 / No physical board attached |
| 传感器/音频/屏幕/负载实测 / Peripheral behavior | **待验 / PENDING** | 需要用户板上完成下表 / Requires actual hardware |

SDK commit: `d35ea023029363d97c42565f85e0db23486f26b2` (ESP-IDF 6.2-dev). Compiler: `riscv32-esp-elf-gcc 16.1.0`, toolchain `esp-16.1.0_20260609`. CMake 4.4.4, Ninja 1.13.2. BSP 1.0.1; LVGL 9.6.0~1. Exact component resolution is checked in as `dependencies.lock`.

| 本地编译 / Local build | 应用大小 / App bytes | SHA-256 |
| --- | --- | --- |
| Default | 1,456,800 | `46b9231c84170011e672c4883c08b8905ac8953731945508e39e16c0fa4da22d` |
| External | 1,262,432 | `5d0fa1f0320c972d0cb363ff9e90bdcce35fc73786e1f5345b6ff5e7e2b063b3` |

以上哈希仅对应本地验证产物，不保证不同路径/项目版本下重建仍逐字节相同。编译产物不加入源码仓库；GitHub Actions 配置会生成可下载的构建产物。CI 配置已加入，是否通过请看仓库 Actions 的实际状态，不能把“有 workflow 文件”当成 CI 已运行通过。

Hashes identify local artifacts only; different build paths/project revisions may produce different bytes. Binaries are not committed. The Actions workflow uploads build artifacts; its presence alone is not a hosted-CI pass.

### 本地环境兼容说明 / Local environment compatibility

本轮容器限制了 `/proc` 进程查询，因此对安装在本地工具环境中的 Component Manager 增加了一个临时开关，使用其文档描述的 Linux `os.getppid()` 路径，避免 `psutil.Process()` 查询失败。工程源码、官方 BSP 和 ESP-IDF 驱动没有因此改写。该处理不在仓库中，不是用户正常 PC 安装的必要步骤。AddressSanitizer 关闭了此容器不支持的 leak 检查，地址与未定义行为检查仍执行。

The execution container restricts `/proc`. A local-only Component Manager compatibility switch used the documented Linux parent-PID path instead of process-tree introspection. It is not part of this repository or a normal PC setup step. Leak checking was disabled in this restricted container; address/undefined-behavior checks ran. SDK emits some upstream Kconfig deprecation notices; they are not application compile errors.

## 可复现命令 / Reproduction

默认构建 / Default:

```bash
idf.py --preview set-target esp32s31
idf.py --preview build
python tools/test_host.py
```

独立外设配置构建（无需覆盖默认 sdkconfig）/ Separate external-profile build:

```bash
idf.py --preview -B build-external -D SDKCONFIG=build-external/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;configs/external.defaults' build
```

此配置用于覆盖可选代码分支；没有对应外接硬件时不能期待所有功能运行成功。若改变 Kconfig 选项或配置文件，在对应构建目录运行 `idf.py --preview reconfigure`，再重新编译。

The external profile covers optional source branches. It requires the corresponding hardware for runtime success. Reconfigure the same build directory after changing configuration.

## 上板验收清单 / Hardware acceptance

以下均为空白待记录，不能提前写 PASS。All items below remain pending until observed on hardware.

| 操作 / Action | 预期 / Expected | 实测 / Observed |
| --- | --- | --- |
| UART 口烧录并复位 / Flash via UART | 启动无复位循环，有 `home>` / Stable boot and prompt | PENDING |
| 默认 LCD / Default screen | 显示 SIMULATED；触摸命令有状态反馈 / Labelled simulation and command state | PENDING |
| SET / PLAY / VOL+ / VOL− | 与 README 对应 / Matches documented actions | PENDING |
| `home light on/off` | RGB 灯跟随 / Physical RGB changes | PENDING |
| `home sim 30 55 50` in AUTO | 两路命令 ON / Both commands ON | PENDING |
| `home sim 27 55 150` | 滞回保持 / Outputs held | PENDING |
| `home sim 25 50 300` | 两路 OFF / Both OFF | PENDING |
| `home fault`, 再 `home sim` | 故障关闭、恢复采集 / Fault and recovery | PENDING |
| 修改阈值、save、断电 / Save and reboot | 阈值保留，模式回 AUTO / Thresholds restored | PENDING |
| `home test tone/mic` | 测试音/左右峰值 / Audible tone and microphone peaks | PENDING |
| memory / i2c / wifi | 日志合理，无意外复位 / Plausible logs without reset | PENDING |
| 有/无 SD 卡 / With/without card | CSV / 报错但其他功能继续 / Logs or graceful failure | PENDING |
| 相机探测 / Camera probe | 正确 sensor/format；不声称预览 / Sensor and format only | PENDING |
| 真实传感器单路掉线 / Disconnect one sensor | 仅对应自动输出关闭 / Independent fault handling | PENDING |
| 实际风扇 / Physical fan | 真实动作与命令一致，复位期间保持关闭 / Verified actual movement and reset behavior | PENDING |
| 2h → 8h 连续运行 / Soak test | 记录 heap、复位、故障与响应 / Record measured results | PENDING |
