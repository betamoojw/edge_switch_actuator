# 开发入门

`platformio.ini` 默认选择产品目标 `waveshare-relay-6ch`。接线或刷写前阅读[板卡参考](esp32-s3-relay-6ch-hardware.md)；通用 ESP32 环境构建的是模板应用，而不是六路执行器。

## 1. 准备源码 { #1-prepare-a-checkout }

需要 Git、Python 3.11+、Node.js 24 和 npm。VS Code 配合 PlatformIO/Svelte 扩展可选。

```sh
git clone --branch dev https://github.com/betamoojw/edge_switch_actuator.git
cd edge_switch_actuator
python -m venv .venv
```

PowerShell 使用 `.venv\Scripts\Activate.ps1`，macOS/Linux 使用 `source .venv/bin/activate` 激活环境，然后安装：

```sh
python -m pip install -r requirements-dev.txt
cd interface
npm ci
cd ..
```

PlatformIO 工具链保存在 `.pio/network-platformio`。首次构建需要联网下载依赖并准备证书包。

## 2. 无硬件体验界面 { #2-try-the-interface-without-hardware }

```sh
cd interface
npm run dev:sim
```

打开终端输出的本地地址，使用 `admin` / `sim-admin` 登录。这只是本地测试账号，不能用于真实设备。模拟器不连接继电器、MQTT、KNX 或小智。其他角色、测试配置和设备代理见[前端测试](frontend-testing.md)。

## 3. 构建与刷写 { #3-build-and-flash-a-unit }

在仓库根目录运行：

```sh
pio run -e waveshare-relay-6ch
python scripts/check_firmware_size.py waveshare-relay-6ch
```

压缩界面嵌入固件，带版本文件输出至 `buildRelease/`。以下命令会刷写或打开串口，仅在获授权的维护窗口执行：

```sh
pio run -e waveshare-relay-6ch -t upload
pio device monitor -b 115200
```

多串口时指定 `--upload-port <port>`。板型使用 8 MB 分区布局，先核对实际闪存。镜像类型及 OTA 见[构建与更新](buildprocess.md)。

## 4. 配网与登录 { #4-provision-wi-fi-and-sign-in }

1. 从 115200 波特率本地串口或已有私有标签获取 **Device setup password**。24 字符随机密码保存在 NVS，恢复出厂后仍保留。
2. 没有已配置上行时，用该密码加入 `ESP32-SvelteKit-<unique_id>`。
3. 打开 `http://192.168.4.1`，以 `admin` 和设置密码登录；已有账号仍用保存密码。
4. 配置 **WiFi → WiFi Station**，之后使用 STA 地址。
5. 接实际负载前检查 **Users**、输出配置及协议。

执行器不使用模板的 `admin/admin`、`guest/guest` 或 `esp-sveltekit` 作为出厂凭据，见[凭据指南](device-credentials.md)。

## 5. 选择集成 { #5-select-integrations }

在 **Switching Actuator → Protocol Interface** 选择一种现场总线。RTU 需启用 RS485；TCP/KNX 等待 IPv4 上行。默认所有输出 OFF、总线 Off。MQTT 发现和小智 MCP 在 **Connections** 独立启用。继续阅读[操作](device-operation.md)、[Modbus](actuator-modbus-map.md)或 [KNX](knx-address-entry.md)。
