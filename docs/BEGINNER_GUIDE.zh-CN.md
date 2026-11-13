# 零基础上手：从两块板到一次完整演示

本教程只走一条路线：**先让 Korvo 与 STM32 通信，再接传感器，再接灯和风扇，最后加 MQTT 与微信小程序。** 每一步都能单独检查，遇到问题停在当前步，不把全部外设一次接上。

**目前先准备纯软件，不需要现在提供硬件身份信息。** 先看[行动清单](START_HERE.zh-CN.md)，然后阅读本教程第 3～4 节安装环境，执行构建与可选主机测试。第 2 节的板型/模块/供电核对留到实际接线和烧录前集中处理；离线编译不能据此确认任何实物接法。

教程核对基线：ESP32-S31-Korvo-1 V1.1、原配 LCD/触摸、ESP-IDF `d35ea023029363d97c42565f85e0db23486f26b2`、BSP 1.0.1、STM32F103C8T6。目标是 `esp32s31`，不是 `esp32s3`；屏幕子板名称中出现 S3，也不能据此改变主控编译目标。依据见[本板官方指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)、[固定 SDK](https://github.com/espressif/esp-idf/tree/d35ea023029363d97c42565f85e0db23486f26b2)。

**本教程不是已经做过的实验记录。** 当前实物 USB、传感器、风扇、LCD/触摸、NVS 掉电与微信手机预览均未验证。文中的“应看到”是验收预期，只有你上板观察并留下记录后，才能填真机 PASS。已有编译/主机测试记录见 [VALIDATION](VALIDATION.md)。

## 1. 先认识这套系统

```text
电脑 USB ────────────── Korvo UART Type-C：烧录与调试
5V 电源 ────────────── Korvo Power Type-C：供电
                             │ 原配 LCD / 触摸 / 可选 microSD
                             │
                     Korvo USB-A Host
                             │ USB 数据线或直接插入
                      CP2102 / CP2102N
                         TXD RXD GND
                             │ 3.3V 串口三线
                       STM32F103C8T6
                         │        │
                     温湿度/光照  PB0/PB1 → 两个驱动模块 → 灯/风扇

联网阶段：Korvo → 路由器 → 电脑 Mosquitto（MQTT 状态上传）
         手机微信小程序 → 同网段 HTTP → Korvo → STM32（控制请求）
```

| 名词 | 在本项目里是什么意思 |
| --- | --- |
| 固件 | 最终写入开发板 Flash、由板子自己运行的程序 |
| 编译 / build | 把 C 代码和依赖转换成固件；成功不代表接线正确 |
| 烧录 / flash / download | 把编译出的固件写入板子；电脑下载 ZIP 不叫给板子烧录 |
| 串口 / COM | Windows 分配给串口设备的端口，如 COM5；它不是引脚编号 |
| UART TX / RX | 发送 / 接收信号，连接时一边 TX 接另一边 RX |
| SWD / ST-Link | STM32 本项目使用的下载方式，不是普通 USB 串口 |
| 3V3 / 5V / GND | 电源电压及参考地；GND 相连叫共地，正电源不能因此随意并接 |
| I2C / SCL / SDA | 传感器总线及两根信号线；本项目只在 STM32 上接 |
| GPIO | MCU 的信号输入输出脚；不能当风扇或灯的电源 |
| NVS | Korvo Flash 中保存小参数的区域；本项目保存阈值 |
| ACK | STM32 对请求的应答；与“已入队”、风扇真实转动分别判断 |
| Broker / MQTT | 电脑上的消息服务器 / 发布订阅协议；本项目每秒上传状态 |
| IP / 同网段 | 局域网设备地址 / 手机与 Korvo 满足微信局域网访问条件 |

Korvo 是操作、参数和显示中枢；STM32 是采集与执行节点。自动规则只在 STM32 计算。协议、滞回、断链策略和界面布局是本项目的设计，官方资料用于核实芯片、电路和 SDK 用法，不代表厂商认证了整个毕设。

## 2. 准备物品，并先确认三件事

完整选型依据是 [HARDWARE](HARDWARE.md)。新手按以下参考清单准备，不按商品照片猜排针顺序。

| 数量 | 物品 | 本教程要求 |
| --- | --- | --- |
| 1 | Korvo V1.1 + 原配 LCD/触摸 | 不改主板，不拆 LCD，不从 SD 卡槽引线 |
| 1 | STM32F103C8T6 学习板 | 已确认供电入口；有 PA9/PA10、PB0/PB1/PB6/PB7、SWD、3V3/GND 可插接排针 |
| 1 | ST-Link | 已有；查清探针引脚定义，准备 SWD 线 |
| 1 | CP2102 或 CP2102N 单口模块 | `VID:PID=10C4:EA60`，3.3V TTL，带 TXD/RXD/GND；不是 RS232，也不是 CH340 |
| 1 | SHT31-DIS 模块 | 3.3V I2C，地址 0x44，已焊排针或自带线缆 |
| 1 | BH1750 模块 | 3.3V I2C，地址 0x23，上拉接 3.3V |
| 2 | 高有效 MOSFET 驱动模块 | 接线参考 Adafruit PID 5648 与三芯 JST-PH 转杜邦线；替代品先查实际电路 |
| 各 1 | 5V 两线灯、5V 两线风扇 | 灯应已限流；参考演示负载为灯 ≤100mA、风扇额定 ≤200mA，启动电流也要匹配 |
| 1 | 稳定独立 5V 负载电源 | 参考至少 1A，带可接线端子；给驱动/负载供电，不由 GPIO 供电 |
| 按需 | USB 数据线、杜邦线、面包板、万用表、可选 microSD | USB 线必须有数据功能；传感器/驱动无需焊接 Korvo |

首先在纸上写下三组实际信息：

1. **STM32 学习板**：型号/丝印、供电入口、排针标号；板载 LED 接哪个 GPIO、什么电平亮。`F103C8T6` 只是 MCU 型号，不能证明学习板电路。默认 LED 是 PC13 低有效，但目前还未确认你的板子是否如此。
2. **USB 转串口模块**：芯片型号、硬件 ID、UART 电平、TXD/RXD/GND 位置。模块上的“3.3V”跳帽可能只切换供电输出，并不证明 TXD 电平就是 3.3V。查卖家原理图或测量；不清楚时先不连 MCU。
3. **供电与负载**：STM32 怎样供电；ST-Link 的电压脚是参考输入还是供电输出；灯/风扇额定电压电流、驱动端子定义。不能把 5V 接到板上 3V3，也不能把不同供电输出并在一起。

STM32 的 64KB Flash、20KB SRAM、引脚功能和 SWD 依据 [ST 数据表 DS5319](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf) 与 [RM0008](https://www.st.com/resource/en/reference_manual/cd00171190.pdf)。这些手册不包含普通第三方学习板的 USB/LED/稳压器接线。探针也须按它自己的官方说明接线，不能用另一款 ST-Link 的排针图替代。

### 2.1 在电脑上检查 CP210x

先不要接 STM32 的信号线，把模块单独插到电脑上；它此时不能同时插 Korvo。

1. 右键开始菜单 → 设备管理器 → 展开“端口（COM 和 LPT）”。
2. 拔插模块，找出消失/出现的端口，记下 COM 号。
3. 右键该设备 → 属性 → 详细信息 → 属性选“硬件 Id”，查找 `VID_10C4&PID_EA60`。
4. 没有正确识别时，从 [Silicon Labs 官方 VCP 驱动页](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers)安装与你的 Windows 匹配的 CP210x 驱动，再检查。若实际是 CH340，应按另一种器件处理；本工程没有实现它的 Korvo Host 驱动。

**通过条件**：型号、VID/PID、电平和排针均有依据。电脑能出现 COM 口只证明电脑端驱动识别了模块，仍不能证明 Korvo→模块→STM32 已通信。记录硬件 ID 和电平核对结果后，从电脑拔下模块，留待第 7 节接 Korvo。

## 3. 认识 PowerShell、目录和命令

本教程的电脑命令统一在 **PowerShell** 执行，不把 Git Bash、CMD 和 PowerShell 的写法混着用。右键开始菜单 → 终端，选择 PowerShell；一般无需用管理员模式。带 `powershell` 标记的代码块输入电脑，带 `text` 标记的 `home ...` 命令输入板子串口。

```powershell
Get-Location
Get-ChildItem
```

第一条显示当前目录，第二条列出文件。仓库根目录应能看到 `README.md`、`main`、`stm32`、`tools`。`Set-Location C:\esp\embedded-smart-home` 是切换到工程，路径不是要烧进板子的命令。

**不要把板子串口命令整块粘贴。** 每次只发一条操作，等 `home status` 显示对应 ACK/result=0、`pending=0` 后再发下一条；改变阈值后还须等 `synced=1`。看到 Queued 只是入队，不能立即接着发开关或模式命令。busy/syncing 的拒绝要先等当前操作结束，再由你手动重试。

本教程统一使用短路径：

| 路径 | 用途 |
| --- | --- |
| `C:\esp\esp-idf` | 固定版本 ESP-IDF 源码 |
| `C:\esp\idf-tools` | 本教程这一份 SDK 下载的工具与 Python 环境 |
| `C:\esp\embedded-smart-home` | 毕设仓库；后面简称“工程根目录” |

逐条执行并检查结果。命令报错就先处理当前错误，不继续复制后面的烧录命令。教程里的 COM5、IP、密码和 AppID 都是需要按自己情况替换的位置。

## 4. 第一次安装开发环境

### 4.1 安装 Git、Python 与固定 ESP-IDF

安装 [Git for Windows](https://git-scm.com/download/win) 和 [Python 官方 Windows 版本](https://www.python.org/downloads/windows/)。本工程使用 Python 3.11/3.12；安装时使终端能够找到 Git 和 Python。安装完成后关闭旧终端，重新开 PowerShell：

在普通 x64 Windows 上选择对应版本的 **Windows installer (64-bit)**，不要选源码包、embeddable package 或 ARM64 包；安装 Python 时勾选 **Add python.exe to PATH**。Git 安装选允许从命令行/第三方程序使用 Git 的 PATH 选项。官网下载页默认展示的最新版可能不是本工程版本，先找到 3.11/3.12 的发行页再选安装器；不熟悉选项时保留其余默认值。

```powershell
git --version
python --version
```

如果 `python` 只打开商店或不是 3.11/3.12，先修正 Python 安装/应用执行别名，不继续安装 SDK。

乐鑫当前 [Windows 安装指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/get-started/windows-setup.html)推荐 EIM。**本工程固定的是已有验证的 preview 提交**，这里使用该提交中真实存在的官方 `install.ps1` 和 `export.ps1`，不把随手安装的最新 release 当成同一环境。EIM 已有环境也须通过本工程的精确检查；无需同时安装多套后在 PATH 中混用。

下面假定三个短路径尚未被其他项目占用。若目录已经存在，先确认里面是什么，不删除已有 SDK 或仓库。

```powershell
New-Item -ItemType Directory -Force C:\esp
Set-Location C:\esp
git -c core.longpaths=true clone https://github.com/espressif/esp-idf.git
Set-Location C:\esp\esp-idf
git -c core.longpaths=true checkout d35ea023029363d97c42565f85e0db23486f26b2
git -c core.longpaths=true submodule update --init --recursive
$env:IDF_TOOLS_PATH='C:\esp\idf-tools'
.\install.ps1 esp32s31
. .\export.ps1
```

完整下载可能较久，取决于网络。子模块下载失败，就在 SDK 目录重新执行相同的 `submodule update`；保留错误信息，不把设置“跳过子模块检查”当成修复。若仅是 PowerShell 阻止脚本，在当前终端执行 `Set-ExecutionPolicy -Scope Process Bypass` 后重试；它只影响这个终端。

`export.ps1` 前面的 **点、空格、点反斜杠** 不要漏掉。安装脚本和激活脚本可追溯至 [固定 install.ps1](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/install.ps1)、[固定 export.ps1](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/export.ps1)。

### 4.2 下载本项目

继续在 PowerShell：

```powershell
Set-Location C:\esp
git clone https://github.com/xuan511/embedded-smart-home.git
Set-Location C:\esp\embedded-smart-home
git switch codex/dual-board-usb-basics
git log -1 --oneline
python tools/check_environment.py
```

目前功能在 `codex/dual-board-usb-basics` 分支，不能只下载旧 main 后照最新教程烧录。用 `git log -1` 记录本次代码提交，以后实验数据与它对应。

**通过条件**：环境检查显示 `Baseline check PASS`，SDK 提交正确、递归子模块 OK、RISC-V GCC 为 16.1.0。这个检查只核对环境，不测试电路。出现 FAILED 时按提示修复 SDK/工具来源，不能把源码改成跳过检查。

### 4.3 安装 STM32 的一套工具

本项目只维护 **Arm GNU Toolchain + CMake + Ninja**，烧录使用 **STM32CubeProgrammer + ST-Link**；不需要再安装 Keil、Arduino 或 STM32 FreeRTOS 工程。

1. 从 [Arm 官方工具链入口](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain)进入[官方 GitLab 发布仓库](https://gitlab.arm.com/tooling/gnu-toolchains-for-arm)，选择已验证的 **14.2.Rel1（分支 `releases/14.2.rel1`）、Windows 主机、`arm-none-eabi` 目标**。不要选 AArch64 Linux 或 `arm-linux-*`。Arm 迁移了发布位置，不代表本工程需要升级工具链；`arm-none-eabi-gcc --version` 中该版应显示 14.2.1。安装包下载/校验需在本机按发布页完成，本次没有把反机器人页面当作安装包验证；已有实际编译结果见 [VALIDATION](VALIDATION.md)。
2. 安装时添加其 `bin` 到 PATH；若安装器未做，在 Windows“编辑账户的环境变量”中找到用户 Path → 新建 → 填实际安装目录的 `bin`，不要覆盖原 Path。重新开终端后测试。
3. CMake/Ninja 优先复用 ESP-IDF 环境；若激活后仍找不到，从 [CMake 官方下载](https://cmake.org/download/)与 [Ninja 官方发行页](https://github.com/ninja-build/ninja/releases)安装。CMake 需 ≥3.22，Ninja 在 PATH 中可用；建议短目录，长路径问题先缩短路径。
4. 从 [ST 官方 STM32CubeProgrammer 页](https://www.st.com/en/development-tools/stm32cubeprog.html)下载安装 Windows 版本及该安装包所需的 ST-Link 驱动。烧录界面就是 STM32CubeProgrammer，不是 CubeMX。

检查：

```powershell
arm-none-eabi-gcc --version
cmake --version
ninja --version
```

**通过条件**：三条均输出版本。`arm-none-eabi-gcc` 编译的是 STM32 固件；它不能用来编译电脑本机运行的测试。电脑测试另见第 13 节，暂时不需要它也可以推进上板。

### 4.4 以后每次重新打开终端

环境激活只对当前终端有效。重新开 PowerShell 后先运行：

```powershell
$env:IDF_TOOLS_PATH='C:\esp\idf-tools'
Set-Location C:\esp\esp-idf
. .\export.ps1
Set-Location C:\esp\embedded-smart-home
```

不需要每次重新 clone 或重新安装。若打开多个终端，每个都分别激活；不能认为终端 A 激活后终端 B 也自动生效。

## 5. 先单独编译、烧录 Korvo

### 5.1 当前只连接 Korvo

保留原配 LCD/触摸。Korvo UART Type-C 用数据线接电脑；Power Type-C 用合适 5V 电源供电，打开板上电源开关。暂时不插 CP210x，不接外部负载，不运行音频自检。两 Type-C 与 USB-A 的用途以[官方接口表及供电说明](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html#description-of-components)为准；风扇/灯不从 Host 口取电。

在设备管理器“端口（COM 和 LPT）”通过拔插 **Korvo UART 数据线**识别其 COM 号。它与刚才插电脑检查的独立 CP210x 模块是两个设备，不能混用端口号。

### 5.2 编译

在激活了固定 SDK 的工程根目录：

```powershell
python tools/check_environment.py
idf.py --preview set-target esp32s31
idf.py --preview build
```

等待最后的 `Project build complete`，确认不是中途某行“成功”之后又出现错误。首次构建会从 Component Registry 获取锁定 BSP、LVGL、USB Host、MQTT 和 cJSON，后续不必手动把库复制进 `main`。

`set-target` 只在首次设置或需要纠正目标时运行；它会重新生成相关配置。以后正常改代码/参数时直接 `build`、`flash`，不要每次用 `set-target` 重置自己的配置。

第一次先用默认配置：LCD 开，显式离线模拟关，Wi-Fi SSID 空，SD 日志关。若以前编译过其他配置，运行 `idf.py --preview menuconfig`，到 Smart Home 检查 `Explicit OFFLINE simulation` 关闭；不要靠修改 `sdkconfig.defaults` 期待已有 `sdkconfig` 自动重置。

### 5.3 烧录并打开串口

把下例 COM5 改成你的 Korvo 端口：

```powershell
idf.py --preview -p COM5 flash monitor
```

如果卡在连接，先关闭其他占用 COM 的串口窗口。再按官方方法：按住 BOOT → 按下再松开 RESET → 松开 BOOT，重试下载。`Ctrl+]` 退出 monitor；需要重新进入时运行 `idf.py --preview -p COM5 monitor`。通用下载流程见[固定 SDK 的 get-started 文档源码](https://github.com/espressif/esp-idf/tree/d35ea023029363d97c42565f85e0db23486f26b2/docs/en/get-started)。

看到 `home>` 提示符后，**在 monitor 内**输入：

```text
home help
home status
```

**应看到**：LCD 有主页，串口能返回状态；没有 STM32 时 USB 是断开/等待、数据不可用，不会悄悄出现正常室温。LCD 上 `WAITING / NO DATA`、`DISCONNECTED` 是此时正常的等待状态。看到 `OFFLINE SIMULATION` 表示烧录的是模拟配置，先纠正配置再继续。

**先查什么**：没有 COM→数据线/驱动/插错 Power 口；烧录失败→端口占用和 BOOT/RESET；LCD 不亮→供电、原配屏及排线安装；找不到 target→激活错 SDK。不要为修 LCD 改成 S3 或拔用 SD 引脚。

**留下依据**：代码提交、完整编译结果、烧录成功日志、串口 `home status` 和 LCD 实物照片。这里只能给 Korvo 单板状态填结果，不能提前给 USB 双板填 PASS。

## 6. 单独烧录 STM32 阶段 1

先不接传感器和驱动。按照已核实的学习板说明接独立合适电源，BOOT0 保持 0。断电接 ST-Link 的信号：

| 信号 | STM32 MCU 信号 |
| --- | --- |
| SWDIO | PA13 |
| SWCLK | PA14 |
| GND | GND |
| NRST（建议） | NRST |
| 真品探针的目标电压参考 VTref | 目标 3V3，用于参考，不假定是电源 |

廉价探针标“3.3V”的脚可能是电源输出，不能照搬 VTref 接法；查探针说明。目标已有独立电源时不再并接另一电源输出。具体学习板 USB 供电电路尚未确认，不指定它的插座用途。接口依据 [ST MCU 手册](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)与探针原厂手册；STM32CubeProgrammer 的已核对官方命令参考为 [UM2237 在线版 2.23.0](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_Command_Lines.html)。GUI 菜单以安装版本附带的官方说明为准。

回到工程根目录编译：

```powershell
cmake -S stm32 -B stm32/build-stage1 -G Ninja -DSTAGE=1
cmake --build stm32/build-stage1
```

产物位置 `stm32\build-stage1\home_stm32.elf`。`.elf` 带程序与符号，可直接给 CubeProgrammer；`.bin/.hex` 是其他烧录形式，`.map` 用于查看内存分配。链接脚本固定 64KB Flash/20KB SRAM，不能靠改大容量解决不明芯片问题。

使用图形界面烧录，无需先找 CLI 的 PATH：

1. 打开 STM32CubeProgrammer，右侧连接方式选 **ST-LINK**，接口选 **SWD**。
2. 给已正确连接的目标板供电，点 **Connect**，确认工具能识别目标。
3. 打开“Erasing & Programming”或对应下载页面，浏览选择刚才的 `home_stm32.elf`。
4. 执行 Download，并启用/执行下载校验 Verify；记录成功结果。ELF 含地址，不随意填写其他存储地址。
5. 执行复位/运行，或断电后重新上电。若界面版本布局不同，按安装版本附带的官方 ST-LINK/SWD 下载流程操作，不把文字按钮位置当成所有版本固定布局。

若你已把 CLI 加入 PATH，同一操作也可用：

```powershell
STM32_Programmer_CLI -c port=SWD -w stm32/build-stage1/home_stm32.elf -v -rst
```

**通过条件**：阶段 1 编译、目标连接、下载、校验分别成功。此时没有传感器读数，也不应靠“板上某个灯亮了”判断协议成功。连接失败先查板上真实供电、探针地、SWDIO/SWCLK 和 BOOT0；必要时接 NRST，按官方说明尝试 Under Reset，不先更换工程或解锁读保护。

**留下依据**：STM32 编译器版本、ST-Link/目标识别、文件路径与 Download/Verify 结果。软件没有读写保护测试需求，不对未知芯片做批量擦除/解锁实验。

## 7. 阶段 1：只验证两板通信

断电，先从电脑拔下独立 CP210x，再按这张表连接。排针名称是 MCU 信号，不是学习板排针“第几个孔”。

| CP210x | STM32 | 为什么 |
| --- | --- | --- |
| TXD | PA10 / USART1_RX | 模块发 → STM32 收 |
| RXD | PA9 / USART1_TX | STM32 发 → 模块收 |
| GND | GND | 信号参考共地 |
| VCC / 5V / 3V3 / DTR / RTS | **不接** | 本方案只用 UART 三线，不从模块给 STM32 供电 |

将模块直接插入 Korvo 的 **USB-A Host** 或使用正常 USB 数据线连接，不加 Hub。STM32 按自己的已核实入口供电，Korvo 保持第 5 节固件和供电方式。**这就是本项目的 USB 双板链路，STM32 学习板的 USB 插座不参与数据通信。** Host 用法依据[固定 SDK CDC Host 官方示例](https://github.com/espressif/esp-idf/tree/d35ea023029363d97c42565f85e0db23486f26b2/examples/peripherals/usb/host/cdc)和[选定 CP210x 驱动](https://components.espressif.com/components/espressif/usb_host_cp210x_vcp/versions/2.2.0/readme)，本板接口依据见 [SUPPORT_EVIDENCE](SUPPORT_EVIDENCE.md)。

上电，在 Korvo monitor 里运行：

```text
home status
```

**应看到两层结果**：

- USB 层：日志 `CP210x 10C4:EA60 opened, 115200 8N1; awaiting STM32` 表示模块打开了，尚不等于 STM32 已握手。
- 应用层：稍后 `stage=1`、`online=1`、`synced=1`，表示 HELLO/READY、配置和状态链路已建立。阶段 1 无采样，所以环境读数无效/过期是正常现象。

两端协议配置为 115200/8N1。STM32 当前使用 HSI 8MHz，整数分频的计算波特率约 115942（约 +0.64%，另有 HSI 本身的误差和温漂）；不能仅凭编译通过就保证整条实物 UART 链路在所有条件下可靠，连续运行时要记录通信错误与重连。

然后拔下 Korvo 的 CP210x USB，观察断开，再重新插回并等参数重新同步。**通过条件**是能连接、双向握手/应答、断开与重连，不是只有一个“USB 打开”日志。

**先查什么**：没有打开日志→是否选定芯片与 VID/PID、线缆、USB-A/供电；已打开但不 online→STM32 供电/固件是否运行、TX/RX 是否交叉、共地、电平；重复断开→先看供电与接触，再保存完整日志排查。

**留下依据**：USB 身份、一次正常状态、拔出时状态、重插后状态，写明实际等待时间。保留 LCD/触摸连接；这些现象全部未上板时不要在表里预填 PASS。

## 8. 阶段 2：测试数据与板载 LED

这一步仍不接真实传感器和负载。先确认 LED 引脚/极性。只有实际确认是 **PC13 低电平亮**时，才直接用默认值；未知时先查学习板资料，不把这当成所有 C8T6 板的事实。

```powershell
cmake -S stm32 -B stm32/build-stage2 -G Ninja -DSTAGE=2 -DLED_PORT=C -DLED_PIN=13 -DLED_ACTIVE_LOW=1
cmake --build stm32/build-stage2
```

如果你的 LED 经过确认是其他允许引脚，修改三个 LED 参数。固件会拒绝与固定 UART、I2C、SWD/JTAG 或负载引脚冲突的配置。按第 6 节方法把 **build-stage2** 的 ELF 烧到 STM32，别误烧回 build-stage1。

**应看到**：LCD 明确标记 `STM32 TEST DATA`，数据固定为 **30℃、55%RH、50lx**，大约每秒上报。这些是源码规定的测试值，不是房间测量值。系统初始 AUTO，默认 28/26℃ 与 100/200lx，因此测试值可能使 LED 自动点亮；它不是接线结论。

在 Korvo 串口依次执行，每条等返回后再执行下一条：

```text
home light on
home status
home light off
home status
```

手动 light 命令会切换到 MANUAL。分别确认请求提示、`STM32 ACK ... result=0`、light 回报，以及板载 LED 的实际亮灭。阶段 2 没有风扇，`home fan on` 应被 STM32 拒绝，不应接风扇“试一试”。

**通过条件**：测试来源标签正确、数据到 LCD、两次 LED 实际动作与状态返回一致。LED 不动作先核对引脚和有效电平；若回报没有 ACK，先回第 7 节处理通信，不先改 GPIO 驱动。

**留下依据**：板型与 LED 来源、测试标签、ON/OFF 请求及应答、LED 实物变化。此时只能证明测试数据与 LED 闭环，不能证明传感器或风扇。

## 9. 阶段 3：只接真实环境传感器

先断电，暂时不接灯、风扇驱动。两传感器并联在同一组 I2C 线上：

| STM32 | SHT31-DIS | BH1750 |
| --- | --- | --- |
| 3V3 | VCC | VCC |
| GND | GND | GND |
| PB6 | SCL | SCL |
| PB7 | SDA | SDA |
| 地址设置 | 0x44，按模块电路确认 ADDR 默认状态 | 0x23，按模块电路确认 ADDR 默认状态 |

SCL/SDA 必须通过合适上拉电阻接 **3.3V**，不是 5V；先查模块是否已有上拉，不盲目叠加多个低阻上拉。PB6/PB7 在本工程是软件开漏 I2C，不能据引脚名称假定启用了 STM32 硬件 I2C 外设。SHT31 应为 DIS 数字 I2C 型，不用模拟输出型替代。依据：[Sensirion SHT3x-DIS 数据表](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf)、[Espressif 固定 BH1750 驱动](https://github.com/espressif/esp-iot-solution/tree/7a813417274c663226601f390668b9c8e5eec768/components/sensors/light_sensor/bh1750)。后者用于核对命令和换算，实际模块电源/上拉仍由模块电路决定。

编译阶段 3，LED 参数仍按已确认实物填写：

```powershell
cmake -S stm32 -B stm32/build-stage3 -G Ninja -DSTAGE=3 -DLED_PORT=C -DLED_PIN=13 -DLED_ACTIVE_LOW=1
cmake --build stm32/build-stage3
```

烧录对应 ELF，上电检查 `stage=3` 和 LCD `STM32 SENSORS` 标签。温湿度和光照应随实际环境变化，不能仍稳定等于阶段 2 的固定三元组就直接判断通过。

操作顺序：先记录一分钟自然读数；用遮挡改变 BH1750 的入射光，观察照度趋势；用旁边的独立温湿度计比较温湿度并记录环境/偏差。这里只能作工程合理性比较，不能把未校准参考当成精度认证。不要短接或用火/高温直接加热模块。

故障检查分开做：**断电**后断开 SHT31，再上电，应显示温湿度故障，BH1750 仍能更新；恢复后对 BH1750 做同样操作，应只影响光照。最后恢复全部接线。阶段 3 风扇仍不启用。

**先查什么**：只有某传感器故障→该模块电源、地址、上拉、接触；两者都坏→共用 3V3/GND/SCL/SDA、SCL/SDA 是否对调；标签 TEST→烧错阶段。不要为让页面“正常”把驱动改成返回固定数值。

**留下依据**：完整接线、地址/电平、真实来源标签、自然/遮光数据、参考仪器、单通道断开与恢复记录。传感器电气和读数在实物未测前保持 NOT RUN。

## 10. 阶段 4：接真实灯和风扇

这一步使用两个独立的高有效驱动。下表专门对应参考模块 **Adafruit PID 5648**；其他模块端子顺序、供电和有效电平必须按其厂家原理图重新核对，不能照线色直接套用。

| 接点 | 灯驱动 | 风扇驱动 |
| --- | --- | --- |
| In / 参考线缆白线 | STM32 PB0 | STM32 PB1 |
| GND / 黑线 | STM32 GND + 负载电源负极 | 同一公共 GND |
| V+ / 红线 | 独立负载电源 +5V | 同一独立 +5V |
| 输出 + | 灯正极 | 风扇正极 |
| 输出 − / Out | 灯负极 | 风扇负极 |

Out 是开关负端，不是固定接地。信号 GPIO 只接 In；驱动的负载电源不取 STM32 3V3，也不从 Korvo USB-A 带风扇。参考模块支持 3.3V 控制并提供保护，依据 [Adafruit 官方引脚说明](https://learn.adafruit.com/adafruit-mosfet-driver/pinouts)与 [厂家电路](https://github.com/adafruit/Adafruit-MOSFET-Driver-STEMMA-PCB)。共地与供电详图继续以 [HARDWARE](HARDWARE.md)为准。

编译并烧录：

```powershell
cmake -S stm32 -B stm32/build-stage4 -G Ninja -DSTAGE=4
cmake --build stm32/build-stage4
```

**第一次给负载电源上电前**，先使 MCU/USB/传感器正常运行，确认 `stage=4` 与参数同步，在 Korvo 串口依次执行并确认返回：

```text
home mode manual
home light off
home fan off
home status
```

确认回报 OFF 后，再给独立 5V 负载电源上电。这样不会因初始 AUTO 和当前环境达到阈值而让负载意外启动。之后逐个 `home light on/off`、`home fan on/off`，同样可用 LCD 上 Light toggle、Fan toggle 操作。

上面的四行也要逐行执行：`home mode manual` 等 ACK 后，才发 `home light off`；再等 ACK 后发 `home fan off`。最后检查 `home status` 的 MANUAL、两路 OFF、`pending=0`。某条拒绝或超时就不继续给负载上电，先查明并重新确认状态。

**每一次都要分开确认**：请求进入 → STM32 ACK → 输出状态回报 → 实际灯亮/风扇转动。没有转速/电流反馈，页面 ON 不是物理运行证据。

再做自动联动。先用 `home status` 看室温与照度，再选择能在当前环境演示的合理阈值，例如：

```text
home thresholds 29 27 80 180
home status
```

先反复 `home status`，等 `synced=1`、`pending=0`，再单独发送 `home mode auto`，等该模式请求成功 ACK 后再观察。改阈值会发起一次 CONFIG 同步，不能立即整块粘贴 AUTO 命令。

这四个参数依次为风扇开温度、风扇关温度、开灯照度、关灯照度。29/27℃ 表示 ≥29 开、≤27 关，27～29℃ 保持原状态；80/180lx 表示 ≤80 开灯、≥180 关灯，中间保持。**示例值不会保证你的室温恰好跨过阈值。** 若自然环境不满足，按实测读数调整阈值并记录，不伪造传感器值。遮光时避免让被控灯直接照射 BH1750，否则会形成自己的光学反馈，影响演示解释。

手动开关会把整个系统切回 MANUAL；想继续联动需再发 `home mode auto`。上电/复位的运行模式以代码初始化为准，当前是 AUTO；不要认为先前手动关机状态一定跨复位保留。

**先查什么**：ACK 是拒绝→确认 stage 4、同步；ACK 成功但风扇不动→分别检查驱动 In、共地、独立电源、输出端子与风扇，不能用代码直接拉 GPIO 供电来“解决”。

**留下依据**：负载/电源/驱动型号、接线、实测电压、ON/OFF 应答和实际动作视频；自动规则每个边界与滞回区的读数/状态。驱动模块或负载被更换，要重新记录相应接线依据。

## 11. 保存、断线与故障验证

串口改阈值后先在 RAM 生效并同步，`home save` 才写 NVS；手机保存按钮会先保存，再更新/同步。按以下顺序做：

```text
home thresholds 29 27 80 180
home status
home save
home status
```

等待 `synced=1`，保存提示须成功。分别重启 STM32、重启 Korvo，确认期望阈值恢复并重新同步。Korvo 保存权威副本，STM32 只持运行副本；STM32 旧值不会反向覆盖 NVS。NVS API 依据[乐鑫官方 NVS 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/storage/nvs_flash.html)，本项目的同步顺序是自行设计，见 [ARCHITECTURE](ARCHITECTURE.md)。

| 实验 | 操作 | 应看到 |
| --- | --- | --- |
| 手动失联 | 手动开负载，拔 Korvo 的 CP210x USB | STM32 约 5 秒内关演示负载；新会话握手可更早关；Korvo 显示断开 |
| 自动失联 | AUTO 条件有效时拔同一 USB | STM32 本地有效传感器规则继续；Korvo 不把旧读数显示正常 |
| 数据过期 | 保持模块插 Korvo，按住 STM32 RESET ≥6秒 | Korvo 断线/过期，旧读数不显示 OK；没有模拟回退 |
| 传感器故障 | 按第 9 节断电后分别断开模块再启动 | 对应通道故障；AUTO 下对应输出关闭，另一规则独立工作 |
| 未保存参数 | 改阈值但不 save，保持 Korvo 运行重连 STM32，再重启 Korvo | 重连仍用 Korvo RAM 新值；Korvo 重启恢复已保存值 |

上述秒数是本项目协议/控制策略，不是 MCU 厂商的器件保证。每个实测写开始条件、实际时间、观察结果和日志。结束先 `home light off`、`home fan off`，再关闭独立负载电源。

### 可选 microSD 日志

核心闭环通过后再加入。断电插入已准备好的 FAT 卡，保留原卡槽功能；在 Smart Home 中启用 SD CSV 日志，重新 build/flash。程序约每 10 秒向卡中 `home.csv` 追加一行，`SD READY` 是挂载/写入状态；失败不自动格式化。安全取卡时先停止运行并断电，再用读卡器检查文件。表中同时包含有效位/来源，分析时不能只取数值列忽略故障标记。

## 12. 最后加入 MQTT 与微信小程序

### 12.1 准备同一个局域网

电脑、Korvo、手机连接同一普通路由器，先使用易联调的 2.4GHz Wi-Fi。手机与 Korvo 必须同网段且不是同一设备 IP；来宾网络或 AP 隔离可能禁止设备互访。本项目没有蜂窝网络/公网远程后台，手机切到移动数据后不能按局域网方法连接。

在电脑 PowerShell 运行 `ipconfig`，查实际联网网卡的 IPv4，例如 `192.168.1.10`。它是 MQTT Broker 地址。以后 Korvo LCD 显示的例如 `192.168.1.50` 是**另一台设备的地址**，手机填后者，不能把两者混用。微信同网段 HTTP 规则见[官方局域网说明](https://developers.weixin.qq.com/miniprogram/dev/framework/ability/mDNS.html)；本项目手填 IP，不依赖 mDNS 自动发现。

### 12.2 在电脑安装并启动 Mosquitto

安装 [Eclipse Mosquitto 官方 Windows 版本](https://mosquitto.org/download/)。下面假定程序安装在 `C:\Program Files\mosquitto`；如果安装位置不同，替换命令中的程序路径。

在工程根目录：

```powershell
Copy-Item network/mosquitto.conf.example network/mosquitto.conf
notepad network/mosquitto.conf
```

检查 `password_file` 一行指向本机真实位置，本教程为 `C:/esp/embedded-smart-home/network/passwd`，保存并关闭记事本。接着：

```powershell
& 'C:\Program Files\mosquitto\mosquitto_passwd.exe' -c network/passwd home
```

按提示输入并确认一个自己保管的 MQTT 密码。`home` 是本项目示例用户名；它不是串口 `home>` 提示符。创建密码文件后，启动前台 Broker：

```powershell
& 'C:\Program Files\mosquitto\mosquitto.exe' -c network/mosquitto.conf -v
```

保留该窗口运行。防火墙提示仅允许需要的专用局域网；不要关闭整个防火墙或开公网端口映射。如果提示 1883 已占用，检查是否安装时已启动 Mosquitto 服务，不再开第二个同端口实例。配置/认证依据：[Mosquitto 官方认证](https://mosquitto.org/documentation/authentication-methods/)、[配置手册](https://mosquitto.org/man/mosquitto-conf-5.html)。

### 12.3 配置 Korvo 联网

另开并激活 SDK 的 PowerShell，到工程根目录：

```powershell
idf.py --preview menuconfig
```

方向键移动、Enter 进入/编辑、空格切换 bool；按界面的 Save/Exit 提示保存退出。在 Smart Home → Network 填：

| 项目 | 怎么填 |
| --- | --- |
| Compile Wi-Fi, LAN phone API and MQTT upload | 开启 |
| Wi-Fi SSID / password | 自己路由器的名称和密码，大小写一致 |
| LAN API token | 自己生成的 16～64 位字母/数字/`_`/`-`；留给手机登录本地 API |
| Unique MQTT device ID | 单台可用 `korvo01`；多台不得同名 |
| LAN broker URI | `mqtt://电脑实际IP:1883`，例如 `mqtt://192.168.1.10:1883` |
| MQTT username / password | 刚才创建的 `home` 和 MQTT 密码 |

**这三种凭据不同**：Wi-Fi 密码用于入网；MQTT 用户/密码用于 Broker；API token 用于手机访问 Korvo。不要把 `configs/network.defaults` 的 `BUILD_CHECK_ONLY`、保留文档 IP 或 CI token 当成自己的配置。本版是本人可信局域网内的 HTTP/MQTT TCP，令牌提供访问控制，传输没有加密；不配置公网映射。

保存后编译烧录：

```powershell
idf.py --preview build
idf.py --preview -p COM5 flash monitor
```

COM5 仍是 Korvo UART 端口。串口执行 `net status`，应看到 WiFi 已连接、非 `0.0.0.0` 的 IP、MQTT 已连接；屏幕应同步显示网络状态。SSID 空时程序不启动联网；SSID 非空但 token 不合规则报错并保持本地 LCD/USB 工作。官方 MQTT 组件和 IDF 6.x 获取方式见[乐鑫说明](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/mqtt.html)、[本工程固定 MQTT 1.0.0](https://github.com/espressif/esp-mqtt/tree/2c3e1e0942afcc5b3cea2710ffe220fe699da02a)。

### 12.4 先确认 MQTT 收到真实状态

再开一个 PowerShell，密码替换为刚创建的 MQTT 密码：

```powershell
& 'C:\Program Files\mosquitto\mosquitto_sub.exe' -h 127.0.0.1 -p 1883 -u home -P 'YOUR_MQTT_PASSWORD' -t 'home/korvo01/#' -v
```

这里订阅程序和 Broker 在同一电脑，所以用 `127.0.0.1`；Korvo 不能把这个地址当电脑。命令行密码会留在终端历史中，不把含凭据的截图/日志提交到 GitHub。

**应看到**：`home/korvo01/state` 大约每秒一条 JSON；来源 `source=sensor`、`stage=4`、`online=true`、`synced=true`，正常读数不是 null。`availability=online` 仅指 Korvo 与 Broker 的连接，不证明 STM32 或传感器正常。停止 Broker 后 LCD/USB/本地 AUTO 仍应工作；重启后再检查恢复。完整 Topic、故障/遗嘱语义见 [NETWORK](NETWORK.md)。

**先查什么**：Wi-Fi 未连→SSID/密码/路由器；有 IP、MQTT 未连→电脑 IP、Broker 进程、端口、认证、防火墙；MQTT 有消息但 source=test→STM32 烧错阶段；消息 stale/null→回到 USB/传感器检查。

### 12.5 微信导入与手机预览

1. 安装[微信官方开发者工具](https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html)。
2. 选择导入项目，项目目录选 **`C:\esp\embedded-smart-home\miniprogram`**，不是整个仓库，也不是 `pages` 子目录。无需在此目录 npm install，无 AppSecret、无云开发环境。
3. 真机预览使用自己的小程序 AppID，并具备该小程序开发/体验权限。仓库 `touristappid` 是占位，不能拿它作为真实发布身份。不要把 AppSecret/私钥放到前端工程。
4. 选择基础库 3.7.0 或更高的可用版本，保持合法域名校验开启。先看代码/页面是否编译成功，再用工具的预览流程生成二维码，让具备权限的手机微信扫码。
5. 手机连同一 Wi-Fi、允许微信局域网访问，输入 **Korvo LCD 显示的 IP**和 API token，点“连接 / 刷新连接”。电脑开发者工具可用不等于手机可用；按[微信官方网络规则](https://developers.weixin.qq.com/miniprogram/dev/framework/ability/network.html)核对真机，不能用“关闭检查后模拟器能连”代替实测。
6. 页面来源应为“真实传感器”，温湿度/光照约每秒更新；查看 USB、样本年龄、模式、参数同步、MQTT、SD。先点单个开关，等请求 ID 对应结果，再进行下一次操作。屏幕/手机同时操作时可能遇到 busy/rejected，稍后重新观察并手动操作，不自动重放超时请求。
7. 修改四个阈值后保存，先看保存结果，再看参数已同步；分别重启两板复核。`saved` 与 `synced` 是不同步骤，不能只看到已保存就认定 STM32 已同步。

手机通过 HTTP 请求 Korvo，MQTT 由 Korvo 上传。手机关闭、失去 Wi-Fi或 Broker 停止**不会自动清除已有手动输出**，因为 Korvo 与 STM32 的 USB 心跳仍在。手动 5 秒保护针对两板失联，不能把退出小程序当急停。

**通过条件与证据**：手机真机预览截图/录像、数据来源、请求 ID→STM32 应答→实际负载动作、MQTT 原始消息、网络中断与恢复。当前这些实物项目仍未运行。

## 13. 留下可用于毕设的验证记录

### 13.1 真机连续记录

真实传感器与负载阶段、网络均已通过后，在电脑新终端执行：

```powershell
Set-Location C:\esp\embedded-smart-home
python tools/live_check.py --ip 192.168.1.50 --seconds 1800 --output evidence/local/run-01.jsonl
```

IP 改为 Korvo 实际 IP。按提示输入 API token，它不会写入日志；输出文件须是一个新名字，避免覆盖原始证据。需要同时要求 MQTT 在线时，可按工具帮助增加 `--require-mqtt`。该工具只读 HTTP 状态、不操作负载；记录无异常也不能证明风扇实际在转。意外中断/故障记录保留，不删掉“难看”样本后称稳定。

先做 30 分钟，再按 [ACCEPTANCE](ACCEPTANCE.md) 做 2 小时或学校要求的连续运行。记下实际起止、代码提交、硬件/供电、复位、断线、故障与操作次数。更多实验表和演示材料见 [PROJECT_GUIDE](PROJECT_GUIDE.zh-CN.md)。

### 13.2 没有实物时可以先做的主机测试

主机测试需要电脑本机 C 编译器（如 MinGW-w64 GCC），而不是 `arm-none-eabi-gcc` 或 `riscv32-esp-elf-gcc`。Node.js 22+ 仅用于主机 MQTT/小程序客户端测试；微信前端本身不用 npm 装库。

```powershell
python tools/fetch_test_deps.py
python tools/test_host.py
python -m unittest discover -s tests -p test_live_check.py -v
node tests/test_miniprogram.cjs
python tools/build_bridge_fixture.py
npm ci --prefix tools/network-test
npm test --prefix tools/network-test
```

这些命令运行实际的 C 协议/控制代码、小程序客户端和本机 MQTT 数据包，测试传感器输入、UART、HTTP 适配层与时间来自明确夹具，**没有运行真实 USB、ESP 网络任务、NVS Flash、传感器 I2C 或风扇**。不会为了让主机测试通过而伪装真实驱动。

代码已经有 GitHub CI，可先阅读准确提交的 checks 了解已验证范围；自己运行时保存实际版本和日志。命令的 PASS 填“主机测试通过”，不能填“真机验证通过”。

## 14. 遇到问题先按这一条顺序回退

| 现象 | 先查 | 暂时不要做 |
| --- | --- | --- |
| 命令找不到 | 当前 PowerShell 是否激活 SDK、工具 PATH、重开终端 | 乱改源码/同时装多个工具链 |
| 环境检查拒绝 | 固定 SHA、完整子模块、SDK 是否被改、GCC 来源 | 跳过检查、换 S3 目标 |
| Korvo 无 COM | UART 口、USB 数据线、设备管理器、对应驱动 | 把 Power 口当串口 |
| STM32 无法连接 | 实际供电、SWD/GND、BOOT0、探针/NRST | 不明原因解锁/整片擦除 |
| USB 打开但不握手 | STM32 阶段/运行、TX↔RX、3.3V 电平、共地 | 直接接传感器和负载继续堆叠 |
| 测试值一直不变 | 标签是否 TEST、是否烧对 stage3/4 | 宣称真实传感器已通过 |
| 某个通道故障 | 对应供电/地址/接线；SHT CRC 与 I2C 错误 | 返回固定正常数值掩盖故障 |
| ACK 成功但无动作 | 外部驱动/负载供电和端子，单独记录物理动作 | 用 GPIO 直接供电 |
| 屏幕数据变旧/未知 | USB 断线、采样有效性、样本年龄、真实来源 | 强制画成 OK 或切模拟 |
| 小程序无法访问 | Korvo IP、同网段、令牌、手机权限/隔离 | 把电脑模拟器成功当手机通过 |
| MQTT 不通 | Broker 进程、电脑 IP、1883、用户名/密码、防火墙 | 先加云平台或另写后端 |

对每一条“已经成功”，同时写清它对应哪一层：代码完成、编译通过、主机测试通过，还是真机验证通过。官方资料与本项目代码的映射见 [SUPPORT_EVIDENCE](SUPPORT_EVIDENCE.md) 和 [network-evidence.json](network-evidence.json)；具体器件/模块不知道的地方保留待确认，不用其他板子的经验替代。
