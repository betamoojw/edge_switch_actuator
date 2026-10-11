# Modbus RTU 继电器 FAT/SAT 验收程序

这是可重复的工厂/现场功能验收程序，**不是已执行结果**。流程借鉴 IEC 62381 的 FAT/SAT/SIT 方法，功能码及异常依据 [Modbus Application Protocol V1.1b3](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf)，RTU 帧和时序另依据[串行指南 V1.02](https://www.modbus.org/file/secure/modbusoverserial.pdf)。时长和循环数是项目测试条件，并非标准规定。此程序不构成 IEC/Modbus 认证、安全评估或电气安装批准。参见[寄存器契约](actuator-modbus-map.md)。

## 1. 范围与限制

验证身份、映射、传输参数、初始全 OFF、FC05 回显、FC01 线圈、FC02 已应用输出、逐路独立切换、重复切换、ON/OFF 保持及最终恢复。FC02 不是触点反馈；物理验收须独立使用合适电表、隔离指示器或记录仪，并记录仪器身份。

一次只测试一路，使用隔离低压负载，不接市电。继电器电气寿命、抖动、温升、绝缘、开断能力、EMC、功能/过程安全、完整协议符合性不在本次范围；异常帧时序、广播静默及所有波特率需单独验证。

## 2. 人员、设备与前提

操作员须有负载上电权限，建议另一观察员全程监控触点和急停。准备生产代表板及已识别镜像、测试主机、六路安全负载、独立急停和唯一 JSON 证据路径。

1. 记录带时区日期、测试/观察者、型号/序列/板版、固件版本及哈希、传输配置、夹具/仪器、拓扑和报告位置。
2. 检查配线、隔离、额定值、保护与急停，确保可承受重复切换和最长 ON。
3. 停止无关自动化及主站写入，确认实际传输参数和权限。
4. 独立确认全部负载 OFF；工具也检查六路 FC01/FC02，全 OFF 才启动。
5. 接线、状态不确定、通信失败或急停不可用时停止，不自动把未知状态归零。
6. 可选 I/O 测试须启用 RGB、按键及 **Allow manual indicators and diagnostic commands**，双击现有绑定为 identify（6），手动亮度非零，无故障覆盖，全部输出 OFF。不要为测试修改持久按键绑定。


RTU 使用已确认的 USB-RS485 适配器，A/B、必要参考地和物理两端终端正确；关闭其他串口程序，保持一个主站。下面沿用计划示例 COM22、unit 10、19200/8E1、掩码 63，**不是出厂值或本次实测值**，执行前核对。RS485 必须启用且初始化。

仅当初始线圈和输出读数一致、负载已确认安全且操作员明确授权时，可逐路指定 `--prepare-off-channel N` 关闭已知初始 ON 通道。未列出的 ON 通道、读失败或不一致均停止且不写。全 OFF 时省略此选项。

## 3. 验收矩阵

| 项目 | 操作与通过条件 |
| --- | --- |
| 基线 | 身份、映射版本、能力、协议、参数及负例全部通过，目标与记录一致 |
| 初始状态 | 六路 FC01/FC02 均 OFF，否则不开始输出测试 |
| 短周期 | CH1 到 CH6 逐路三轮 1 秒 ON/1 秒 OFF；每个边沿和保持后检查 FC05 回显及全部状态 |
| 长保持 | 每路 ON 至少 30 秒，其他路 OFF；随后全部 OFF 至少 5 秒 |
| 互不干扰 | 任一步只有选定通道 ON 或全部 OFF，非选定路不变化 |
| 最终状态 | 全部线圈和输出 OFF，物理接点另行确认 OFF |
| 证据 | 每步有实际结果和通过/失败；任何通信、输出、触点或恢复异常均失败/中止 |
| I/O-01 | 一笔 FC16 写 0x0200–0x0203，opcode 1 识别；读命令序号/结果、FC04 RGB/原因及 FC02 活动状态，应为五秒非黑白色、手动原因，继电器不变 |
| I/O-02 | 操作员短双击 BOOT，工具轮询按下与手势计数；恰好增加一次双击，并按已有 identify 绑定显示同样白色 |
| I/O-03 | 观察者另外记录真实白光，寄存器值不证明光学输出 |

I/O 测试不命令继电器，但识别和按键仍是主动操作。权限缺失应经网页授权，不能绕过。不要长按 BOOT，也不要测试不在本案例范围的单击/三击。物理按键不能远程注入。

默认最少保持时间为 `6 × (3 × (1 + 1) + 30 + 5) = 246` 秒，另加通信与执行开销。主机单调时钟衡量保持时间，不测接点切换延迟。


RTU 另须通过 CRC/站号、RS485 就绪、FC08 子功能 0 回显及异常 02/03，前后帧错误计数不得增加。实际触点未测时，物理验收标为未完成。

## 4. 中止与恢复

首个错误即停止：非预期通道变化、矛盾/丢失读回、失联、RTU 新帧错误、不安全负载、操作员顾虑或紧急情况。工具在 `finally` 尽力关闭测试通道，并明确报告恢复失败；电源、链路、固件或驱动失效时不能保证成功。

必要时使用独立急停，直接验证安全状态，按批准流程恢复。调查原因并建立新的测试记录后才能重测，不把不完整运行当完整验收。保留失败和中止报告。

## 5. 运行器与证据

完整继电器测试必须有 `--confirm-safe-loads`，先跑基线再全 OFF 预检，失败不启动。各保持时长大于 0、最多 3600 秒，循环 1–100。以下会操作输出，仅在授权后执行，每次使用新报告名：


```powershell
py scripts/verify_modbus.py --transport rtu `
  --serial-port COM22 --unit 10 --baud 19200 --parity E --stop-bits 1 --timeout 5 `
  --exercise-all-relays --toggle-cycles 3 `
  --toggle-on-seconds 1 --toggle-off-seconds 1 `
  --on-seconds 30 --off-seconds 5 --confirm-safe-loads `
  --output evidence/modbus-rtu-relay-fat-sat.json
```

已确认指示权限及观察者后，可单独执行 RGB/按键测试：

```powershell
py scripts/verify_modbus.py --transport rtu `
  --serial-port COM22 --unit 10 --baud 19200 --parity E --stop-bits 1 --timeout 5 `
  --exercise-device-io --confirm-indicator-observation `
  --button-timeout-seconds 60 `
  --output evidence/modbus-rtu-device-io-fat-sat.json
```

`safeLoadsConfirmed` 和观察确认只记录人员声明，不是传感器或硬件互锁。JSON 与物理 OFF/ON/OFF 测量、测试者签字、镜像身份、配置快照、夹具及可选网络抓包一起保留，去除秘密。退出码 0 为请求的软件检查通过，1 为检查失败，2 为参数或准备无效。软件成功不代替物理验收。
