# REST API 与事件

本页依据 `dev` 提交 `bf8cb12` 的 [`ActuatorApi.cpp`](https://github.com/betamoojw/edge_switch_actuator/blob/bf8cb12ab9ada4337751968332c7d5e17e056ccc/src/device/ActuatorApi.cpp)。可选框架功能编译关闭后，相应路由不存在。

## 认证与权限 { #authentication-and-permissions }

`POST /rest/signIn` 接收含 `username`、`password` 的 JSON，成功返回 `access_token`，失败 401。后续使用 `Authorization: Bearer <access_token>`，JSON 请求带 `Content-Type: application/json`。初始账号 `admin` 使用每台设备独立的设置密码。令牌最长八小时且绑定本次启动，管理账号编辑轮换签名秘密。`GET /rest/verifyAuthorization` 返回 200/401；这些接口不为 HTTP 增加 HTTPS。

设备 GET 的 `capabilities` 含 `configure`、`command`、`admin`、`channels`；掩码位 0–5 对应通道 1–6，63 表示全部。**REST 通道下标 0–5，UI/MCP/MQTT 使用 1–6。**

## 执行器路由 { #actuator-routes }

| 方法与路由 | 权限 | 契约 |
| --- | --- | --- |
| `GET /rest/device/status` | 已认证 | 输出、网络/协议、指示、计数、故障、审计 |
| `GET /rest/device/config` | 已认证 | 完整 schema 1 及当前修订 |
| `POST /rest/device/config` | Installer/admin | 完整配置及当前一般修订 |
| `POST /rest/device/commands` | Operator/Installer/admin | 运行命令及额外检查 |
| `POST /rest/protocol/transition` | Installer/admin | `mode`、一般 `revision`，保留其他设置 |
| `GET /rest/knx/config` | 已认证 | KNX 镜像/所有权/关联及独立修订 |
| `POST /rest/knx/config` | Installer/admin | [KNX 调试事务](knx-address-entry.md) |
| `POST /rest/knx/programming` | Installer/admin | 布尔 `active`，需 KNX 模式和 IPv4 上行 |

命令/切换/编程路径也注册 GET 并返回快照，普通读取建议 `/rest/device/status`。POST 非对象或序列化后大于 8192 字节返回 400。修改由执行器队列串行处理，队满 503。

### 继电器示例 { #relay-examples }

以下提交到 `/rest/device/commands` 会实际操作**通道 1**，仅获授权后执行：

```json
{"command":"relay","channel":0,"value":true,"requestId":"panel-0001"}
```

```json
{"command":"pulse","channel":0,"requestId":"panel-0002"}
```

响应包含 `ok`、`error`、一般 `revision`、`state`。单路禁用/锁定返回 409，通道权限拒绝 403。

### 命令 { #commands }

| `command` | 字段与限制 |
| --- | --- |
| `relay` | `channel` 0–5，布尔 `value`，需通道权限 |
| `pulse` | `channel` 0–5，使用已保存时长，需通道权限 |
| `all_on`、`all_off` | 先检查所有启用通道权限及锁定，任一失败整体 403 |
| `rgb` | `red`/`green`/`blue` 0–255，`brightness` 0–100 默认10，`seconds` 1–30 默认5，RGB 须启用 |
| `identify` | 五秒识别，受 RGB 启用及更高优先级限制 |
| `tone` | `hz` 500–4000 默认2000，`ms` 10–2000 默认100，`duty` 1–50 默认25，蜂鸣器须启用 |
| `acknowledge` | 停止当前提示音 |
| `unblock` | Installer/admin，`channel` 0–5，不检查该账号通道掩码 |
| `modbus_window` | Installer/admin，`seconds` 0–300 默认60，`peer` 默认 `rtu`，0 关闭 |
| `factory_reset` | admin 标志及 `confirm: "ERASE"`，清配置并重启 |

RGB、提示音、识别按角色授权，不是逐继电器授权，详见[权限范围](device-operation.md)。

### 修订与重试 { #revisions-and-retries }

先 GET 配置，再修改完整对象并带当前 `revision` POST，必须含六路、三组点击绑定及所有必填类型字段；成功递增修订。只切协议可使用：

```json
{"mode":"modbus_tcp","revision":1,"requestId":"mode-change-0001"}
```

把 `1` 换成刚读到的修订。模式为 `off`、`modbus_rtu`、`modbus_tcp`、`knx_ip`。响应含 `ok`、`error`、`revision`，成功后重读；KNX 保存返回已提交 `knx` 快照及 **KNX 独立修订**。

可选 `requestId` 最长 64 字符；按用户名+ID 缓存最多 16 个已完成操作、60 秒。同路径和完全相同序列化负载重试返回旧响应，同 ID 不同请求返回 409。缓存可淘汰且重启丢失，不是持久化恰好一次执行。丢响应时先查状态，尤其脉冲，不要直接换新 ID。

| 状态码 | 典型含义 |
| --- | --- |
| 200 | 读取或操作完成 |
| 400 | 格式错误/过大 |
| 401 | 未认证/无效令牌 |
| 403 | 角色/通道拒绝、锁定批量或不支持命令 |
| 409 | 修订/状态冲突、单路禁用/锁定、应用失败 |
| 422 | 字段/schema/范围无效 |
| 503 | 队列不可用或满 |

早期认证、形状和队列错误可能空响应。应用失败也可能是存储或协议启动失败，不一定只是修订，重试前看错误。

## 框架与集成路由 { #framework-and-integration-routes }

| 路由 | 用途与权限 |
| --- | --- |
| `GET /rest/features` | 公开编译功能及固件身份 |
| `GET /rest/wifiStatus`、`/rest/apStatus`、`/rest/systemStatus` | 已认证状态 |
| `GET`, `POST /rest/wifiSettings`、`/rest/apSettings` | admin 网络配置 |
| `GET /rest/scanNetworks`、`/rest/listNetworks` | admin 异步扫描 |
| `GET /rest/mqttStatus`、`/rest/ntpStatus` | 已认证，依功能 |
| `GET`, `POST /rest/mqttSettings`、`/rest/ntpSettings` | admin，MQTT 含可选 HA 发现 |
| `GET`, `POST /rest/ethernetSettings`；`GET /rest/ethernetStatus` | 仅以太网板型，设置 admin、状态认证 |
| `GET`, `POST /rest/securitySettings` | admin，序列化清空密码/签名秘密 |
| `GET /rest/generateToken?username=<name>` | admin 生成令牌 |
| `POST /rest/restart`、`/rest/factoryReset` | admin 生命周期操作 |
| `POST /rest/uploadFirmware` | admin multipart 上传 |
| `POST /rest/downloadUpdate` | admin，JSON `download_url` 指向应用 OTA |
| `GET /rest/coreDump` | 认证后下载崩溃诊断 |

执行器没有可选睡眠路由 `/rest/sleep`。五个 MCP 路由及脱敏见[小智](xiaozhi-mcp.md)，主题授权见 [HA](home-assistant.md)。

## 事件套接字 { #event-socket }

`/ws/events` 接入时认证。默认二进制 MessagePack，`EVENT_USE_JSON=1` 改为文本 JSON，逻辑信封含 `event`、`data`：

```json
{"event":"subscribe","data":"device.state"}
```

`device.state` 约每秒一次且只读，修改用 REST。退订用 `event: "unsubscribe"`；`{"event":"ping"}` 回 `{"event":"pong"}`。拒绝大于 8192 字节帧，发送待办有界。重连后读新 REST 快照并重新订阅，`lib/device/reconcile.ts` 合并部分状态。账号编辑不保证立即关闭已有连接，见[审查](actuator-source-review.md)。
