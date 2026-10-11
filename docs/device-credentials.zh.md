# 设备凭据与二维码设置标签

`scripts/get_device_credentials.py` 从 ESP32-S3 原生 USB 读取固件输出的设置密码，可选验证网页登录并通过认证 API 读取已保存 AP 凭据。它不会刷写、擦除、操作继电器、猜密码，也不能从哈希恢复自定义网页密码。

## 从 USB 设备生成标签 { #quick-start-usb-device-to-printable-label }

在仓库根目录使用 Python 3.10+，先关闭其他串口监视器：

```powershell
python -m pip install -r scripts/requirements-credentials.txt
python scripts/get_device_credentials.py --list-ports
```

从 JSON 中取本机 `port` 和 `usb_serial`，替换以下占位符。**`--reset` 会重启设备**，中断控制并应用启动策略，须先获授权：

```powershell
python scripts/get_device_credentials.py `
  --port <PORT> `
  --expect-mac <DEVICE_MAC> `
  --reset `
  --label-png .pio/labels/device-setup.png `
  --label-dpi 300
```

成功时退出码 0，输出含 `label.path`、`label.dpi: 300`、`label.contains_secrets: true`。终端仍遮盖密码，但 **PNG 含真实密码**。只有确实需要私下显示时才加 `--show-secrets`；禁止将输出写进公共日志。已有文件拒绝覆盖，每次选新文件名。

打印用 **100 × 70 mm** 标签纸，选择 **100%/实际尺寸**，禁用适应页面。DPI 支持 203、300、600，应匹配打印机；生成 PNG 不会提交打印任务。AP 工作时扫码加入 Wi-Fi，再打开 `http://192.168.4.1`，用 `admin` 和标签上的共享出厂 Wi-Fi/网页密码登录。已修改设置可能不同，标签必须私下保管。

## 常用选项与排错 { #common-commands }

| 需求 | 参数或处理 |
| --- | --- |
| 枚举设备 | `--list-ports` |
| 捕获并显示密码 | `--port <PORT> --expect-mac <DEVICE_MAC> --reset --show-secrets` |
| 等待手动重启 | 不传 `--reset`，可加 `--timeout 120` |
| 核对当前网页登录/AP | 加 `--device-url http://device.example`，明确指定目标来源 |
| 使用已知自定义密码 | 加 `--username admin --ask-password`，隐藏输入 |
| 查看参数 | `--help` |
| 端口不可用/身份不匹配 | 重新枚举，不能只按复用 COM 编号选择 |
| 没有设置密码行 | 关闭其他工具，经授权重启并延长超时 |
| PNG 依赖缺失 | 用同一个 Python 重装 `scripts/requirements-credentials.txt` |
| 输出文件存在 | 改用新 `.png` 文件名 |
| 退出码 3 | 出厂捕获成功、API 核对失败，检查 URL/网络/账号，已生成标签仍有效 |
| 能扫码但不能连接 | 确认 AP 广播及保存设置仍与出厂值相符 |
| 纸质二维码无法识别 | 核对 DPI、实际尺寸、白边、打印机及耗材 |

仓库已有 PlatformIO Python 时，可将 `python` 替换为 `& .pio/network-platformio/penv/Scripts/python.exe`。

## 安装与身份确认 { #install-and-identify }

捕获必须同时指定 `--port`、`--expect-mac`。原生 USB 须报告 VID/PID `303A:1001` 和匹配的 MAC 序列号；没有该身份的 USB-UART 转接器不支持。USB 重新枚举换端口后，应重新列出并核对。

## 读取出厂设置凭据 { #read-factory-setup-credentials }

`--reset` 先运行 esptool 只读 `flash-id`，再正常应用复位，不写闪存；没有该选项时仅开串口等待，默认 60 秒。即使预先撤销 DTR/RTS，部分驱动在打开/关闭串口时仍可能触发复位。

固件必须输出 `Device setup password (admin and factory AP): …`。捕获有界且可处理分片，不打印或保存原始串口日志。`factory_setup` 包含捕获密码、仓库默认 AP 名称/地址和管理员名，它们是**设置默认值**，不是当前保存值证明。NVS 设置身份在出厂复位后保留，普通固件上传保留已修改账号及 AP 设置。

默认遮盖密码，只有 `--show-secrets` 才明文输出；标签仅在显式 `--label-png` 时写入。脚本不输出认证令牌，也不在代码或测试中内置真实密码。

## 可打印 PNG { #print-ready-png-setup-labels }

标签总是包含真实出厂密码，与终端遮盖无关。拒绝覆盖已有文件和符号链接，按需建目录；POSIX 使用 0600，Windows 继承目标目录 ACL。`.pio/` 被 Git 忽略，但仍须控制本地访问。

黑白图默认 300 DPI、1181 × 827 像素，物理尺寸 100 × 70 mm，也可 203/600 DPI。图中有型号、MAC、SSID、共享出厂密码、账号和设置 URL，标明默认值，不用当前自定义密码替换标签内容。二维码仅按 `WIFI:T:WPA;S:…;P:…;;` 编码 AP SSID/密码，不包含网页登录令牌。

### 标准与打印质量 { #standards-and-print-quality }

采用 [ZXing Wi-Fi 内容约定](https://github.com/zxing/zxing/wiki/Barcode-Contents)及特殊字符转义，`qrcode` 生成 Model 2、Q 纠错、黑白方形模块和四模块留白，遵循 [DENSO WAVE 区域指南](https://www.qrcode.com/en/howto/code.html)。模块使用整数像素、不重采样，最小 0.33 mm 且至少三像素。

这些选择不是 ISO/IEC 15415 印刷认证、GS1 身份、Matter 或 Wi-Fi Easy Connect/DPP 支持。没有提供 GTIN，不虚构 [GS1 Digital Link](https://www.gs1.org/standards/gs1-digital-link) 标识。二维码用于配网，不是产品追溯身份；量产前应验证打印机、耗材、对比度、耐久和扫描器，PNG 解码不能证明印刷质量。

## 核对当前网页登录和 AP { #verify-the-current-web-login-and-ap-settings }

通过 `--device-url` 明确给定目标来源，示例域名须替换。工具最多等待 `/rest/features` 30 秒（`--network-wait` 可改），确认 Waveshare 目标后，仅尝试一次捕获密码登录。若 API 提供 STA MAC 则核对，再读 `/rest/apSettings`；不匹配即停止。AP-only 时可能无 MAC，`current.identity` 会注明无法验证网络身份。报告 AP 设置不证明 AP 正在广播。

固件通常为 HTTP，密码和令牌未加密；若设备支持 HTTPS 则使用 HTTPS，证书验证保持启用。工具禁用重定向和环境代理，不自动跟随串口中的地址，也不改变电脑 Wi-Fi。

自定义密码无法从 PBKDF2 哈希恢复；用 `--ask-password` 隐藏输入已知密码。仅登录和 AP 读取成功才填 `current`。401 为登录拒绝，403 可能是无管理员权限；失败保留出厂结果，不输出原始响应，也不自动恢复出厂。

## 退出码与验证 { #exit-codes-and-validation }

| 代码 | 含义 |
| --- | --- |
| 0 | 完成请求；未给 URL 时只验证设置密码捕获 |
| 1 | USB、依赖、I/O 或捕获失败 |
| 2 | 参数错误 |
| 3 | 出厂捕获成功，但 API 核对失败 |
| 130 | 用户取消或密码输入 EOF |

```powershell
python -m unittest discover -s tests -p test_device_credentials.py -v
python -m pip install zxing-cpp==2.3.0
```

独立二维码解码测试需额外 `zxing-cpp`。测试覆盖串口分片/坏输入、内存边界、身份错配、期限/清理、重启显式选择、脱敏、部分失败、网络身份、畸形/过大响应、重定向、尺寸/DPI、三种分辨率解码及防覆盖。单元测试不需设备或网络。历史上曾手动读取一台设备，但脚本完整重启捕获流程仍需单独硬件验收，不能由单元测试推断。
