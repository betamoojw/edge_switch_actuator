# 当前版本与文档基线

审查日期：**2026 年 10 月 11 日**；`dev` 提交：[`bf8cb12`](https://github.com/betamoojw/edge_switch_actuator/tree/bf8cb12ab9ada4337751968332c7d5e17e056ccc)。`platformio.ini` 中的 `APP_VERSION` 为 **0.6.4**。这里描述源码变化，不表示现场设备已运行该镜像。

## 固件 0.6.4 { #firmware-064 }

- GitHub 仓库标识改为 `betamoojw/edge_switch_actuator`。去掉 `/tree/dev` 后，更新界面可构造正确的发布 API 地址。
- 侧栏用 **Project website** 替换 Discord 链接，指向本 GitHub Pages 文档。
- 已更新嵌入式界面和仓库内的默认板型固件文件。参见[镜像选择](buildprocess.md#firmware-build-and-release-artifacts)。
- `buildRelease/Edge_S3_Relay_6CH.knxprod` 已纳入版本管理。文件存在不代表 ETS 验收或 KNX 认证通过；常规固件打包钩子不会自动生成它。

与此前文档基线 `ced3e6d` 相比，后端 REST 路由、继电器策略、持久化配置结构以及 Modbus、KNX、MQTT、MCP 的行为均未改变。

## 已知限制与证据 { #remaining-limits-and-evidence }

发布选择器仍按 `.bin` 和板型名称子串匹配文件，可能选中合并镜像或相近板型。请手动选择完全匹配的 `_ota.bin`。修复 URL 不代表文件选择已安全，也不能证明实机升级成功。

`media/live/` 英语界面截图拍摄于 **2026 年 10 月 10 日，版本 0.6.3**，侧栏与 0.6.4 不同。用户提供的 KNX 图片未确认固件版本。本次文档工作没有操作继电器、指示灯、重启、复位、升级或修改配置。

[当前源码审查](actuator-source-review.md)列出尚未解决的问题；[历史验证记录](validation.md)保留当时的日期和结果，不应视为本次 0.6.4 验证。

## 文档验证：2026 年 10 月 11 日 { #documentation-validation }

**32 篇当前指南**均提供英语、简体中文和繁体中文，共 96 个语言页面。历史资料保留原文并明确标注。两种中文版本已检查地区术语和技术含义；这是智能体审阅，并非独立人工翻译认证。

- 严格 MkDocs 构建通过。离线检查覆盖 166 个 HTML 页面的当前译文、内部链接与锚点、图片替代文字、语言选择器及搜索索引内容。
- 浏览器在 1440 × 1000 和 390 × 844 视口检查桌面/移动排版、导航、对应页面切换、已翻译图表和图片。搜索 `relay`、`继电器`、`繼電器` 均有结果。搜索共用多语言索引，技术词可能匹配多个语言版本。
- 已检查现有设备及 KNX 截图的隐私信息，并遮蔽 KNX 图片中的私有端点。文本示例使用文档占位值；出厂 AP 地址保留真实默认值。
- 本次未新建设备会话，未执行继电器测试、ETS 下载、固件构建、OTA 更新或硬件资格验证。0.6.3 截图和版本未知的 KNX 图片仍有各自的证据限制。

Pages 工作流在部署前重新执行严格构建与离线检查。各语言目录下的 `build-info.json` 标识已部署的文档提交。
