# 編譯與韌體更新

## 工具鏈與目標 { #toolchain-and-targets }

使用 Python 3.11+、Node.js 24，安裝 `requirements-dev.txt`，於 `interface/` 執行 `npm ci`。完整程序見[入門](gettingstarted.md)。`platformio.ini` 固定 pioarduino `55.03.312-1`，採 Arduino，核心置於 `.pio/network-platformio`；目前 `APP_VERSION` 為 **0.6.4**。

| 環境 | 用途 |
| --- | --- |
| `waveshare-relay-6ch` | 預設 ESP32-S3 六路致動器，8 MB 分割表 |
| `waveshare-relay-6ch-mcp-off` | 不含 MCP 的編譯回歸 |
| `waveshare-relay-6ch-mcp-no-mqtt` | 確認 MCP 可獨立於 MQTT |
| `esp32-s3-devkitc-1`、`esp32-c3-devkitm-1`、`esp32dev` | 一般框架/燈光示範 |
| `Kincony-B16M`、`esp32-wt32-eth01` | 啟用乙太網路的框架板型 |

KNX 固定於 `980c047ad7fc5e27bf2fae95e48acde5d5e0b4fd`，WebSockets 為 2.7.2。部分其他相依套件仍採版本範圍，尚非完整可重現鎖定；前端有 `package-lock.json`。

## 功能選項 { #selecting-features }

生效旗標結合 `features.ini`、共用參數及板型覆寫。已編入安全、MQTT、NTP、手動/下載 OTA、遙測、核心傾印與 MCP；睡眠、電池停用，預設板型未啟用乙太網路。MQTT/MCP 在執行時仍預設關閉。

MCP 需安全及 NTP，禁止 `SERVE_CONFIG_FILES`。認證資訊不可寫進原始碼或編譯旗標。致動器以 `SetupIdentity` 專屬身份取代範本密碼，見[認證資訊](device-credentials.md)。

## 編譯流程 { #build-flow }

```sh
pio run -e waveshare-relay-6ch
python scripts/check_firmware_size.py waveshare-relay-6ch
```

1. `scripts/build_interface.py` 依變更檢查建置並壓縮 UI；預設 `EMBED_WWW` 輸出 `lib/framework/WWWData.h`。
2. `scripts/generate_cert_bundle.py` 準備 Adafruit CA 憑證包。
3. 產生 `.pio/build/<environment>/firmware.bin` 與 `firmware.elf`。
4. 掛鉤分別將符號、合併映像、OTA 存至 `build/elf/`、`build/merged/`、`build/release/`。
5. `scripts/package_release.py` 封裝本次輸出至 `buildRelease/`，一般增量編譯也執行；檔案系統專用、clean、erase 目標不封裝。

UI 變更偵測僅掃描 `interface/src/`。只改靜態資源、相依套件或 Vite 設定可能不重建；遇此情況，先刪除產生的 `lib/framework/WWWData.h` 再編譯。大小檢查以實際映像與應用分割區比較，不能只信連結器估算；使用 LTO 的 `esp32dev`、`esp32-wt32-eth01` 特別需要確認。

## 韌體輸出檔案 { #firmware-build-and-release-artifacts }

| `buildRelease/` 檔案 | 用途 |
| --- | --- |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4_ota.bin` | 僅含應用程式的 OTA 映像 |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4_ota.md5` | 十六進位 MD5，可於更新介面先上傳 |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4_webflash.bin` | 開機載入器/分割表/啟動應用/韌體合併映像，首次燒錄偏移 `0x0` |
| `edge_switch_actuator_waveshare-relay-6ch_0.6.4.elf` | 對應除錯符號 |
| `Edge_S3_Relay_6CH.knxprod` | 已納入版本管理的 KNX 套件，須另行產生與驗證 |

名稱由有效 `APP_VERSION` 與環境組成。同版本同環境重新編譯會取代檔案，其他版本保留；先暫存產出再替換，合併失敗會中止建置。此目錄受 Git 追蹤，沒有忽略。一般韌體封裝掛鉤不會自動產生 KNX 套件。

**OTA 請用 `_ota.bin`，不可用 `_webflash.bin`。** `EMBED_WWW` 讓韌體與 UI 一併更新，不刻意覆寫檔案系統設定。停用後 UI 改由 LittleFS 提供，`buildfs`/`uploadfs` 為獨立作業，發行包不含該映像；上傳檔案系統可能取代設定，需預先備存。

## 更新裝置 { #updating-a-device }

管理員在 **System → Firmware Update** 上傳相符檔案，確認板型並保留 ELF。MD5 只能偵測意外毀損，不是韌體簽章；建置程序未建立簽章真實性保證。

0.6.4 已將 `page.data.github` 改為 `betamoojw/edge_switch_actuator`，移除 `/tree/dev`，並新增文件側欄連結。這些是原始碼核對結果，不是實機 OTA 測試。

檔案挑選仍依 `.bin` 與板型子字串比對，合併映像或 MCP 變體也可能符合。請優先手動使用精確 `_ota.bin`，避免把含混的資產發佈給目前選擇器。韌體 CI 保留產物 14 天，不自動建立 tag 或 GitHub Release；發佈文件不會燒錄裝置或發行韌體。

## 憑證與原廠設定 { #certificates-and-factory-settings }

`board_ssl_cert_source = adafruit` 與 `src/certs/x509_crt_bundle.bin` 設定信任包。MCP 檢查 WSS 主機名及憑證，並等待合理時間。此板型的 `DOWNLOAD_OTA_SKIP_CERT_VERIFY` 已註解，正常更新不應啟用略過驗證。

`factory_settings.ini` 僅為未設定項目提供預設值，不會遷移既有裝置。目前 `Europe/Berlin` 標籤搭配 UK 風格 POSIX 時區，請在 **Connections → NTP** 選取一致設定。KNX 產生程序見[套件說明](https://github.com/betamoojw/edge_switch_actuator/blob/dev/knx/README.md)，網站維護見[文件發佈](documentation.md)。
