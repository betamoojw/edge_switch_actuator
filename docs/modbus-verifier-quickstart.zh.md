# Modbus 验证工具快速入门

`scripts/verify_modbus.py` 对 TCP 或 RTU 做可重复冒烟检查。默认不请求改变继电器状态，但会发送非法 FC05 值以验证异常响应，**不是纯只读流量**。本次文档工作未运行设备测试；接线、切模式及主动测试均须授权。时序、异常帧、并发、耐久及物理输出见[历史完整计划](MODBUS_VERIFICATION_VALIDATION_PLAN.md)。

## 1. 准备 { #1-start-safely }

隔离市电负载，使用安全低压夹具，确认触点初态。网页选择已批准的 RTU 或 TCP，记录站号及参数。RTU 需启用 RS485，完成 A/B、必要参考地及终端匹配。所有命令从仓库根目录执行。

## 2. 本地自检 { #2-check-the-local-tool }

```powershell
py scripts/verify_modbus.py --self-test
py scripts/verify_modbus.py --help
```

无需设备，自检应输出 `Modbus verifier self-test passed`。

## 3. TCP { #3-modbus-tcp-quick-start }

确认 IPv4、TCP running、允许测试机来源且端口可达。以下 `192.0.2.10` 是文档保留地址，必须换成批准的目标：

```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.0.2.10 --port 502 --unit 1 `
  --output evidence/modbus-tcp-smoke.json
```

默认端口 502、unit 1、超时两秒，可加 `--timeout 5`。连通性辅助检查为 `Test-NetConnection 192.0.2.10 -Port 502`，同样替换地址。

## 4. RTU { #4-modbus-rtu-quick-start }

```powershell
py -m pip install pyserial
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name
py scripts/verify_modbus.py --transport rtu `
  --serial-port COM4 --unit 1 --baud 19200 --parity E --stop-bits 1 `
  --output evidence/modbus-rtu-smoke.json
```

关闭占用适配器的软件，COM4 只是示例。站号、波特率、校验、停止位必须与设备相同；`--parity` 支持 E/O/N，停止位 1/2，出厂为 19200 8E1。

## 5. 可选主动测试 { #5-optional-relay-test }

单路在原命令加 `--write-channel 1`（允许 1–6）。工具先读初态、请求相反状态、读 FC01/FC02，再在 `finally` 尝试恢复。须确认实际触点及恢复，软件恢复不能取代隔离或急停。通道需启用、未锁定且掩码允许。

### 全通道 FAT/SAT { #all-channel-fatsat-sequence }

使用 [TCP](modbus-tcp-relay-fat-sat.md) 或 [RTU](modbus-rtu-relay-fat-sat.md) 的完整验收条件。需要主动测试时在传输参数后添加：

```text
--exercise-all-relays --toggle-cycles 3
--toggle-on-seconds 1 --toggle-off-seconds 1
--on-seconds 30 --off-seconds 5 --confirm-safe-loads
```

默认每路三轮 1 秒 ON/1 秒 OFF，再 ON 30 秒、OFF 5 秒。每步检查全部六路，首个失败中止，尽力恢复测试通道 OFF。`--confirm-safe-loads` 是操作员声明，不是硬件互锁；JSON 不是接点测量。

RTU 额外验证 FC08、串口配置和前后帧错误计数。仅对明确授权关闭的初始 ON 通道，重复指定 `--prepare-off-channel N`。全 OFF 时省略；未授权 ON、读失败或 FC01/FC02 不一致均停止，不能自动归一化未知状态。

### RGB 与物理按键 { #rgb-indicator-and-physical-button-test }

需 RGB/按键启用、双击为 identify、亮度非零、全部输出 OFF、观察人员及安装权限 **Allow manual indicators and diagnostic commands**。在传输参数后加：

```text
--exercise-device-io --confirm-indicator-observation
--button-timeout-seconds 60
```

工具以 FC16 邮箱调用识别，再等一次物理短双击，验证计数、手势和 RGB 值；不改绑定或开继电器。不要长按 BOOT，也不要测试其他点击。观察者另外记录白光，确认标志不是光学传感器。权限拒绝时经认证网页授权，不绕过策略。

## 6. 报告 { #6-understand-the-report }

终端及 `--output` 输出同一 JSON，`checks` 每项含 `name`、`passed`、`detail`。默认检查 FC43/14 身份、映射/能力/状态、六路线圈及输出、传输参数、未映射地址异常 02、非法 FC05 异常 03，RTU 再查 FC08。写入或 I/O 仅在相应选项出现时运行。

PowerShell 用 `$LASTEXITCODE`：0 全部请求检查通过，1 设备检查或运行错误，2 参数/依赖/目标无效。证据与固件哈希、配置、板卡身份和日期一起保存，失败记录不要覆盖。

## 7. 常见问题 { #7-common-failures }

- TCP 拒绝/超时：检查 running 而非 waiting、IP/端口、精确 IPv4 限制、上行、防火墙及端口冲突。
- RTU 超时：检查模式、RS485 初始化、串口参数、A/B 定义、终端/参考地、驱动、指示器和端口占用；接线变更需先隔离。
- 异常 02：地址未映射或对象不可访问、禁用、锁定、策略拒绝，按[映射](actuator-modbus-map.md)核对。
- 异常 03：数值、数量或编码无效，默认非法 FC05 用例有意期待该结果。
- 身份/版本不符：先保存报告，确认目标与候选镜像，勿贸然改配置。

## 8. 最低复查要求 { #8-minimum-revisit-checklist }

记录镜像及板卡身份；确认模式运行、参数正确；本地自检通过；冒烟退出 0 并留证；主动测试使用隔离负载并恢复；用独立主站复查核心读写；完成完整计划中剩余 P0 项。任何未测物理项都应标为未完成。
