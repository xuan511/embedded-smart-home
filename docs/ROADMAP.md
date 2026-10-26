# 后续开发 / Development roadmap

目标：先完成可以解释、可以联调、可以演示的本科计科毕设。增加功能前先验证当前链路，不用一次接入全部板载外设。

Goal: a demonstrable computer-science graduation project with understandable code and testable hardware/software integration. Finish one end-to-end path before adding more devices.

## P0：板载最小闭环 / Onboard baseline

- 刷默认固件；屏幕明确显示模拟数据。Boot the default build with simulation labelled.
- 用实体按键、触摸、串口各切换一次 RGB 灯。Each input path should control the same LED state.
- 注入 30°C/50lux、27°C/150lux、25°C/300lux，解释滞回为什么保持/切换。Verify hysteresis rather than switching at a single noisy threshold.
- 改阈值、`home save`、断电重启，确认仅阈值恢复，模式回自动。Persist thresholds and verify reboot behavior.
- 按需做音频/内存/I2C 自检，记录真实输出；某项自检失败不应被当成主控制链路已经失败。Record independent peripheral results.

验收产物：一次完整演示录像、串口日志、板子与接线照片。The result is a short demonstration, console log and hardware photo.

## P1：真实环境采集 / Real sensing

- 接 SHT3x 和 BH1750，启用真实配置，检查地址/CRC/单位。Verify addresses, CRC and physical units.
- 用手握传感器/遮挡光线观察趋势，与参考仪器比较。Do not invent accuracy figures; record actual measurements.
- 分别断开温湿度、光照；验证故障独立，恢复后重新正常采集。Each failed channel should only affect its own AUTO output.
- 对读数做必要的短窗口平均，再决定是否需要滤波；避免先堆复杂算法。Add a small averaging filter only after observing actual noise.

验收：记录范围、采集周期、误差样本、掉线/恢复现象。Record measurement samples and fault behavior.

## P2：真实灯光与风扇 / Physical loads

- 默认 RGB 灯已经是真实灯光输出；需要另一盏灯和风扇时再加 MCP23017 + 驱动。Add external loads only after stable sensing.
- 验证上电关闭、手动切换、自动阈值、重复开关、控制电源掉电和负载供电波动。Check startup behavior and power disturbances.
- 如果论文要求执行反馈，再增加转速/电流采集，并区分“请求状态”“写输出成功”“真实反馈”。Add feedback only if required; the current ON label is a command.

## P3：稳定性与论文 / Stability and report

| 项目 / Check | 建议记录 / Suggested evidence |
| --- | --- |
| 连续运行 / Continuous operation | 先 2 小时，再 8 小时；记录复位次数、free heap、数据故障、输出切换次数 / 2h then 8h logs |
| 阈值边界 / Boundaries | 上下阈值附近来回变化，确认不抖动 / Exercise both hysteresis edges |
| 故障 / Faults | 单传感器断开、CRC 错误、数据超时、无 SD 卡 / Inject observable failures |
| 操作 / Inputs | 快速按键、触摸和串口连续命令；检查拒绝次数 / Check bounded queue behavior |
| 掉电保存 / Persistence | 明确 save 前后、重启后差异 / Compare unsaved vs saved thresholds |
| 输出 / Loads | 软件命令与实际动作分别记录 / Record command and physical action separately |

当前控制任务也执行低速传感器转换和 CSV 写入，并非硬实时设计。若实测卡写入阻塞或控制延迟不满足需求，再把采样/日志迁移到独立任务，用长度为 1 的最新值队列传给控制任务。不要在没有测量依据前先增加大量任务。

The controller currently performs low-rate sensor conversion and CSV writes. It is not a hard real-time design. If measured storage latency is unacceptable, move sampling/logging to separate workers with a latest-value mailbox.

论文可围绕需求分析、模块设计、任务与消息组织、驱动接口、阈值算法、异常处理、联调过程和测试结果展开。不要把未实现的云端/语音/相机预览写成成果。

The report can focus on requirements, modules, queue/state organization, drivers, control rules, integration and measured results. Describe future extensions as future work.

## P4：任选扩展 / Optional extensions

按兴趣选择一个即可 / Choose one extension at a time:

1. **局域网 Web**：先提供只读状态页，再接入控制请求；所有命令仍走 `home_submit()`。Start with read-only status, then the existing command queue.
2. **MQTT**：只回传有效读数/故障/命令状态，加入断线重连，不让云端失联影响本地自动控制。Keep local automation independent of cloud connectivity.
3. **本地语音**：先确认 ESP-SR 对 S31 的当前支持，再把确定命令映射到现有 action。The microphone test is not speech recognition.
4. **相机**：先验证官方取帧示例，再做独立预览；不与核心采集互相等待。Build frame capture before claiming video preview.
5. **USB/BLE**：独立验证官方 Host/HID/串口或 BLE 示例，再加入服务；勿复用 LCD/SD 占用的 GPIO。Use supported official components instead of guessed pin mappings.

不把 AI、联网、触摸屏、自研 PCB 或 OTA 作为基础毕设能否完成的前提。Keep the environmental monitoring/control baseline independently usable.
