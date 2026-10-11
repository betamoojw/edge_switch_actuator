# 開發入門

`platformio.ini` 預設選用產品目標 `waveshare-relay-6ch`。配線或燒錄前請讀[板卡參考](esp32-s3-relay-6ch-hardware.md)；通用 ESP32 環境編譯的是框架範本，並非六路致動器。

## 1. 取得原始碼 { #1-prepare-a-checkout }

需備妥 Git、Python 3.11+、Node.js 24 與 npm。可選用 VS Code 的 PlatformIO/Svelte 擴充套件。

```sh
git clone --branch dev https://github.com/betamoojw/edge_switch_actuator.git
cd edge_switch_actuator
python -m venv .venv
```

PowerShell 執行 `.venv\Scripts\Activate.ps1`，macOS/Linux 執行 `source .venv/bin/activate` 啟用環境，再安裝：

```sh
python -m pip install -r requirements-dev.txt
cd interface
npm ci
cd ..
```

PlatformIO 將工具鏈放在 `.pio/network-platformio`。初次編譯須連網下載相依項目並準備憑證包。

## 2. 不接硬體試用介面 { #2-try-the-interface-without-hardware }

```sh
cd interface
npm run dev:sim
```

開啟終端機列出的本機網址，以 `admin` / `sim-admin` 登入。這是本機測試資料，並非實機密碼。模擬器不會接觸繼電器、MQTT、KNX 或小智。其他角色、測試組態與裝置代理模式見[前端測試](frontend-testing.md)。

## 3. 編譯與燒錄 { #3-build-and-flash-a-unit }

於儲存庫根目錄執行：

```sh
pio run -e waveshare-relay-6ch
python scripts/check_firmware_size.py waveshare-relay-6ch
```

壓縮介面會內嵌至韌體，版本化檔案輸出至 `buildRelease/`。以下會燒錄或開啟序列埠，僅在核准的維護時段執行：

```sh
pio run -e waveshare-relay-6ch -t upload
pio device monitor -b 115200
```

有多部序列裝置時，使用 `--upload-port <port>`。板型採 8 MB 分割配置，使用前先查驗實際 Flash。映像種類及 OTA 見[編譯與更新](buildprocess.md)。

## 4. 設定 Wi-Fi 並登入 { #4-provision-wi-fi-and-sign-in }

1. 從 115200 baud 本機序列主控台或既有私有標籤取得 **Device setup password**。24 字元專屬密碼儲存在 NVS，原廠重設後仍保留。
2. 無已設定上行時，以該密碼加入 `ESP32-SvelteKit-<unique_id>`。
3. 開啟 `http://192.168.4.1`，使用 `admin` 與設定密碼；既有帳號維持儲存密碼。
4. 設定 **WiFi → WiFi Station**，再改以 STA 位址連線。
5. 接上現場負載前，核對 **Users**、輸出設定與通訊協定。

致動器不使用範本的 `admin/admin`、`guest/guest` 或 `esp-sveltekit` 作為原廠認證資訊，詳見[認證指南](device-credentials.md)。

## 5. 選擇整合項目 { #5-select-integrations }

在 **Switching Actuator → Protocol Interface** 擇一啟用現場總線。RTU 需啟用 RS485；TCP/KNX 會等待 IPv4 上行。預設所有輸出 OFF、總線 Off。MQTT 探索及小智 MCP 於 **Connections** 各自啟用。接著參閱[操作](device-operation.md)、[Modbus](actuator-modbus-map.md)或 [KNX](knx-address-entry.md)。
