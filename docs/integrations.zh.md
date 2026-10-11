# 选择集成方式

浏览器不依赖现场总线。Protocol 页只选择一种：Off、Modbus RTU、Modbus TCP 或 KNX/IP。MQTT/Home Assistant 和小智 MCP 可同时运行；各来源共享输出，后接受的命令可覆盖先前命令。

| 路径 | 准备 | 首次验证 | 限制 |
| --- | --- | --- | --- |
| 浏览器/REST | 地址及账号 | 读取仪表盘或认证状态 | HTTP；角色与通道掩码生效 |
| Modbus RTU | RS485 主站、站号/串口参数、终端匹配 | FC04 偏移 0，数量 5 | 信任物理总线，无逐主站认证 |
| Modbus TCP | IPv4 上行、允许的主站，默认 502 | 同一组寄存器 | 最多四客户端，精确 IP 限制不是加密 |
| KNX/IP 路由 | IPv4 组播网络、地址规划 | 检查快照和组关联 | 无 KNX TP，不承诺隧道服务器；ETS 验收待完成 |
| Home Assistant | MQTT 代理、HA MQTT、启用发现 | 先看发现和可用性 | 代理 ACL 授权，不沿用网页掩码 |
| 小智 MCP | 私有 WSS 端点、NTP、开放通道 | Ready 后读取状态工具 | 默认无通道，不自动重放修改 |

## Modbus：先读后写 { #modbus-example-a-read-before-a-write }

传输已配置时，以站号 **1** 读取输入寄存器零基偏移 `0x0000`、数量 `5`，依次应为映射版本、能力、所选协议、网络状态和故障。版本应为 `1`，RTU/TCP 分别为 `1`/`2`。超时先排查传输，不要改用写入。`30001` 等显示地址因主站而异，优先使用零基偏移。

FC01 偏移 `0`、数量 `6` 读取命令输出。FC05 的 `0xFF00` 开、`0x0000` 关会操作硬件，仅在批准测试时执行；不支持厂商演示的 `0x5500`。详见[映射](actuator-modbus-map.md)、[验证工具](modbus-verifier-quickstart.md)、[RTU](modbus-rtu-relay-fat-sat.md)/[TCP](modbus-tcp-relay-fat-sat.md) 验收。

## KNX：命令与反馈分开 { #knx-example-separate-command-and-feedback }

按拓扑选择唯一地址，例如可用时的 `1.1.20`。通道 1 的对象 1 为 Switch、2 为 Block、3 为 Status；可用 `1/0/1` 关联 Switch、`1/1/1` 关联 Status，未用 Block 留空。这只是示例，不应直接套用到现场。三者采用已实现的一位行为；对象 DPT/标志的设计背景见[历史设计](actuator-knx-design.md)。

网页应用会修改配置。先协调 ETS/web 所有权，读取修订、验证关联，再按调试计划保存并读回。只有受控组写加独立触点观察才能验证切换，Ready 本身不够。见[地址配置](knx-address-entry.md)。

## Home Assistant：先发现再自动化 { #home-assistant-example-discover-before-automating }

确认设备与 HA 使用同一目标代理后再启用发现。应出现一个 Edge Switch 设备、六路继电器和诊断实体。主题 `edge_switch/<id>/relay/1/set` 接受精确 `ON`/`OFF`，**不得保留发布**；`<id>` 是实际设备身份，不是友好名称，发布会操作硬件。

先检查可用性和状态，再测试批准的单一路。禁用或锁定通道应不可用；ON 不是负载反馈。改名、换代理、关闭发现或移除设备前阅读[生命周期](home-assistant.md)。

## 小智：先查询状态 { #xiaozhi-example-begin-with-status }

按[MCP 指南](xiaozhi-mcp.md)设置私有端点，禁止公开。Ready 后从实际工具注册表查找 `actuator_status_<device-identity>`，传 `{}`，只返回开放通道；不要猜后缀。

设置继电器和脉冲是主动操作。脉冲要求 `request_id`；响应丢失时先查状态，再决定是否使用新 ID。去重有容量和时间限制，重启后丢失。文档检查设备没有 MCP 菜单，本节是源码说明，不是小智连接实测。
