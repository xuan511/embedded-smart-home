# 方案支持性与验证溯源 / Support and evidence

核查日期：2026-10-27。结论：**ESP32-S31-Korvo-1 V1.1（原配 LCD/触摸）作为上位控制与显示中心，STM32 作为采集与执行节点，通过 Korvo USB-A Host 通信，有官方硬件与软件能力支持。** 这是基于下列能力组合得出的工程判断，不是乐鑫对本项目整套硬件的实测认证。

本项目唯一实现通路：

```text
Korvo USB-A Host → CP2102/CP2102N（10C4:EA60）→ 3.3V UART → STM32 PA9/PA10
Korvo：用户操作、阈值持久保存与同步、LCD/触摸、可选 SD 日志
STM32：温湿度/光照采集、灯/风扇驱动、本地基础自动规则和断链保护
```

Korvo 是人机操作和参数控制中枢。按照前述需求，基础自动规则仍只在 STM32 执行，失联时保持有效数据的本地联动；两板不会各算一次再互相覆盖输出。“采集中心”包含与传感器同侧的执行和本地保护，并非只能采集不能控制。

## 官方证据链

固定 SDK 为 `espressif/esp-idf@d35ea023029363d97c42565f85e0db23486f26b2`。下表的 GitHub 源码链接固定提交；在线指南仍可能更新，不能把 latest 页面的将来修改当成历史快照。

| 编号 | 官方来源 | 可以支持的结论 | 不能据此声称 |
| --- | --- | --- | --- |
| S1 | [Korvo V1.1 用户指南：接口表第5项](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html#description-of-components) | USB-A 接 S31 USB 2.0 OTG HS，以 Host 身份连接下游设备；Type-C Power/UART 用途不同 | 任意 STM32 USB 插座都可通信 |
| S2 | [V1.1 官方原理图](https://dl.espressif.com/schematics/esp32-s31-korvo-1-schematics.pdf)，第2/3页 | J2、USB_DP/DM、U14、LCD/SD 实际线路；无须改主板取 GPIO | 学习板的排针顺序、供电、LED 已确认 |
| S3 | [固定 SDK CDC Host 示例 README](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/examples/peripherals/usb/host/cdc/README.md#L1-L34) | Supported Targets 包含 ESP32-S31；官方示例包含 CP210x 等 USB-UART | 已运行本项目真机，或可忽略不同桥芯片驱动 |
| S4 | [同提交 CDC 示例 C 代码](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/examples/peripherals/usb/host/cdc/main/usb_cdc_example_main.c#L141-L180) | 根据 VID 选择专用 `cp210x_vcp_open`，Host/CDC 安装、收发与断线处理是正式 SDK 用法 | CP210x 可当成不需厂商请求的普通原生 CDC |
| S5 | [S31 HAL 控制器表](https://github.com/espressif/esp-idf/blob/d35ea023029363d97c42565f85e0db23486f26b2/components/esp_hal_usb/esp32s31/usb_dwc_periph.c#L13-L25) | 控制器0使用 UTMI，`internal_phy_io=NULL`，不是 S3 GPIO19/20 复用方案 | 仅由宏名就可认定某 GPIO 空闲 |
| S6 | [BSP 1.0.1 的 USB 实现](https://github.com/espressif/esp-bsp/blob/6db6acb86a7dd56d17bf42093d2119dd9b0c14f5/bsp/esp32_s31_korvo_1/src/bsp_usb.c#L33-L50) | `bsp_usb_host_start` 安装 Host 并创建事件任务；不重新设置 LCD/SD GPIO | mode/limit 参数已实现电源切换或软件限流（此版本未使用两参数） |
| S7 | [BSP 1.0.1 显示/触摸](https://github.com/espressif/esp-bsp/blob/6db6acb86a7dd56d17bf42093d2119dd9b0c14f5/bsp/esp32_s31_korvo_1/src/bsp_display.c#L213-L310)、[SD 实现](https://github.com/espressif/esp-bsp/blob/6db6acb86a7dd56d17bf42093d2119dd9b0c14f5/bsp/esp32_s31_korvo_1/src/bsp_storage.c#L75-L87) | LCD、GT1151、SD 继续由本板 BSP 管理 | USB+屏+SD 已通过本项目实物连续运行 |
| S8 | [USB 1.6.0 默认控制器选择](https://github.com/espressif/esp-usb/blob/630f1c4e61f71d62bb57ff1b840931da24c3329f/host/usb/src/usb_host.c#L655-L659) | `peripheral_map=0` 选择控制器0，与 S5 相符 | 需要手动接出 D+/D− 或抢 LCD/SD 引脚 |
| S9 | [CP210x 2.2.0 发布源码](https://github.com/espressif/esp-usb/blob/ebfa6f84ee7c7c070dcc244c79573eeab228298e/host/class/cdc/usb_host_cp210x_vcp/include/usb/vcp_cp210x.h#L12-L36) | 选定单口 10C4:EA60、接口0及专用 C API | CH340、FT232、定制 VID/PID 自动兼容 |
| S10 | [S31 USB Host 指南：Features & Limitations](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s31/usb_host.html#features-limitations) | Host 支持 HS/FS/LS；需遵守当前栈的限制 | 通过任意 USB Hub 连接 Full-Speed 桥都可工作 |

**连接限制：CP210x 直接接 Korvo USB-A，不加入 USB Hub。** 官方 Host 文档说明 HS 主机经 Hub 连接 FS/LS 设备存在 Transaction Translator 限制。CP2102N 的 [原厂数据表](https://www.silabs.com/documents/public/data-sheets/cp2102n-datasheet.pdf)明确它是 Full-Speed 设备。500mA 是 Host 口硬件上限描述，灯和风扇仍由独立电源供电。

## 发布版本与固定源提交

这些映射直接取自对应 registry 发布包 `idf_component.yml` 的 `repository_info`，不是从名字猜测的 tag。下载由 Component Manager 及 `dependencies.lock` 固定；包 hash 见锁文件。`docs/evidence-sources.json` 另记录本机留存官方原理图/发布包的 SHA-256，用来识别本次检查的字节。

| 发布组件 | 版本 | 官方仓库提交 |
| --- | --- | --- |
| `esp32_s31_korvo_1` | 1.0.1 | `espressif/esp-bsp@6db6acb86a7dd56d17bf42093d2119dd9b0c14f5` |
| `usb` | 1.6.0 | `espressif/esp-usb@630f1c4e61f71d62bb57ff1b840931da24c3329f` |
| `usb_host_cdc_acm` | 2.4.1 | `espressif/esp-usb@eb0f02163be96dfc753cf43d1207329e44f1ca60` |
| `usb_host_cp210x_vcp` | 2.2.0 | `espressif/esp-usb@ebfa6f84ee7c7c070dcc244c79573eeab228298e` |

BSP 头文件确实残留 GPIO20/19 的 USB 宏，而同一头文件把它们分配给 SD/LCD：[固定源码](https://github.com/espressif/esp-bsp/blob/6db6acb86a7dd56d17bf42093d2119dd9b0c14f5/bsp/esp32_s31_korvo_1/include/bsp/esp32_s31_korvo_1.h#L95-L176)。本项目不用这些旧宏；物理连接以原理图和 S31 UTMI 实现为准。

## 开发者社区交叉检查

- [esp-bsp #839，维护者 suda-morris 的具体评论](https://github.com/espressif/esp-bsp/issues/839#issuecomment-6182366764)确认 S31 LCD BSP 支持。该讨论前文出现 S3/S31 型号混淆，后续 AI bot 的长回复注明未经 Espressif 事实核查；本项目不把 bot 的 timing/最低版本建议当硬件事实。
- [esp-idf #18992](https://github.com/espressif/esp-idf/issues/18992)讨论 **S31 作为 USB Device 输出 console/启动日志** 的支持缺口，不是本方案 **Korvo 作为 USB Host** 的否定依据。它是社区问题报告，不能作为未经核实的 ROM 地址/补丁依据；本项目没有采用该帖的 ROM 修改。
- 未找到官方对“同一 S31 Korvo + 具体 STM32 学习板 + 原配 LCD + SD + 这些传感器/负载”的整套真机验收记录。没有这条记录就明确写没有，不能拼接其它芯片、板型或模式的经验冒充。

## 我们自己的实现与验证

USB Host、CP210x 驱动、LCD/触摸、SD API 有上述官方出处。ASCII 应用协议、阈值同步、自动滞回和故障策略是本项目代码，主机测试也由本项目编写，**不是乐鑫认证测试**。STM32 寄存器、传感器和驱动电路的事实分别依据 ST、Sensirion、Silicon Labs 与相应模块厂商资料，不能用乐鑫文档替代。

上一实现提交 `7c69b1b26b6e7930b22a171229bde06f165ccb85` 的 [PR CI](https://github.com/xuan511/embedded-smart-home/actions/runs/40497571500)已复查实际五个 job 和 Build 步骤，均成功；不只读取交付包里的自述 JSON。firmware job 采用固定 SDK、官方 `install.sh esp32s31` / `export.sh`、完整递归子模块及 `idf.py --preview build`。本机旧构建用过跳过无关子模块下载检查的优化，已如实披露；它不冒充完整官方安装验证。

本次增加严格环境检查：SDK 提交必须匹配，已跟踪 SDK 文件不得改动，所有递归子模块必须匹配，不能设置 `IDF_SKIP_CHECK_SUBMODULES=1`，RISC-V GCC 必须为该基线 16.1.0；CI 在编译前执行该检查。另修复 USB 收帧排队期间丢失接收时间的问题：延迟处理不能把旧样本/心跳或过期 ACK 重新变成新数据，测试结果单列在 VALIDATION。

**当前仍未进行真机验证。** USB 实际枚举/双向传输、LCD/触摸/SD 同时运行、STM32 板型/供电/LED、电平、传感器、负载、NVS 掉电和连续运行都须依 [ACCEPTANCE](ACCEPTANCE.md)逐步实测。编译和主机测试能支持“实现存在且可构建”，不能支持“实物演示已经通过”。
