# 裝置認證資訊與 QR 設定標籤

`scripts/get_device_credentials.py` 從 ESP32-S3 原生 USB 讀取韌體輸出的設定密碼，亦可選擇核對網頁登入，再由已認證 API 取得儲存的 AP 資訊。它不會燒錄、清除設定、操作繼電器、猜測密碼，也無法從雜湊還原自訂網頁密碼。

## 從 USB 裝置製作標籤 { #quick-start-usb-device-to-printable-label }

於儲存庫根目錄使用 Python 3.10+，先關閉其他序列監視器：

```powershell
python -m pip install -r scripts/requirements-credentials.txt
python scripts/get_device_credentials.py --list-ports
```

從 JSON 取得裝置的 `port`、`usb_serial`，取代下列預留值。**`--reset` 會重新啟動裝置**，中斷控制並套用開機策略，務必先取得授權：

```powershell
python scripts/get_device_credentials.py `
  --port <PORT> `
  --expect-mac <DEVICE_MAC> `
  --reset `
  --label-png .pio/labels/device-setup.png `
  --label-dpi 300
```

成功時結束碼為 0，JSON 包含 `label.path`、`label.dpi: 300`、`label.contains_secrets: true`。終端機仍遮蔽密碼，**PNG 卻含真實密碼**。只有需要私下檢視時才加 `--show-secrets`，不可將其存入共用紀錄。工具拒絕覆寫，每次匯出請用新檔名。

使用 **100 × 70 mm** 標籤紙，以 **100%/實際大小** 列印，停用符合頁面。DPI 可選 203、300、600，應配合印表機；產生 PNG 不會自動列印。AP 啟用時掃碼連線，再開啟 `http://192.168.4.1`，以 `admin` 及標籤的共用原廠 Wi-Fi/網頁密碼登入。已修改的設定可能不同，標籤須妥善保管。

## 常用選項與排解 { #common-commands }

| 工作 | 參數或處理方式 |
| --- | --- |
| 列出 USB 裝置 | `--list-ports` |
| 擷取並顯示密碼 | `--port <PORT> --expect-mac <DEVICE_MAC> --reset --show-secrets` |
| 等候手動重新啟動 | 不加 `--reset`，可用 `--timeout 120` |
| 核對現行登入/AP | 加 `--device-url http://device.example`，明確指定來源 |
| 使用已知自訂密碼 | `--username admin --ask-password`，採隱藏輸入 |
| 完整選項 | `--help` |
| 埠不可用或身份不符 | 重新列出裝置，勿只依重用的 COM 編號選擇 |
| 無設定密碼行 | 關閉其他程式，經核准重啟並延長逾時 |
| 缺少 PNG 套件 | 用同一 Python 重新安裝 `scripts/requirements-credentials.txt` |
| 標籤檔已存在 | 改用新 `.png` 檔名 |
| 結束碼 3 | 原廠擷取完成、API 核對失敗；查 URL/網路/帳號，已產生標籤仍保留 |
| QR 可讀但連不上 | 確認 AP 廣播及儲存設定是否仍為預設值 |
| 印出的 QR 無法讀取 | 核對 DPI、實際大小、留白、印表機及耗材 |

可將 `python` 改成儲存庫既有的 `& .pio/network-platformio/penv/Scripts/python.exe`。

## 安裝與辨識 { #install-and-identify }

擷取時 `--port`、`--expect-mac` 缺一不可。原生 USB 須有 VID/PID `303A:1001` 及相符 MAC 序號；不支援沒有此身份的 USB-UART 轉接器。重新枚舉導致埠改變時，須再次列出並核對。

## 讀取原廠設定資訊 { #read-factory-setup-credentials }

`--reset` 先執行 esptool 唯讀 `flash-id`，再回到應用重啟，不寫入 Flash。未指定時，只開啟序列埠等待，預設 60 秒。即使先解除 DTR/RTS，部分作業系統或驅動仍可能於開關埠時觸發重設。

韌體須輸出 `Device setup password (admin and factory AP): …`。擷取有界、支援分段，不輸出或儲存原始序列紀錄。`factory_setup` 含擷取密碼、儲存庫預設 AP 名稱/位址與管理員名稱；這些是**設定預設值**，不是目前已儲存值的證明。NVS 設定身份跨原廠重設保留，正常韌體上傳也保留修改過的帳號及 AP。

預設遮蔽密碼，`--show-secrets` 才輸出明文；只有明確提供 `--label-png` 才儲存含密碼圖片。工具不輸出認證權杖，程式與測試也不內含真實密碼。

## 可列印 PNG 標籤 { #print-ready-png-setup-labels }

標籤一定包含真實原廠密碼，與終端遮蔽無關。既有檔案及符號連結皆拒絕覆寫，父目錄視需要建立。POSIX 採 0600，Windows 繼承目的目錄 ACL。`.pio/` 雖被 Git 忽略，仍須管制本機存取。

黑白標籤為 100 × 70 mm，預設 300 DPI、1181 × 827 像素，也支援 203/600 DPI。列出型號、MAC、SSID、原廠共用密碼、帳號及設定 URL，清楚註明原廠值；不會換入已核對的自訂帳號密碼。QR 僅使用 `WIFI:T:WPA;S:…;P:…;;` 編碼 AP SSID/密碼，不含網頁權杖。

### 標準與列印品質 { #standards-and-print-quality }

依 [ZXing Wi-Fi 內容慣例](https://github.com/zxing/zxing/wiki/Barcode-Contents)處理跳脫字元，以 `qrcode` 產生 Model 2、Q 錯誤更正、黑白方形模組與四模組淨空，遵循 [DENSO WAVE 區域說明](https://www.qrcode.com/en/howto/code.html)。每模組採整數像素、不重新取樣，最小 0.33 mm 且至少三像素。

這些設計不等於 ISO/IEC 15415 印刷認證、GS1 身份、Matter 認證或 Wi-Fi Easy Connect/DPP。未提供 GTIN，因此不虛構 [GS1 Digital Link](https://www.gs1.org/standards/gs1-digital-link) 識別。此 QR 供網路設定，不是追溯標識。正式導入前須評估印表機、標籤紙、對比、耐用性與掃描器；PNG 解碼測試不能證明印刷品質。

## 核對目前登入與 AP 設定 { #verify-the-current-web-login-and-ap-settings }

以 `--device-url` 指定來源，範例網域須換成實際裝置。工具等候 `/rest/features` 最長 30 秒（可改 `--network-wait`），確認 Waveshare 目標後僅嘗試一次設定密碼登入。API 若提供 STA MAC 則先核對，再讀 `/rest/apSettings`，不符即停止。僅 AP 模式可能沒有 MAC，`current.identity` 會明示無法驗證網路身份。讀到 AP 設定不代表 AP 正在廣播。

韌體通常使用 HTTP，密碼與權杖並未加密；若支援 HTTPS 可改用，但憑證驗證保持啟用。工具拒絕重新導向及環境代理，不自動跟隨序列輸出的網址，也不切換電腦 Wi-Fi。

自訂密碼無法從 PBKDF2 雜湊還原。以 `--ask-password` 隱藏輸入已知密碼，只有登入及 AP 讀取成功才填入 `current`。401 為登入遭拒，403 可能缺管理員權限；失敗仍保留原廠擷取結果，不顯示原始伺服器回應，也不自動原廠重設。

## 結束碼及驗證 { #exit-codes-and-validation }

| 代碼 | 定義 |
| --- | --- |
| 0 | 作業完成，未給 URL 時只證明設定密碼擷取 |
| 1 | USB、相依套件、I/O 或擷取失敗 |
| 2 | 命令列參數無效 |
| 3 | 原廠擷取成功，但 API 核對失敗 |
| 130 | 使用者取消或密碼提示收到 EOF |

```powershell
python -m unittest discover -s tests -p test_device_credentials.py -v
python -m pip install zxing-cpp==2.3.0
```

獨立 QR 解碼測試另需 `zxing-cpp`。測試涵蓋序列分段/異常內容、記憶體界限、身份不符、清理/期限、明確重啟選項、遮蔽、部分失敗、網路身份、異常/過大回應、重新導向、尺寸/DPI、三種解析度解碼及防覆寫。單元測試不需實機或網路。雖曾手動讀取一部開發裝置，腳本完整重啟與擷取流程仍須另做實機驗收，不能由單元測試推定。
