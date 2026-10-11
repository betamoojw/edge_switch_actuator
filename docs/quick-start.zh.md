# 首次使用

本指南适用于已安装 Edge Switching Actuator 的 Waveshare 六路板。尚未安装固件时，请先[构建并刷写](gettingstarted.md)。安装和验收完成之前，应隔离实际负载。

## 连接前的准备 { #before-you-connect }

准备 `waveshare-relay-6ch` 固件、适配电源、浏览器、2.4 GHz Wi-Fi 和本机专属设置凭据。向网络管理员确认目标网络，阅读[硬件接线说明](esp32-s3-relay-6ch-hardware.md)，记录板卡版本和镜像标识。

已调试的设备应使用分配的地址和账号。不要为了照着教程操作而恢复出厂设置；这会删除配置。

## 连接与登录 { #connect-and-sign-in }

1. 通过支持的接口供电。从私有设置标签或 115200 波特率串口获取 **Device setup password**，不要将标签或串口截图公开。
2. 新设备没有上行网络时，使用该密码连接 `ESP32-SvelteKit-<unique_id>`，打开 `http://192.168.4.1`。
3. 以 `admin` 和本机设置密码登录。已有账号沿用保存的密码；执行器没有统一出厂密码。
4. 在 **WiFi → WiFi Station** 中填写目标网络并应用。这会改变网络连接；随后使用路由器分配的 STA 地址重新访问。可从路由器客户端列表或本机网络状态页确认地址。
5. 打开 **Switching Actuator**。应看到六个通道、网络信息、协议状态和运行时间。源码默认值为总线 Off、所有输出 OFF；已有配置可能不同。

以上是调试步骤，并非本次[只读检查](live-verification.md)执行过的操作。

## 发出命令前先读懂状态 { #read-the-dashboard-before-commanding-anything }

参阅[界面导览](interface-tour.md)。**OFF** 仅表示固件命令关闭，不证明电路已隔离或触点断开。通道下的 `startup` 是最后命令来源，不代表启动策略设置为 ON。

检查名称、启用状态、启动策略、脉冲时长、断连关闭策略和按键绑定。KNX 调试完成后，以 KNX 应用参数作为继电器配置依据。向安装人员确认账号角色和可操作通道。

## 选择控制方式 { #choose-a-control-path }

可保持总线 Off，仅使用浏览器；也可选择 RTU、TCP 或 KNX/IP 中的一种。MQTT/Home Assistant 和小智 MCP 独立启用，见[集成方式](integrations.md)。新增集成意味着新增命令来源，应与现场操作负责人协调。

## 首次受控输出测试 { #first-controlled-output-test }

仅在获得现场负责人授权且已准备隔离低压夹具后执行；文档检查没有运行该测试。

1. 确认一个启用且未被锁定的通道和允许使用的测试负载，按测试计划暂停其他自动化命令来源。
2. 记录初始状态及脉冲时长。
3. 对该通道执行 **Turn ON**，检查 ON 标记，并独立测量夹具或触点响应。
4. 执行 **Turn OFF**，确认画面与夹具后，再执行一次 **Pulse**；应先 ON，再在设置时长后 OFF。
5. 记录结果并恢复约定状态。遇到异常立即停止，不要用全部开启来测试连通性。

[调试验收](commissioning.md)涵盖重启、总线、故障和恢复。交付前为操作人员分配合适账号，私下保存配置及通道—负载对应关系。异常时查阅[故障排查](troubleshooting.md)。
