# 任务 — 在 Edge Switching Actuator 全项目中集成 Xiaozhi MCP

**状态：** 实施计划，尚未实现。  
**日期：** 2026-10-09  
**目标项目：** [`betamoojw/edge_switch_actuator`，`dev` 分支](https://github.com/betamoojw/edge_switch_actuator/tree/dev)，审查时 HEAD 为 `7fd6d69ce46ed5d1993b9feff164dae61721b21b`。  
**参考项目：** [`betamoojw/WLED_UserMods`，`UserModsDev` 分支，`usermods/Xiaozhi_MCP`](https://github.com/betamoojw/WLED_UserMods/tree/UserModsDev/usermods/Xiaozhi_MCP)，审查时 HEAD 为 `138da958f6d05c2b24fe4a4bfbaede088f32b59b`。  
**交付范围：** 技术设计、实施计划、API/Schema 提案、UI 规划、测试策略及验收门禁。**不修改应用程序代码。**

## 1. 目标

为六通道 ESP32-S3 开关执行器增加一项可选、安全、主动向外连接的小智云端 MCP WebSocket 功能。新管理页面作为 **Connections（连接）** 下的第三个菜单项，与 MQTT、NTP 并列：

```text
IoT Platform（物联网平台）
  Switching Actuator（开关执行器）
  Connections（连接）
    MQTT
    NTP
    Xiaozhi MCP       <-- 新增菜单项
  WiFi
  Users（用户）
  System（系统）
```

用户应能够配置和启用连接、查看连接状态、安全地管理凭据，并可选择是否允许 AI 智能体控制指定继电器通道。现有 MQTT/Home Assistant、KNXnet/IP、Modbus RTU/TCP、Wi-Fi/Ethernet、本地按键、OTA、用户角色及执行器安全逻辑必须保持完整。**MCP 是一个补充性的远程命令接口，而不是互斥的 KNX/Modbus 协议选择器中的第五个选项。**

### 设计承诺

- 默认禁用，端点地址默认为空；远程写操作也**默认禁用**。
- 采用经过验证的 `wss://` TLS 连接，并要求设备时间有效；不得回退至不安全的 TLS。
- 复用执行器的中央命令和继电器控制逻辑；网络回调不得直接修改 GPIO。
- 必须在命令实际执行时验证权限和允许的通道掩码，而不仅是在 UI 中检查。
- MCP 配置与 `/config/actuator` 分开存储，不强制升级执行器配置 Schema。
- 在硬件板卡和网络协议栈支持的情况下同时支持 Wi-Fi 和以太网。
- 扩展现有无硬件模拟器、功能开关、主题、七种语言和自动化测试。
- 不自动继承 WLED 的商业许可或试用限制。

## 2. 参考代码审查与适配性

| 源文件 | 已确认的行为 | 移植决策 |
|---|---|---|
| `Xiaozhi_MCP.h` | WLED `Usermod` 类；`isEnabled`、终端别名和端点地址 | 改为目标项目原生的配置与服务；绝不引入 WLED 全局变量 |
| `Xiaozhi_MCP.cpp` | 依赖网络的初始化、MCP 客户端循环、状态/配置回调 | 使用明确的状态机及目标项目现有网络支持层 |
| `Xiaozhi_MCP.cpp` | 六个 WLED 工具：`led_status`、`led_get_alias`、`led_power`、`led_brightness`、`led_color`、`led_effect` | 映射为执行器专用工具，而非 WLED 亮度/灯效控制 |
| `WebSocketMCP.h/.cpp` | 主动发起的 WebSocket、工具注册表、JSON-RPC `ping`、`initialize`、`tools/list`、`tools/call` | 重新实现有边界限制的协议处理，并验证服务提供方使用的协议变体 |
| `platformio_override_xiaozhi_mcp.ini` | WLED ESP32 构建；`links2004/WebSockets@^2.7.2` | 不将 WLED 的覆盖配置直接复制到当前 PIOArduino 项目 |
| `readme.md` | 小智连接、别名、端点，以及 WLED `License_Mgnt` 的 60 分钟试用 | 许可证集成应作为独立产品决策 |

### 关键发现

1. **源码中硬编码了疑似有效的凭据：** `Xiaozhi_MCP.h` 包含完整的 `wss://...token=...` 端点字符串。应将其视为已泄露，**在小智平台撤销或轮换凭据**，从后续源码默认值中移除，且绝不在示例代码或日志中再次出现。仅删除某个文件中的内容并不会清除 Git 历史记录。
2. **未证明 TLS 证书验证有效：** 参考代码调用 `WebSocketsClient::beginSSL()`，但调用位置并未明确证明 CA/主机名验证已启用。目标实现必须强制执行经过验证的 TLS。
3. **JSON-RPC 健壮性存在缺口：** 参考实现直接拼接消息 ID 到 JSON，假定文本载荷以空字符结尾，使用较小的固定 JSON 文档，将 `tools/call` ID 视为整数，且对畸形或分片输入缺少健壮处理。应改用感知长度的 JSON 解析与序列化，保留数值型和字符串型 ID，正确处理通知，并限制载荷大小。
4. **生命周期与并发：** WLED 使用全局 MCP 客户端/别名及静态单例回调，初始化重试和重连逻辑分散在不同层。应实现单一所有者及确定性的状态转换。
5. **安全：** 源工具回调直接写入 WLED 全局变量。工业继电器执行器必须让远程命令经过现有安全、权限及审计逻辑。
6. **许可：** 复制代码前，应审查源代码分叉版本及原始 `xiaozhi-mcp` 库的许可证和署名要求。

### 已审查的目标架构

- `src/device/Actuator.h/.cpp`：`Actuator` 管理六路继电器输出，在 `relay()` 中检查被禁用/阻止的通道，启动 KNX/Modbus，处理脉冲定时器、指示灯、看门狗和 FreeRTOS 命令队列。`Actuator::loop()` 在递归互斥锁保护下串行执行设备操作。
- `src/device/ActuatorApi.cpp`：经过身份认证的 GET/POST 路由（如 `/rest/device/status`、`/rest/device/config`、`/rest/device/commands`），角色/通道掩码权限控制、命令队列、60 秒请求 ID 去重及审计。
- `src/device/DeviceConfig.h/.cpp`：包含六通道配置及 `mode=off|modbus_rtu|modbus_tcp|knx_ip`；MCP 集成不得更改此配置模式。
- `src/device/DurableStore.h`：具有校验和的双代持久化写入机制（用于完整性和回滚，**不是加密**）。
- `lib/framework/NetworkSupport.h`：可用的 IPv4 上行连接（不包括配网 AP）及默认网络接口跟踪，抽象支持 Wi-Fi/以太网。
- `lib/framework/Features.h`、`FeaturesService.cpp`、`/rest/features`：构建时/运行时功能声明。
- `interface/src/routes/menu.svelte`：按功能开关显示的 Connections 菜单，现含 MQTT 和 NTP。
- `interface/src/routes/connections/mqtt/*` 和 `ntp/*`：可参考的 `SettingsCard`、`Collapsible`、状态、设置及 REST 实现。
- `interface/src/lib/i18n/*`：七种语言（英语、德语、西班牙语、法语、波兰语、简体中文、繁体中文），通过 TSV 生成语言目录。
- `interface/simulator/*`、`interface/tests/simulator/*`、`interface/tests/e2e/frontend.spec.ts`：现有模拟设备和浏览器测试。

## 3. 高层架构

```text
小智 AI 智能体与云端 MCP 端点
        | 主动发起 wss://，验证 CA 与主机名
        v
XiaozhiMcpTransport        （Socket、JSON-RPC、心跳、重试）
        |
        v
XiaozhiMcpService          （工具注册表、连接状态、配置、访问控制 ACL）
        | 有边界的命令/结果队列；不允许不可信回调直接写 GPIO
        v
执行器命令分发器（硬件状态的唯一所有者）
        | 现有继电器启用/阻止检查、通道掩码、调试配置锁定
        +--> 继电器输出 [1..6]
        +--> KNX 反馈 / Modbus 可见状态
        +--> MQTT/Home Assistant 及 device.state 事件
        +--> 审计记录

需认证的管理 REST 接口 + 已脱敏的状态事件
        ^
        |
SvelteKit: /connections/xiaozhi-mcp
```

### 所有权与线程策略

- 在 Socket/TLS/DNS 操作、重连、Flash 写入或生成外发响应期间，传输/网络回调不得持有 Actuator 互斥锁。
- 将传入的工具请求转换为紧凑的强类型命令，放入执行器的单一所有者执行上下文队列，并在受限截止时间内异步返回结果。
- 优先提取 Web 和 MCP 调用方共用的命令校验/执行逻辑。**不得**通过本机 HTTP 回环调用，也不得为 MCP 调用方伪造管理员 JWT。应为 `xiaozhi-mcp` 指定明确的安全主体、受控权限掩码和允许操作。
- 避免不同回调/任务间死锁；断线或配置变更时，待处理回调和响应缓冲区的生命周期必须明确可控。
- 保持执行器写入命令的时间先后顺序。现有通道禁用/阻止规则及看门狗安全规则优先于远程请求。
- MCP 断开连接不得强制更改输出，也不得取消已经启动的脉冲定时器。现有 KNX/Modbus 网络断线策略仍具有最终决定权。

## 4. 传输层与协议

仅在比较以下方案后，才定义并实现 `XiaozhiMcpTransport` 接口及其具体实现：（a）兼容当前基于 Arduino 的 PIOArduino 构建、支持已验证 TLS 的 ESP-IDF WebSocket 客户端；（b）固定版本的 `links2004/WebSockets`。需验证 CA 证书包集成、堆内存占用及分片行为。**不得假设 WLED 所用 WebSocket 库或覆盖配置可直接兼容。**

生命周期状态：`disabled`、`not_configured`、`waiting_for_network`、`waiting_for_time`、`connecting`、`connected`、`initializing`、`ready`、`reconnecting`、`error`。

- 只有在功能已启用、端点有效、默认上行网络可用且可信时间足以验证证书时，才能启动连接。
- 完整解析 WebSocket URL（协议、主机、可选端口、路径、查询参数 token）；拒绝不安全的 `ws://`、片段标识符、URL 用户信息、CR/LF 及超长值。生产环境默认主机白名单为 `api.xiaozhi.me`；其他主机须通过明确策略批准。默认拒绝本地/私有目的地址，以缓解 SSRF 风险。
- 强制验证 CA 证书链和主机名；禁止 `setInsecure` 或静默降级。TLS 错误须使用脱敏错误码报告。
- 使用序列化器而非字符串插值构造 JSON-RPC 2.0 消息。保留字符串/数值 ID 类型，区分请求和通知；支持 `ping`、`initialize`、`tools/list`、`tools/call`、协议错误及与提供方实际协商的协议版本。
- 处理明确的帧长度、分片文本帧、无效 UTF-8/JSON、请求大小上限、缺失字段及不支持的方法，并返回恰当的 MCP/JSON-RPC 错误。
- 工具注册在重连时必须幂等；别名/策略更新时重建工具列表，或者在不支持变更通知时重新连接。
- 由单一模块负责重连，采用指数退避加随机抖动、受限重连次数、心跳与握手超时；不得在忙循环中重复输出日志。
- 为多执行器配对提供稳定、全局可区分的设备后缀；避免参考实现仅截取前四个字符。示例：`actuator_set_relay_7f43a19c`（仅为示例后缀）。

## 5. MCP 工具清单与安全策略

建议的 v1 工具（所有工具名均附加稳定的设备标识后缀）：

| 工具基础名称 | 参数 | 功能 | 授权要求 |
|---|---|---|---|
| `actuator_get_status` | `{}` | 返回脱敏后的六路继电器状态、标签、启用/阻止状态及工作模式 | 只读 |
| `actuator_get_alias` | `{}` | 返回 MCP 别名及稳定的工具设备标识 | 只读 |
| `actuator_get_capabilities` | `{}` | 返回公开的工具及允许操作的通道 | 只读 |
| `actuator_set_relay` | `{"channel":1,"state":"on"}` | 设置继电器状态 | `allowWrite` 与对应通道位 |
| `actuator_pulse_relay` | `{"channel":1}` | 按已配置的持续时间进行脉冲控制 | `allowWrite` 与对应通道位 |
| `actuator_all_off` | `{}` | 关闭符合条件的继电器 | 额外启用 `allowBulkOff` 并进行完整权限校验 |
| `actuator_identify` | `{}` | 触发现有的限时设备识别效果 | 明确的策略授权和速率限制 |

**MCP 通道编号：1..6；内部 `Actuator::relay` 索引：0..5。** 每次实际执行时均需检查边界和权限，不能仅依赖 `inputSchema`。v1 不公开 WLED 颜色、亮度、灯效、任意 GPIO、任意 HTTP 请求、管理员/用户配置、OTA、恢复出厂设置、KNX ETS 编程、网络凭据或批量全部开启操作。

- 远程写操作初始为禁用，且 `allowedChannelsMask=0`；应先确保只读工具可正常运行。
- 对已禁用或阻止的通道拒绝写入；KNX 编程、应用下载或忙碌状态禁止并发控制时，也必须拒绝写入。在安全允许时继续提供只读工具。
- 遵守现有 KNX/Modbus 模式选择；MCP 不改变当前协议。只要 IP 上行链路在线，Modbus RTU 模式可以与主动外连的 MCP 并存。
- 在有界审计记录中加入 `origin=xiaozhi-mcp` 和工具请求元数据，但不记录凭据或包含秘密信息的原始消息。
- 对远程写操作进行速率限制和去重，包括云端重发请求的情况；断线后不得引发意外的重复脉冲。
- 云端端点 token 不等同于设备本地管理员身份认证主体。

请求示例：

json

成功响应示例：

json

无效或被拒绝的调用应返回明确错误，例如 `invalid_arguments`、`unauthorized`、`channel_disabled`、`channel_blocked`、`commissioning_busy`、`timeout`、`service_unavailable`。被拒绝的操作绝不能改变输出状态。

## 6. 配置、持久化与 API

使用独立的 `/config/xiaozhi-mcp` 配置存储（如果遵循框架命名约定，也可使用 `/config/xiaozhiMcpSettings`）。复用 `DurableStore` 的完整性校验语义，以支持修订版本和回滚，但**不能把校验和当作保密机制**：生产环境的凭据保护需要 Flash/NVS 加密和合适的设备配置下发机制。恢复出厂设置必须清除端点/密钥并停止重连。

| 字段 | 类型/默认值 | 约束 |
|---|---|---|
| `enabled` | 布尔值 `false` | 必须由管理员明确启用 |
| `alias` | 字符串 `"Switching Actuator"` | 去除首尾空白/规范化，1..32 个显示字符；正确支持 Unicode |
| `endpoint` | 机密字符串，默认为空 | 生产环境使用 `wss://`；合法且获准的主机/路径/token；长度受限（建议 1024 字节，最终确定前需评估） |
| `allowWrite` | 布尔值 `false` | 独立控制远程物理输出权限 |
| `allowedChannelsMask` | 整数 `0` | 仅允许六位，范围 0..63 |
| `allowBulkOff` | 布尔值 `false` | 额外明确启用 |
| `revision` | uint32 `1` | 乐观并发控制 |

### 建议的 REST 端点

| 方法/路径 | 角色 | 响应/行为 |
|---|---|---|
| `GET /rest/xiaozhiMcpStatus` | 已认证用户 | 安全的连接阶段、别名、仅端点主机名、工具数量、最近错误码、重试/计数器 |
| `GET /rest/xiaozhiMcpSettings` | 管理员 | 已脱敏配置、`endpoint_configured`（布尔值）——**绝不返回 token 或完整原始 URL** |
| `POST /rest/xiaozhiMcpSettings` | 管理员 | 严格字段校验、冲突检查、原子持久化及重新配置 |
| `POST /rest/xiaozhiMcpAction` | 管理员 | `connect` / `disconnect` / `reconnect` / `test`；操作范围受限 |

通过 `FeaturesService` 在功能 API 中增加 `"xiaozhi_mcp": true|false`；新增经过身份认证且内容脱敏的 `xiaozhi.mcp.status` 事件，并提供 REST 轮询回退。未认证返回 401、无权限返回 403、冲突/忙碌返回 409、数据无效返回 422、服务不可用返回 503。在禁用安全机制的构建中，远程写入与配置管理应不可用，而不能退化为匿名访问。省略 `endpoint` 字段时必须保留已存储的密钥；明确提交 `clearEndpoint` 才清除密钥。

建议的设置 POST（token 为占位符；测试中绝不使用真实凭据）：

json

已脱敏 GET 示例：

json

状态示例（数值仅作说明）：

json

## 7. 前端/UI 规范

### 导航

修改 `interface/src/routes/menu.svelte`：加入合适的 Tabler 图标；将 Connections 父菜单显示条件扩展为 `page.data.features.mqtt || page.data.features.ntp || page.data.features.xiaozhi_mcp`；把 Xiaozhi MCP 作为第三个子菜单，路径为 `/connections/xiaozhi-mcp`；保留现有页面标题和菜单激活状态逻辑。层级、缩进及字体排版应与提供的截图一致。

新增文件：

```text
interface/src/routes/connections/xiaozhi-mcp/
  +page.ts
  +page.svelte
  XiaozhiMCP.svelte
```

采用现有 Svelte 5、Tailwind/DaisyUI 和组件（`SettingsCard`、`Collapsible`、`InputPassword`、`Spinner`、Toast 通知），复用登录令牌和语义化表单规范。页面应包括：

1. **状态：** 禁用/未配置/等待网络/等待时间同步/连接中/已连接/初始化中/就绪/重连中/错误；展示别名、仅端点**主机名**、设备 ID、已注册工具、最近一次安全错误、最近就绪时间及下次重试时间。
2. **设置（仅管理员）：** 启用开关、别名、掩码显示的端点替换输入框及清除操作、远程写入授权、六个通道复选框、可选的批量关闭授权、保存/放弃修改。明确告知 AI 命令可能驱动物理继电器动作。
3. **操作/诊断：** 连接/断开/重新连接/测试按钮、脱敏指标、校验/冲突提示；设置应无需重启即可生效。
4. **工具与配置帮助：** 当前工具列表、读/写标记、提供方端点设置和凭据轮换指南、多设备别名、故障排查。

必须区分 **WebSocket 已连接**与 **MCP 已初始化并就绪**。不得在 GET 响应、浏览器日志、Toast、页面 URL 或错误详情中暴露 token。采用组件控制器管理的挂载时轮询并在卸载时清理，或订阅已脱敏状态事件并提供 REST 回退。适配移动端、键盘操作、触控目标尺寸、无障碍及所有现有主题。

### 本地化

更新 `interface/src/lib/i18n/translations.tsv`，通过 `npm run i18n:build` 重新生成 `messages.json`，确保所有现有语言均通过 `interface/tests/i18n/catalog.test.mjs`：English、Deutsch、Español、Français、Polski、简体中文、繁體中文。使用符合当地工业控制行业习惯的术语；MCP/TLS/WebSocket 等协议标识在适当场景下保持不变。

### 类型定义

扩展 `interface/src/lib/types/models.ts` 或独立模块，增加 `XiaozhiMcpSettings`、`XiaozhiMcpStatus`、`McpConnectionPhase` 和强类型操作/错误契约。从前端角度，端点密钥必须是只写数据。

## 8. 按文件划分的具体工作清单

| 目标文件 | 工作内容 |
|---|---|
| `features.ini`；`platformio.ini`；`lib/framework/Features.h` | 定义 `FT_XIAOZHI_MCP`（通用构建默认 0，执行器构建显式启用），固定 TLS/WebSocket 依赖版本和体积预算 |
| `lib/framework/FeaturesService.cpp` | 在 `/rest/features` 暴露 `xiaozhi_mcp` |
| `src/device/XiaozhiMcpConfig.h/.cpp`（新增） | 严格的配置模型、修订版本、密钥处理、持久化/重置 |
| `src/device/XiaozhiMcpTransport.h/.cpp`（新增） | 安全的主动外连 Socket、JSON-RPC、分片、重连/心跳、状态 |
| `src/device/XiaozhiMcpService.h/.cpp`（新增） | MCP 工具注册表、策略、结果分发及诊断 |
| `src/device/Actuator.h/.cpp` | 服务所有权、初始化、网络状态切换、队列及清理、恢复出厂设置 |
| `src/device/ActuatorApi.cpp` | 受保护的 MCP API、统一的命令/权限适配器、审计 |
| `interface/src/routes/menu.svelte` | 在 Connections 下增加第三个菜单项 |
| `interface/src/routes/connections/xiaozhi-mcp/*`（新增） | 页面、状态、配置、操作及帮助 |
| `interface/src/lib/types/models.ts` | 强类型 API/状态模型 |
| `interface/src/lib/i18n/translations.tsv`、`messages.json` | 七种语言翻译 |
| `interface/simulator/profiles.mjs` | 支持的配置档功能标志、默认配置和模拟状态 |
| `interface/simulator/device.mjs`、`framework.mjs`、`server.mjs` | REST 契约、连接模拟器、云端工具调用注入和故障注入钩子 |
| `interface/tests/simulator/*`、`interface/tests/e2e/frontend.spec.ts` | 协议、角色、UI 和回归测试 |
| `docs/*` | 配置教程、安全 token 轮换、诊断、多设备支持及版本说明 |

除必要的功能声明和通用辅助代码外，执行器专属集成应优先放在 `src/device`，避免修改通用 SvelteKit 框架。

## 9. 模拟器与测试

扩展现有无硬件模拟器，而不是另建一套 UI。使用 Node `ws` 和假 token 提供隔离的模拟云端 WebSocket 端点；CI 中不访问真实小智服务。模拟器必须与固件 JSON 契约完全一致，且分别模拟 `connected` 与 `ready`。

**后端/单元与安全测试：** 配置写入读取与回滚；默认禁用；token 脱敏；URL/主机/端口/别名长度；CA/设备时间错误；JSON-RPC 初始化、ping、工具列表和调用、字符串/数值 ID、通知、无效 JSON、无效工具名/参数、分片或超大帧；命令队列满/超时；重复请求 ID；权限掩码和不可用通道；已阻止/已禁用输出；KNX ETS 忙碌/调试配置；脉冲定时器；远程断线、重启及恢复出厂设置。

**模拟/浏览器测试：** 菜单位置与功能开关；管理员编辑、保存与重新加载；凭据不回传；基于角色的只读 UI；后端 401/403/409/422/503；七种语言目录；浅色/深色/其他主题；移动端渲染；连接/错误/空状态；事件 Socket 重连与 REST 轮询回退；不引入新的浏览器控制台错误；两台执行器使用不同工具后缀。

**硬件/回归测试：** 当前 ESP32-S3 板卡固件构建；基线空闲/最小堆内存与 Flash 大小；TLS 握手峰值内存、主循环/继电器响应延迟、OTA 分区容量；使用一次性 token 连接真实端点；实际继电器控制、Wi-Fi 及可选以太网故障切换、24 小时持续运行测试、云服务中断后的恢复；现有 KNX ETS 编程和状态；现有 Modbus RTU/TCP、MQTT/HA、硬件 BOOT 按键、脉冲安全、OTA 及恢复出厂设置。

建议执行的命令（仅在实施后进行测试）：

bash

还需编译禁用 MCP 的通用构建，并确认不支持该功能时不会额外暴露相关路由。创建本计划时**尚未运行**这些测试。

## 10. 交付阶段与阶段门禁

### 阶段 0 — 基线确认与传输方案技术验证

- [ ] 记录现有构建/测试状态、栈/堆/Flash 占用及固定依赖版本。
- [ ] 确定支持 CA 和主机名验证的 PIOArduino 传输实现方案。
- [ ] 使用一次性端点验证服务提供方握手和 MCP 协议版本，包括多设备场景。
- [ ] 单独审查复制代码的许可证及商业许可要求。

**门禁：** 不存在硬编码 token 或不安全 TLS 行为；传输方案有验证证据支持。

### 阶段 1 — 配置与只读后端

- [ ] 增加构建功能开关和 `/rest/features` 标识。
- [ ] 增加持久化、经过校验且脱敏的配置及按角色授权的 REST API。
- [ ] 增加传输生命周期、JSON-RPC 和三个只读工具。
- [ ] 增加脱敏状态事件/指标及主机侧测试。

**门禁：** 具备安全的只读 MCP 连接，禁用/重启行为可预测。

### 阶段 2 — 远程继电器控制

- [ ] 提取共享的执行器命令/权限分发逻辑。
- [ ] 实现从 MCP 到执行器的有界队列、超时及完成结果关联。
- [ ] 增加远程写入显式授权、通道掩码、设置/脉冲/批量关闭/识别策略。
- [ ] 确认与 KNX/Modbus 并存及调试配置过程的安全性。

**门禁：** MCP 无法绕过任何 Web/设备安全规则，也不得在执行器任务上下文之外修改输出。

### 阶段 3 — 完整前端与本地化

- [ ] 增加 `Connections → Xiaozhi MCP` 菜单及状态/配置/帮助/操作页面。
- [ ] 增加强类型契约、轮询/事件刷新、管理员/角色行为和适合移动端的无障碍表单。
- [ ] 完成七种语言本地化及所有主题适配。

**门禁：** 导航与截图一致，配置界面满足安全要求。

### 阶段 4 — 模拟与回归

- [ ] 扩展现有模拟器和模拟云端；测试中不得使用真实秘密信息。
- [ ] 增加原生/协议、浏览器、安全及故障/重连测试。
- [ ] 执行回归测试以及真机验收、内存、TLS 和故障测试。
- [ ] 编写云端配对、凭据轮换、恢复、安全、许可证及发布文档。

**门禁：** 完整验收清单已通过并留存记录。本计划本身不宣称测试已通过。

### 阶段 5 — 生产环境发布

- [ ] 审查威胁模型、远程输出的显式授权及通道权限。
- [ ] 验证 OTA/回滚和恢复出厂设置处理。
- [ ] 发布版本中默认关闭远程写入，不内嵌凭据。
- [ ] 确保 `FT_XIAOZHI_MCP=0` 时原有行为保持不变。

## 11. 发布验收检查清单

- [ ] 在支持该功能的构建中，`Connections → Xiaozhi MCP` 正确显示在 MQTT/NTP 下方。
- [ ] 七种语言、主题、移动端 UI 和用户角色均正常工作，无回归。
- [ ] 配置可以安全地保存、更新、脱敏、重新加载、禁用和清除。
- [ ] WSS TLS 已验证且设备时钟有效；不存在未经验证的回退或 token 日志。
- [ ] 能够针对真实小智服务完成初始握手和 `tools/list`。
- [ ] 只读工具及显式启用的继电器/脉冲控制返回正确结果。
- [ ] 所有远程写入均强制执行通道掩码、启用/阻止状态及调试配置状态检查。
- [ ] 传输断线、请求重复或恢复出厂设置期间无意外继电器动作。
- [ ] Web、KNX、Modbus、MQTT/HA、BOOT 输入、OTA 及身份认证功能继续正常工作。
- [ ] 模拟服务提供方、单元测试、浏览器 E2E 和设备硬件测试全部通过。
- [ ] 峰值 RAM、延迟、固件与 OTA Flash 占用符合实测限制。
- [ ] 依赖许可证及密钥下发机制已完成生产级审查。

## 12. 尚未解决的设计决策

1. 当前小智端点必须支持哪个协议版本及何种工具命名规则？
2. 针对具体 PIOArduino 版本，应选择原生 ESP-IDF WebSocket 传输，还是经过验证的 `links2004/WebSockets`？
3. 如何在生产环境强制实施 Flash 加密，以及 token 的轮换和配置下发？
4. 是否在首版 v1 工具中公开远程批量关闭和指示灯设备识别功能？
5. 是否仅允许 `api.xiaozhi.me`，还是实际部署需要经过审批的私有服务提供方？
6. 是否应为此商业开关执行器单独实现现有 WLED 许可证/试用机制？

**不要从 WLED 源码中直接推断这些决策；必须先明确决策，再合并功能。**

## 13. 参考链接与实施代理交接

**参考源项目：** [Xiaozhi_MCP README](https://github.com/betamoojw/WLED_UserMods/blob/UserModsDev/usermods/Xiaozhi_MCP/readme.md) · [Xiaozhi_MCP.cpp](https://github.com/betamoojw/WLED_UserMods/blob/UserModsDev/usermods/Xiaozhi_MCP/Xiaozhi_MCP.cpp) · [WebSocketMCP.cpp](https://github.com/betamoojw/WLED_UserMods/blob/UserModsDev/usermods/Xiaozhi_MCP/WebSocketMCP.cpp)。

**目标项目：** [Actuator.cpp](https://github.com/betamoojw/edge_switch_actuator/blob/dev/src/device/Actuator.cpp) · [ActuatorApi.cpp](https://github.com/betamoojw/edge_switch_actuator/blob/dev/src/device/ActuatorApi.cpp) · [menu.svelte](https://github.com/betamoojw/edge_switch_actuator/blob/dev/interface/src/routes/menu.svelte) · [FeaturesService.cpp](https://github.com/betamoojw/edge_switch_actuator/blob/dev/lib/framework/FeaturesService.cpp) · [simulator](https://github.com/betamoojw/edge_switch_actuator/tree/dev/interface/simulator) · [frontend tests](https://github.com/betamoojw/edge_switch_actuator/tree/dev/interface/tests)。

**编码代理指令：** 本文档是实施计划，并非已实现的证明。开始工作前重新检查最新 `dev` 分支 HEAD；按阶段实施；保留所有不相关的现有行为；绝不将端点 token 提交到代码仓库；在宣称达到发布就绪状态前，记录实际测试及其结果。
