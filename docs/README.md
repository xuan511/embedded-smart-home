# 文档入口 / Documentation index

第一次做嵌入式项目，请先读行动清单，确定当前关口，再查完整教程。主 README 是快速参考，下面的教程才是按操作顺序编排的学习路线。**所有实物测试目前都没有在本开发会话执行，不能照抄“预期现象”作为自己的实测结果。**

当前可以先完成不依赖实物的环境安装、两板构建和主机测试。板型、USB 身份及供电核对集中放到实际接线/烧录前，不影响先学习和准备软件。

| 顺序 | 文档 | 用途 |
| --- | --- | --- |
| 0 | [从这里开始：行动清单](START_HERE.zh-CN.md) | 每个关口先做什么、应看到什么、保存/回传哪些信息，以及何时可进入下一步 |
| 1 | [零基础完整教程](BEGINNER_GUIDE.zh-CN.md) | 认识接口/名词、安装、编译、分别烧录、逐步接线、联网与手机操作；每步有成功门槛和排错 |
| 2 | [硬件清单与接线](HARDWARE.md) | 对照实物信号名、方向、供电与共地；不凭学习板外形猜 LED 或供电脚 |
| 3 | [上板验收表](ACCEPTANCE.md) | 从 USB 握手、测试数据、传感器、负载到参数/异常逐项留证 |
| 4 | [MQTT 与微信小程序](NETWORK.md) | Windows Mosquitto、Korvo 网络配置、微信导入、同网段 HTTP 和协议字段 |
| 5 | [毕设实施、演示与实验记录](PROJECT_GUIDE.zh-CN.md) | 需求对应关系、拟定测试门槛、演示顺序、实验表、答辩解释 |
| 6 | [官方手册与实现对应表](OFFICIAL_REFERENCE.md) | 每项硬件/驱动事实的厂家资料、章节和代码位置，区分工程自定策略 |
| 7 | [验证记录](VALIDATION.md) | 按准确提交区分代码完成、编译、主机测试和真机验证 |
| 8 | [开源项目对照与许可证](OPEN_SOURCE_REFERENCES.md) | 六个作者仓库的固定文件、采用的模式、拒绝照搬的部分与许可证边界 |

开发时再看 [架构](ARCHITECTURE.md)、[两板行协议](PROTOCOL.md)、[路线与范围](ROADMAP.md)。历史板级纠正见 [HARDWARE_AUDIT](HARDWARE_AUDIT.md)，S31 支持证据见 [SUPPORT_EVIDENCE](SUPPORT_EVIDENCE.md)。

`official-reference-manifest.json` 记录本轮访问的原始资料及实际下载哈希；`evidence-sources.json`、`network-evidence.json` 保留早前证据。URL 的 latest 页面可能更新，固定源码提交与下载哈希用于识别本次核对版本；它们不是乐鑫对整套毕设的认证。

实机抓取命令在仓库根目录运行：

```powershell
python tools/live_check.py --ip 192.168.1.50 --seconds 1800 --output records/run-01.jsonl
```

把 IP 换成 Korvo 屏幕显示的实际内网地址。令牌交互输入，不写入日志。严格检查要求阶段 4、真实传感器、有效数据和正常 USB 同步；使用新版固件的 `stale_ms` 字段。加 `--require-mqtt` 才要求 MQTT 回报也在线。工具只记录 HTTP/设备回报，不能证明传感器标定或风扇实际旋转。

## English reading path

Start with [English README](../README.en.md), then follow [hardware wiring](HARDWARE.md), [staged acceptance](ACCEPTANCE.md), and the bilingual [network guide](NETWORK.md). The expanded Chinese beginner tutorial explains each action from an empty Windows environment. [Official references](OFFICIAL_REFERENCE.md) maps manufacturer documents to implementation; [validation](VALIDATION.md) separates host fixtures from physical tests. Do not treat expected results as observed measurements.
