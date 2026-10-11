# 小智 MCP

执行器通过主动连接、证书验证的 WSS 向小智提供有限工具，可与 MQTT/Home Assistant 及所选 Modbus/KNX 同时使用，不切换协议模式。

## 设置 { #setup }

本指南依据当前源码。[历史检查设备](live-verification.md)显示 0.6.3 却没有 MCP 菜单，具体镜像未知；菜单缺失时核对编译功能和嵌入 UI。

1. 管理员进入 **Connections → Xiaozhi MCP**，位于 MQTT/NTP 下方。
2. 填设备别名和自己的 `wss://…` 端点，端点通常含凭据；固件没有共享默认凭据。
3. 选择小智可见/可控通道，初始一个也不开放。
4. 启用并应用。端点默认隐藏，留空保留已存值，新值替换。管理员勾选 **Show endpoint** 才显式读取，单独显示不会修改或重连；隐藏会清除已读取值，尚未保存的新草稿仍保留但遮盖。
5. 等待 **Ready**。保存成功仅代表持久化，Ready 才表示 MCP 初始化完成；等待时间同步时配置 NTP。

Reconnect 用保存配置重连；禁用仍保留端点。要删除须在同一次保存中禁用并勾选 **Remove saved endpoint**。恢复出厂清除两代配置；普通删除后旧凭据可能留在恢复代，直到覆盖或复位。存于文件系统，不新增闪存加密。

Installer/Operator/Viewer 可看状态；只有 admin 能读写配置、查看工具表或重连。普通 GET、状态、日志和工具结果不包含端点；显式显示端点接口是例外。含凭据构建禁止启用 WebSockets 依赖调试日志。

## 工具与继电器行为 { #tools-and-relay-behavior }

工具后缀为完整 12 位 STA MAC 身份，换上行不变；别名支持 Unicode，改名会重连刷新注册表。外部通道 **1–6**。

| 工具前缀 | 行为 |
| --- | --- |
| `actuator_status_` | 只返回开放通道、修订、协议和故障；协议 0 off、1 RTU、2 TCP、3 KNX |
| `actuator_get_alias_` | 返回别名和稳定身份 |
| `actuator_set_relay_` | 必填 `channel`、布尔 `on` |
| `actuator_pulse_relay_` | 必填 `channel`、`request_id`，使用保存脉冲时长 |
| `actuator_all_off_` | 只操作开放且启用通道，先整体检查，任一受影响通道锁定则全部拒绝 |
| `actuator_identify_` | 五秒识别，RGB 禁用时失败 |

无开放通道时不注册继电器修改工具。所有命令在执行器任务运行，遵守禁用/锁定，不能解锁、换协议、复位、升级或编辑凭据。来源为 `xiaozhi_mcp`，网页/HA/KNX 状态同步。

脉冲 ID 保留 60 秒，可跨短暂重连，最多 16 条；满时拒绝新脉冲，不淘汰未过期记录。同 ID 不同参数或设置修订被拒绝。不自动重放修改，不保证跨重启恰好一次。命令可能已执行但响应丢失，换新 ID 前先查状态。小智断线本身不关闭输出，现有网络/总线策略仍有效。

## 运行与排错 { #runtime-and-troubleshooting }

状态包括 Disabled、Waiting for network/time、Connecting、Initializing、Ready、Retrying、Error、Paused。仅配网 AP 不是外连上行；服务跟随所选 IPv4 Wi-Fi/以太网。支持 DNS 和 IPv4 主机，拒绝 IPv6 字面量。

生产只接受 `wss://`，按嵌入 CA 验证主机名和证书，要求合理系统时间，无不安全 TLS 开关。`authentication_failed` 表示握手 HTTP 401/403，需换有效端点；`connection_failed` 包含 DNS/TCP/TLS、握手/心跳错误，检查网络、时间和信任，不暴露原始远端错误。认证失败约 60 秒重试，其他使用 1–60 秒带抖动指数退避。

TCP 和 TLS 握手各配置五秒；DNS 使用 Arduino 栈。适配层先建立验证后的安全连接，再交给固定版本 WebSocket 库，避开其两分钟默认握手；库自动重连关闭，由服务控制。MCP 初始化期限 20 秒，均为配置值而非实时保证。

`configuration_invalid` 保留损坏/不支持记录，须有意恢复出厂处理；`storage_failed` 保留旧有效设置。并发修订冲突先重新读取。OTA 暂停 MCP，最多等三秒确认传输停止，未停完则 OTA 报错供重试；失败 OTA 恢复 MCP，复位/重启/睡眠停止接命令，不依赖浏览器在线。

限制：端点 2048 字节、别名 64 UTF-8 字节、合并输入 8192 字节、嵌套 8、响应 16384 字节、单次 ArduinoJson 分配预算 49152 字节、队列四操作、执行期限两秒。底层原始帧分配上限 15 KiB；工具每秒 10 次、突发四次。发布前仍需实测堆、栈和延迟。

## API { #api }

无参数工具接受省略 `arguments` 或 `{}`，`_meta` 属请求元数据而非工具 schema。历史修复见[参数兼容记录](tasks/xiaozhi-mcp-tool-parameters-fix.md)。曾有保存时 `Stack canary watchpoint triggered (httpd)`，见[崩溃记录](tasks/xiaozhi-mcp-stack-overflow-fix.md)；当前 HTTP 栈至少 8192 字节，系统状态 `http_stack_min_free_bytes` 可读生命期最小余量。

| 路由 | 权限 |
| --- | --- |
| `GET /rest/xiaozhiMcpStatus` | 已认证 |
| `GET`, `POST /rest/xiaozhiMcpSettings` | admin |
| `GET /rest/xiaozhiMcpTools` | admin |
| `POST /rest/xiaozhiMcpReconnect` | admin，排队成功 202 |
| `POST /rest/xiaozhiMcpEndpoint` | admin，显式读取，正文 `{"revision": <current revision>}` |

显示响应含 `endpoint`、`revision`，`Cache-Control: no-store`；过期修订 409，畸形请求 422。普通响应脱敏。UI 在隐藏、重新加载、保存成功、标签隐藏和导航时丢弃读取值，不写 localStorage；未保存新草稿隐藏时只遮盖。

设置 POST 接受 `revision`、`enabled`、`alias`、`channel_mask`，可选只写 `endpoint` 和布尔 `clear_endpoint`。省略/空端点保留，清除不能同时替换，启用必须有端点。GET 额外给 `schema_version`、`endpoint_configured`、`endpoint_host`，这些只读字段不要发回 POST。

HTTP：400 正文错误、401 未认证、403 无权、409 状态/修订冲突、422 设置无效、503 存储/服务不可用。公开错误只有稳定 `error`、`field`，不回显秘密。功能关闭时不注册路由。

## 构建与开发 { #build-and-development }

`FT_XIAOZHI_MCP` 全局默认 0，Waveshare 开启编译，运行时仍关闭；要求 `FT_SECURITY=1`、`FT_NTP=1`，禁止 `SERVE_CONFIG_FILES`，不依赖 MQTT。固定 `links2004/WebSockets@2.7.2`。实现为项目原生代码，未复制 WLED/厂商包装或授权试用逻辑。

`XiaozhiMcpService` 统一设置/状态/生命周期，协议/设置和传输模块独立，`XiaozhiMcpAdapter` 拥有有界执行器队列。DurableStore 在 `lib/framework` 共用。

```text
python scripts/test_xiaozhi_mcp.py
pio run -e waveshare-relay-6ch -e waveshare-relay-6ch-mcp-off -e waveshare-relay-6ch-mcp-no-mqtt
```

模拟器不连接小智，通过认证控制 API 的 `{"type":"mcp","state":"ready"}` 设置确定状态。`mcp-only`、`mcp-off` 是 UI 测试配置，不是有效生产编译组合。契约测试含设置、脱敏、权限、保存失败和复位。

`esp32dev`、`esp32-wt32-eth01` 用 LTO 保持双 OTA，构建后执行 `python scripts/check_firmware_size.py <environment>` 检查真实镜像，不能只看链接估算。

隔离板可使用 `interface/scripts/mock-xiaozhi-mcp.mjs`，设置 `MCP_TEST_CERT`、`MCP_TEST_KEY`，可选 `MCP_TEST_BIND`、`MCP_TEST_PORT`。测试固件必须信任测试证书，不关闭 TLS。该对端初始化、列工具、只读查询，不操作继电器。伪传输测试不证明实机证书拒绝或云端互通。

发布验收仍需用户提供端点、隔离板、无效证书/主机名测试、协议共存、OTA/复位，以及 24 小时/100 次重连耐久测试，记录实测，不能以模拟器或构建替代。

## 历史验证：2026-10-09 { #validation-record-2026-10-09 }

英文原始记录保留了基于 `7fd6d69` 的软件验证和限制：模拟器 25 项、翻译两项、类型检查零错误警告、产品及 MCP 变体构建通过；Firefox 主机启动失败，C3 本地工具链下载未完成，WT32 余量较小。后续授权硬件测试见[历史记录](tasks/xiaozhi-mcp-hardware-validation.md)。这些不作为本次 0.6.4 重测结果；详细原始表保留在英文版的同名历史段落。
