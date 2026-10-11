# 当前 dev 源码审查

审查日期 **2026-10-11**，基线 [`bf8cb12`](https://github.com/betamoojw/edge_switch_actuator/tree/bf8cb12ab9ada4337751968332c7d5e17e056ccc)，固件 **0.6.4**，当时远端 `dev` 与此一致。后续文档提交不改变固件。本页是源码审查，不是完整安全审计或新的硬件/ETS 认证测试。[原设计](actuator-production-design.md)保留历史意图，[架构](architecture.md)说明实际实现。

## 自 10 月 10 日以来的变化 { #changes-since-the-10-october-review }

0.6.4 修复发布 API 仓库标识，侧栏以文档站替换 Discord；默认固件产物更新到 0.6.4，新增跟踪 `buildRelease/Edge_S3_Relay_6CH.knxprod`。与 `ced3e6d` 比较，没有后端 API 或执行器行为变化，见[版本变更](release-notes.md)。

[10 月 10 日原审查](source-review-2026-10-10.md)保留早期问题，包括当时 C3 大小检查失败。这不是 0.6.4 的新构建结果。本次未刷机或主动操作硬件。

## 基线中仍存在的问题 { #findings-still-present-at-the-reviewed-baseline }

| 优先级 | 源码证据 | 影响与建议 |
| --- | --- | --- |
| P1 | `FSPersistence.h` 以 `w` 打开活动文件，读取失败后写默认值 | 网络/用户/MQTT 设置可能在中断写时丢失；应改为验证提交并测试恢复 |
| P1 | `UpdateIndicator.svelte`、`GithubFirmwareManager.svelte` 按 `.bin` 和板型子串选文件 | 可能选到合并镜像或 MCP 变体；应精确匹配目标及 `_ota.bin`，当前优先手动选择 |
| P2 | `build_interface.py` 只扫描 `interface/src/` 时间戳 | 静态资源、锁文件、Vite 改动可能漏嵌入；暂用删除生成 `WWWData.h` 强制重建 |
| P2 | `factory_settings.ini` 的 `Europe/Berlin` 配 `GMT0BST,M3.5.0/1,M10.5.0` | 名称和偏移不一致，配网时选匹配时区 |
| P2 | `EventSocket.cpp` 取消订阅通过下标创建未校验名称 | 已认证客户端可创建空映射项；需查找/注册检查和数量上限，尚未复现远程资源耗尽 |
| P2 | EventSocket 仅接入认证，帧和发送不再核对账号/令牌 | 既有连接可能超过会话到期或账号修改；须明确撤销机制。`device.state` 虽只读仍可继续观察 |

这些是发现，不是本次文档修复的固件代码。URL 问题已解决，文件选择问题仍由源码可见；未尝试 OTA。[既有设备检查](live-verification.md)还发现 KNX 三击提示与保存绑定可能不符，以及 0.6.3 设备缺 MCP 菜单；未确定镜像身份前不能归因于当前源码。

## 相比模板阶段已实现的改进 { #implemented-improvements-since-the-template-review }

- 网络启动前设置安全 GPIO；独立执行器任务接收有界 HTTP/集成命令，产品不使用演示灯逻辑。
- `NetworkSupport` 依据选定 IPv4 上行，排除仅 AP；上行变化重启监听。
- REST 检查角色和通道掩码，状态事件无写回调；通用 `EventEndpoint<T>` 本身不是角色校验层。
- 唯一 NVS 设置身份、加盐 PBKDF2、API 隐去密码与签名秘密；令牌绑定启动、最长八小时，用户修改轮换签名密钥。
- DurableStore 提供校验、多代、KNX 分块和恢复；挂载保留损坏数据，复位有持久标记。
- KNX 保存地址/表/参数支持回滚、独立修订和持久所有权，产品参数有契约测试。
- 订阅修改互斥、去重，HTTP 任务上有界发送。
- MCP 验证 WSS、配置脱敏、有界解析/队列、开放掩码、脉冲去重及 OTA 协调，独立于 MQTT。
- 固件按当前编译输出打包，CI 检查实际分区容量。

## 运行与资格验证限制 { #operational-and-qualification-limits }

管理仍为 HTTP；Modbus 无认证/加密，MQTT 依赖代理授权。通道掩码不是框架通用权限。未建立闪存加密或固件签名保证；源码中的 OTA 跳过证书选项实际未启用。

RTU 有 UART 空闲和 CRC，但不严格拒绝每个超过 1.5 字符的帧内间隔。生成 KNX 包不证明 ETS 导入、完整/部分下载、设备注册或认证。启动电气行为、寿命、RS485 转向、实机 TLS 拒绝、负载资源和长期恢复都需按部署要求验证。

部分依赖仍使用版本范围；嵌入新鲜度与仓库内二进制意味着不能假设旧产物匹配后续源码，须重建并哈希。

## 文档改进 { #documentation-defects-corrected-by-this-refresh }

当前指南以执行器而非上游演示为核心，更新 0.6.4 URL/镜像说明、Svelte 5 组件用法，并提供三种语言。严格构建、翻译覆盖与链接检查配合现有 `dev` Pages 工作流。历史资料保留原始日期及验证范围，不宣称本次重跑，见[验证记录](validation.md)。
