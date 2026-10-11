# Modbus 驗證工具快速入門

`scripts/verify_modbus.py` 提供可重現的 TCP/RTU 冒煙檢查。預設不要求改變繼電器，但會送出非法 FC05 值確認例外，**不屬於純唯讀流量**。本次文件作業未對裝置執行測試；配線、切換模式及主動測試須先授權。時序、異常訊框、並行、耐久與實體驗證見[歷史完整計畫](MODBUS_VERIFICATION_VALIDATION_PLAN.md)。

## 1. 準備工作 { #1-start-safely }

隔離市電負載，以安全低壓治具測試並確認接點初態。透過網頁選擇核准的 RTU/TCP，記錄站號及參數。RTU 須啟用 RS485，完成 A/B、必要參考地及終端匹配。以下皆在儲存庫根目錄執行。

## 2. 工具自我檢查 { #2-check-the-local-tool }

```powershell
py scripts/verify_modbus.py --self-test
py scripts/verify_modbus.py --help
```

不用實機，預期顯示 `Modbus verifier self-test passed`。

## 3. TCP 檢查 { #3-modbus-tcp-quick-start }

確認 IPv4、TCP running、來源規則允許測試主機且連接埠可達。`192.0.2.10` 是文件示範保留位址，必須換為核准目標：

```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.0.2.10 --port 502 --unit 1 `
  --output evidence/modbus-tcp-smoke.json
```

預設埠 502、unit 1、逾時兩秒，`--timeout 5` 可延長。另可用 `Test-NetConnection 192.0.2.10 -Port 502` 查連線，同樣須替換位址。

## 4. RTU 檢查 { #4-modbus-rtu-quick-start }

```powershell
py -m pip install pyserial
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name
py scripts/verify_modbus.py --transport rtu `
  --serial-port COM4 --unit 1 --baud 19200 --parity E --stop-bits 1 `
  --output evidence/modbus-rtu-smoke.json
```

先關閉占用轉接器的軟體，COM4 只是例子。站號、鮑率、同位檢查與停止位須完全相符。`--parity` 為 E/O/N，停止位為 1/2，原廠預設 19200 8E1。

## 5. 選用主動測試 { #5-optional-relay-test }

原命令加上 `--write-channel 1` 可測試單一路，範圍 1–6。工具先讀初態，切至相反狀態，查 FC01/FC02，再於 `finally` 嘗試還原。另行量測接點及復原結果；軟體還原無法取代隔離或緊急斷電。該路須啟用、未鎖定且遮罩允許。

### 全通道 FAT/SAT { #all-channel-fatsat-sequence }

先滿足 [TCP](modbus-tcp-relay-fat-sat.md) 或 [RTU](modbus-rtu-relay-fat-sat.md) 的完整條件，再於傳輸參數後加：

```text
--exercise-all-relays --toggle-cycles 3
--toggle-on-seconds 1 --toggle-off-seconds 1
--on-seconds 30 --off-seconds 5 --confirm-safe-loads
```

每路預設三輪 1 秒 ON/1 秒 OFF，再維持 ON 30 秒、OFF 5 秒。每步讀取六路，首次失敗停止，盡力將測試通道 OFF。`--confirm-safe-loads` 只是操作者聲明，不是硬體互鎖；JSON 也不是接點量測。

RTU 另檢查 FC08、序列組態及前後訊框錯誤計數。只有明確核准關閉的初始 ON 通道，才逐一加 `--prepare-off-channel N`。全 OFF 時不需要；未授權 ON、讀取失敗或 FC01/FC02 不符皆中止，不自動修正未知狀態。

### RGB 與實體按鍵 { #rgb-indicator-and-physical-button-test }

前提為 RGB/按鍵啟用、雙擊 identify、亮度非零、全輸出 OFF、有觀察者，且安裝權限 **Allow manual indicators and diagnostic commands** 已開。於傳輸參數後加：

```text
--exercise-device-io --confirm-indicator-observation
--button-timeout-seconds 60
```

工具透過 FC16 信箱啟動識別，再等一次實體短雙擊，檢查計數、手勢及 RGB 暫存器；不改指派也不開繼電器。不可長按 BOOT 或測其他點擊。觀察者需另記白光是否可見，確認旗標不是光感測器。若權限遭拒，從認證網頁核准後再測，不可繞過規則。

## 6. 判讀報告 { #6-understand-the-report }

終端及 `--output` 使用相同 JSON，`checks` 每筆有 `name`、`passed`、`detail`。一般檢查包括 FC43/14 身份、映射/能力/狀態、六路线圈與輸出、傳輸組態、未映射位址例外 02、非法 FC05 例外 03；RTU 加 FC08。只有指定對應選項才執行輸出或 I/O。

PowerShell 以 `$LASTEXITCODE` 判讀：0 全部要求檢查通過，1 裝置檢查或執行錯誤，2 參數/相依套件/目標無效。連同韌體雜湊、組態、板卡身份及日期留存，失敗報告不要覆寫。

## 7. 常見失敗 { #7-common-failures }

- TCP 拒絕/逾時：確認 running、IP/埠、精確來源限制、選定上行、防火牆與埠衝突。
- RTU 逾時：核對模式、RS485 初始化、序列參數、A/B 定義、終端/參考地、驅動、指示燈與埠占用；更動配線前先隔離。
- 例外 02：未映射或不可存取、停用、鎖定、策略拒絕，依[暫存器表](actuator-modbus-map.md)確認。
- 例外 03：值、數量或編碼錯誤，非法 FC05 測試刻意期待此結果。
- 身份/映射版本不符：先保留報告，查驗目標與候選映像，勿立即變更設定。

## 8. 最低複查項目 { #8-minimum-revisit-checklist }

留下韌體與板卡身份；確認模式和參數；工具自測通過；冒煙檢查退出 0 且已存證；主動測試採隔離負載並復原；另用獨立主站重查核心讀寫；完成完整計畫剩餘 P0 項目。沒有量測的物理項目須列為未完成。
