# 构建与固件更新

## 工具链与目标 { #toolchain-and-targets }

使用 Python 3.11+、Node.js 24，安装 `requirements-dev.txt`，在 `interface/` 执行 `npm ci`。[入门](gettingstarted.md)提供完整步骤。`platformio.ini` 固定 pioarduino `55.03.312-1`，框架为 Arduino，核心目录 `.pio/network-platformio`；`APP_VERSION` 当前为 **0.6.4**。

| 环境 | 用途 |
| --- | --- |
| `waveshare-relay-6ch` | 默认 ESP32-S3 六路执行器，8 MB 分区 |
| `waveshare-relay-6ch-mcp-off` | 排除 MCP 的编译回归 |
| `waveshare-relay-6ch-mcp-no-mqtt` | 验证 MCP 不依赖 MQTT |
| `esp32-s3-devkitc-1`、`esp32-c3-devkitm-1`、`esp32dev` | 通用框架/灯光演示 |
| `Kincony-B16M`、`esp32-wt32-eth01` | 启用以太网的框架板型 |

KNX 固定提交 `980c047ad7fc5e27bf2fae95e48acde5d5e0b4fd`，WebSockets 固定 2.7.2；其他部分依赖使用范围，固件依赖并非完整锁定。前端使用 `package-lock.json`。

## 功能选择 { #selecting-features }

有效标志由 `features.ini`、通用标志和板型覆盖共同决定。安全、MQTT、NTP、手动/下载 OTA、遥测、核心转储和 MCP 已编译；睡眠、电池关闭，默认板型无以太网。MQTT/MCP 运行时仍默认关闭。

MCP 需要安全和 NTP，禁止 `SERVE_CONFIG_FILES`。不要将凭据写入源码或编译参数。执行器以 `SetupIdentity` 的设备专属身份覆盖模板密码，见[设备凭据](device-credentials.md)。

## 构建流程 { #build-flow }

```sh
pio run -e waveshare-relay-6ch
python scripts/check_firmware_size.py waveshare-relay-6ch
```

1. `scripts/build_interface.py` 按变更检查构建并压缩 UI；默认 `EMBED_WWW` 写入 `lib/framework/WWWData.h`。
2. `scripts/generate_cert_bundle.py` 准备 Adafruit CA 包。
3. 编译生成 `.pio/build/<environment>/firmware.bin` 和 `firmware.elf`。
4. 钩子将符号、合并镜像、OTA 分别归档到 `build/elf/`、`build/merged/`、`build/release/`。
5. `scripts/package_release.py` 将本次输出打包至 `buildRelease/`，普通增量构建也会执行；仅文件系统、clean、erase 目标不打包。

UI 新鲜度检查只扫描 `interface/src/`。仅修改静态资源、依赖或 Vite 配置可能漏构建；此时删除生成的 `lib/framework/WWWData.h` 再构建。大小检查使用实际二进制与应用槽位，链接估算不能代替；启用 LTO 的 `esp32dev`、`esp32-wt32-eth01` 尤其要检查。

## 固件产物 { #firmware-build-and-release-artifacts }

| `buildRelease/` 文件 | 用途 |
| --- | --- |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4_ota.bin` | 仅应用的 OTA 镜像 |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4_ota.md5` | 十六进制 MD5，可在更新界面先上传 |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4_webflash.bin` | 启动器/分区/启动应用/固件合并镜像，首次刷写偏移 `0x0` |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4.elf` | 对应调试符号 |
| `Edge_S3_Relay_6CH.knxprod` | 已跟踪的 KNX 产品包，须单独生成与验证 |

名称取自实际 `APP_VERSION` 和环境；同名构建替换文件，其他版本保留，分阶段生成后替换，合并失败使构建失败。该目录被 Git 跟踪，并未忽略。固件打包钩子不自动生成 KNX 包。

**OTA 只用 `_ota.bin`，不能用 `_webflash.bin`。** `EMBED_WWW` 使固件与 UI 一起更新，不主动覆盖文件系统配置。取消后通过 LittleFS 提供 UI，`buildfs`/`uploadfs` 是独立操作，发布包不含文件系统镜像；上传文件系统可能替换设置，需先备份。

## 更新设备 { #updating-a-device }

管理员在 **System → Firmware Update** 上传匹配镜像，确认环境并保留 ELF。MD5 仅检测意外损坏，不是固件签名；构建流程不证明签名真实性。

0.6.4 将 `page.data.github` 修正为 `betamoojw/edge_switch_actuator`，去掉 `/tree/dev`，并将侧栏链接指向文档。这是源码验证，不是实机 OTA 测试。

文件选择仍按 `.bin` 与板型子串匹配，合并镜像和 MCP 变体也可能入选。优先手动选精确 `_ota.bin`，不要给当前选择器发布易混淆资产。固件 CI 保留产物 14 天，不自动创建 tag 或 GitHub Release；发布文档不会刷设备或发布固件版本。

## 证书与出厂设置 { #certificates-and-factory-settings }

`board_ssl_cert_source = adafruit` 和 `src/certs/x509_crt_bundle.bin` 配置信任包。MCP 验证 WSS 主机名/证书，并等待可信时间。`DOWNLOAD_OTA_SKIP_CERT_VERIFY` 在此板型中被注释，日常更新不应启用绕过。

`factory_settings.ini` 仅提供未设置时的默认值，不迁移已调试设备。当前 `Europe/Berlin` 名称与 UK 风格 POSIX 时区不一致，应在 **Connections → NTP** 选择匹配项。KNX 生成见[包说明](https://github.com/betamoojw/edge_switch_actuator/blob/dev/knx/README.md)，文档流程见[发布](documentation.md)。
