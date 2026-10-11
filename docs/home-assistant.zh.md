# Home Assistant 集成

`waveshare-relay-6ch` 使用现有 MQTT 连接接入 Home Assistant，无需自定义 HA 组件。发现功能默认关闭，包括载入旧设置时；模板板型仍使用演示灯集成。

## 设置 { #setup }

1. 配置 HA 的 [MQTT 集成](https://www.home-assistant.io/integrations/mqtt/)，发现前缀保持 `homeassistant`，birth 主题保持 `homeassistant/status`。
2. 以管理员登录，在 **Connections → MQTT → Change MQTT Settings** 填写同一代理 URI 和凭据，启用 MQTT 及 **Home Assistant discovery** 后应用。
3. 应发现一个 **Edge Switch** 设备。继电器名称跟随执行器配置；修改名称或 MQTT client ID 不改变实体身份。

代理账号和 ACL 应只允许可信控制器发命令。MQTT 不继承 HTTP 用户和通道权限。

## 实体与主题 { #entities-and-topics }

`<id>` 为 `edge_` 加出厂 STA MAC 的 12 位小写十六进制，切换 Wi-Fi/以太网不变。基础主题 `<base>` 是 `edge_switch/<id>`。

| 实体 | 行为 |
| --- | --- |
| 六路继电器开关 | 经过统一继电器方法，含启用/锁定和 KNX 反馈；禁用或锁定时不可用 |
| 六个通道锁定二元传感器 | 表示锁定**或禁用**，不提供解锁权限 |
| 运行时间、协议、协议状态 | 只读诊断 |
| 故障、按键按下 | 当前故障及消抖输入；短按可能落在一秒快照间，应使用手势事件 |
| Identify 按钮 | 五秒识别，仍受 RGB 启用及优先级限制 |
| 手势事件 | `single`、`double`、`triple`；仅按键功能启用时发送，本地绑定仍执行，复位长按不作为自动化事件 |

| 主题 | 负载及保留规则 |
| --- | --- |
| `homeassistant/<component>/<id>/<entity>/config` | 发现 JSON，QoS 1，保留 |
| `<base>/state` | 无私有审计日志的状态 JSON，每秒及命令后发送，QoS 1，保留 |
| `<base>/relay/1/set` … `/relay/6/set` | 精确 `ON` 或 `OFF`，不得保留 |
| `<base>/identify/set` | 精确 `PRESS`，不得保留 |
| `<base>/event` | `{"event_type":"single"}` 或 `double`/`triple`，QoS 1，不保留 |
| 既有框架状态主题 | 保留 `online` 和遗嘱 `offline`，各实体共用 |

此版本的发现前缀和 birth 主题固定。代理重连、HA 的 `online` birth 或继电器改名后重发发现和状态。带 retained 标志的命令丢弃，错误主题/负载忽略；没有 toggle、复位、协议选择或配置写命令。队列上限 16，超出即丢弃。状态不是触点或负载反馈。

## 兼容性与生命周期 { #compatibility-and-lifecycle }

MQTT 可与 RTU、TCP、KNX/IP 或 Off 同时运行，不修改协议、启动、脉冲、按键、断连策略、Modbus 权限或 KNX 配置。MQTT 命令不会喂 RTU 总线看门狗；之后的协议/按键命令可覆盖输出，代理断连不会新增关闭策略。

MQTT 连接时关闭 **Home Assistant discovery** 会清除保留发现记录和状态。若离线关闭，`/config/home-assistant-discovery` 标记允许重启后补清理；清理完成前保持旧代理可达。换代理会在旧代理留下记录，应手动移除或先关闭发现。恢复出厂或永久移除前，也应在已连接时关闭发现；复位不能清理不可达代理。

`FT_MQTT=0` 时适配器不编译。仅执行器暴露该可选设置；既有执行器 schema 和 REST 路由不变，`/rest/mqttSettings` 增加默认 `home_assistant_discovery: false`。

## 审查与验证 { #review-and-validation }

[10 月 10 日只读检查](live-verification.md)中 MQTT 关闭，没有实测发现。原实现基线 `16e5935`：网络回调仅排队有效命令或请求重新发现，不写 GPIO 或获取执行器锁；状态在执行器任务采样，统一覆盖 REST、按键、Modbus、KNX、脉冲和看门狗变化。

安装开发依赖后运行 `python scripts/test_home_assistant.py`，以真实 ArduinoJson 和伪传输/RTOS/硬件验证发现、过滤、命令交接、互锁、保留命令拒绝、非保留手势、改名、移除、重试及重连。`python scripts/test_native.py` 覆盖解析器和产品契约。`interface/` 中运行 `npm run check`、`npm run test:sim`、`npm run test:i18n`；浏览器执行 `npx playwright test --project=chromium --grep MQTT`。

原记录曾通过执行器/通用 S3 构建、原生及适配器测试、零错误警告的 Svelte 检查、13 项模拟器、两项翻译和两项 Chromium MQTT 测试。这些不是本次重新执行的结果。

硬件验收仍需代理和 HA 实例，获授权后逐路测试所有命令来源、锁定/禁用、改名、HA/代理重启、设备断连及关闭发现，并测量实际输出与可用性。主机测试不会刷写或完成现场调试。参考 [MQTT discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)、[switch](https://www.home-assistant.io/integrations/switch.mqtt/)、[event](https://www.home-assistant.io/integrations/event.mqtt/)。
