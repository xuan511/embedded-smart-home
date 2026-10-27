# 实施与验收顺序 / Implementation and acceptance order

目标是完成一个简单、可接线、可解释、可重复演示的双板环境监测与控制系统。代码可包含后续阶段，但真机验收必须从阶段 1 开始，当前阶段未通过就不增加下一组外设。当前真机均未进行；编译/主机测试结果以 [VALIDATION](VALIDATION.md) 为准。

The goal is a simple, wireable, understandable two-board system. Later-stage code can be present, but hardware acceptance starts at stage 1 and advances only after the current gate passes. No hardware checks have been run in this session. See [VALIDATION](VALIDATION.md) for distinct build and host-test results.

| 顺序 / Order | 范围 / Scope | 完成依据 / Evidence needed | 真机状态 / Hardware status |
| --- | --- | --- | --- |
| ① 最小通信 / Minimum link | USB Host + CP2102N + UART；握手、双向消息、失联 / Handshake, responses, disconnect | 实际枚举身份、握手日志、拔插结果 / Enumeration and link logs | NOT RUN |
| ② 测试数据 + LED / Test data + LED | STM32 测试数据明确标记；控制板载 LED 并应答 / Labelled samples, LED, acknowledgement | 显示、命令、LED 实际动作与应答 / Display, physical LED and replies | NOT RUN |
| ③ 真实传感器 / Real sensors | SHT3x、BH1750，约 1Hz；独立有效性 / Real readings, independent validity | 趋势、断开/恢复和原始数据 / Trends, fault/recovery, raw data | NOT RUN |
| ④ 实际负载 / Real loads | 灯/低压风扇驱动；手动、自动、滞回 / Drivers, manual, auto, hysteresis | GPIO 返回和实际动作分别记录 / Returned state plus physical observation | NOT RUN |
| ⑤ 保存与异常 / Persistence/faults | NVS、同步、超时、过期、基础连续运行 / NVS, sync, timeout, expiry, running | 重启/重连/断线/运行记录 / Reboot, reconnect, fault and run logs | NOT RUN |

详细可执行步骤见 [ACCEPTANCE](ACCEPTANCE.md)，接线只按 [HARDWARE](HARDWARE.md)。STM32 使用 `STAGE=1..4` 控制启用范围，不把“所有模块一起接好”作为第一次运行前提。

Follow [ACCEPTANCE](ACCEPTANCE.md) and [HARDWARE](HARDWARE.md). STM32 `STAGE=1..4` limits enabled peripherals; the first run does not require all devices to be connected.

## 当前下一步 / Immediate next step

已知用户有 ST-Link；学习板具体板型、板载 LED 和转串口芯片尚未确认。先确认排针/供电/LED，取得已选 CP2102N 模块，再烧录两板阶段 1，保存一次握手、断开、重连的完整日志。该门槛通过后才烧阶段 2。

The user has ST-Link; the exact learning board, onboard LED and UART-converter chip remain unidentified. Confirm headers/power/LED, obtain the selected CP2102N module, then flash stage 1 and retain handshake/disconnect/reconnect logs before advancing to stage 2.

## 连续运行与论文材料 / Running evidence and report material

先做 30 分钟异常排查，再记录 2 小时连续运行；需要更长论文样本时另做 8 小时。记录开始/结束、实际时长、供电、复位、通信中断、传感器故障、阈值切换和操作响应。没有做过的项目保持 NOT RUN。

Start with a 30-minute functional run and then record two hours. An eight-hour run can be a later additional dataset. Record actual duration, power, resets, disconnects, sensor faults, threshold transitions and response. Leave unrun checks marked NOT RUN.

论文可围绕需求、双板分工、接线、驱动、简单协议、状态归属、滞回、异常处理和逐步联调展开。测试数据只证明链路和界面；真实采集、GPIO 状态、实际负载动作分别给证据。已有音频/内存/板载 I2C 等自检可以作为附录，不扩大基础验收。

The report can focus on requirements, board roles, wiring, drivers, protocol, ownership, hysteresis, failure handling and staged integration. Test data demonstrates the link/UI; real acquisition, GPIO state and physical loads require separate evidence. Existing optional diagnostics can be an appendix.

本轮不增加 AI、语音识别、云平台、手机 App、MQTT、Matter、OTA、自研 PCB、摄像头视频、风扇 PWM 或转速反馈，也不并行维护第二套 STM32 工具链。先完成上述闭环。

This scope excludes AI, voice recognition, cloud, phone apps, MQTT, Matter, OTA, custom PCB, camera video, fan PWM/tachometer feedback and a second STM32 toolchain.
