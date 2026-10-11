# Modbus TCP 繼電器 FAT/SAT 驗收程序

本程序用於可重現的工廠/現場功能驗收，**並非已完成測試的報告**。流程參照 IEC 62381 的 FAT/SAT/SIT 方法；功能與例外依 [Modbus Application Protocol V1.1b3](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf)，RTU 訊框與時序另依[序列指南 V1.02](https://www.modbus.org/file/secure/modbusoverserial.pdf)。時間與次數為專案條件，不是標準指定值。這不是 IEC/Modbus 認證、安全評估或電氣安裝核准，另見[暫存器契約](actuator-modbus-map.md)。

## 1. 範圍與限制

確認身份、映射、傳輸組態、全 OFF 初態、FC05 回送、FC01 線圈、FC02 已套用輸出、逐路隔離、短週期切換、ON/OFF 保持及最後復原。FC02 並非接點回授；實體驗收須另用額定合適的電表、隔離指示器或記錄器，記下儀器身份與結果。

一次只測一路，以隔離低壓負載操作，不接市電。電氣壽命、接點彈跳、溫升、絕緣、切換能力、EMC、功能/製程安全及完整協定符合性皆不在範圍內；異常訊框時序、廣播靜默與各鮑率另行驗證。

## 2. 人員、設備與先備條件

操作者須有負載送電權限，建議另一觀察者全程監控接點及緊急斷電。準備具生產代表性的板卡與已識別韌體、測試主機、六路安全負載、獨立緊急斷電及不重複的 JSON 路徑。

1. 記錄含時區日期、測試/觀察人員、型號/序號/板版、韌體版本與雜湊、通訊設定、治具/儀器、拓撲及證據位置。
2. 查驗配線、隔離、額定值、防護及緊急斷電，確認能承受重複切換與最長 ON。
3. 停用其他自動化或主站寫入，核對實際通訊參數與權限。
4. 獨立確認全部負載 OFF；工具亦檢查六路 FC01/FC02，全 OFF 才能開始。
5. 若配線或狀態不明、通訊失敗、緊急斷電不可用，即停止，不自動將未知狀態歸零。
6. 選用 I/O 測試須有 RGB、按鍵及 **Allow manual indicators and diagnostic commands**，現有雙擊指派為 identify（6），亮度非零、無故障覆蓋、全輸出 OFF。不要為本測試改寫持久按鍵設定。


TCP 需使用預定專用網路，確認選定 IPv4、埠、unit、Modbus TCP/running、通道啟用、寫入遮罩與來源規則。記下主機來源 IP。範例 `192.0.2.10` 為文件保留位址，必須換成核准目標。

## 3. 驗收項目

| 項目 | 程序及通過條件 |
| --- | --- |
| 基準檢查 | 身份、映射、能力、協定、組態與負面案例皆通過，目標符合紀錄 |
| 初始狀態 | 六路 FC01/FC02 均 OFF，否則不進行輸出測試 |
| 短週期 | CH1 至 CH6 各做三輪 1 秒 ON/1 秒 OFF；每次邊緣與等待後驗證 FC05 回送及全部狀態 |
| 保持測試 | 各路 ON 至少 30 秒，其餘 OFF，之後全 OFF 至少 5 秒 |
| 通道隔離 | 每步只有選定通道 ON 或全 OFF，其他通道不應變化 |
| 最終狀態 | 線圈與輸出皆 OFF，實體接點另行確認 OFF |
| 證據檢閱 | 各步有實際結果與判定；通訊、輸出、接點或復原異常都列失敗/中止 |
| I/O-01 | 一筆 FC16 寫 0x0200–0x0203、opcode 1；讀序號/結果、FC04 RGB/原因及 FC02 活動，應為五秒非黑白色與手動原因，繼電器不變 |
| I/O-02 | 操作者短雙擊 BOOT，工具輪詢按下與手勢計數；恰增加一次雙擊，既有 identify 指派產生相同白色 |
| I/O-03 | 觀察者另記實際白光，暫存器不能證明光學輸出 |

I/O 案例不命令繼電器，但識別與按鍵仍是主動操作。缺少權限時由網頁授權，不可繞過。禁止長按 BOOT，也不要測此程序以外的單擊/三擊；實體按鍵無法遠端注入。

預設最少等待時間為 `6 × (3 × (1 + 1) + 30 + 5) = 246` 秒，另加通訊與執行成本。主機以單調時鐘檢查保持時間，並非量測接點轉換延遲。


## 4. 中止與復原

第一個失敗就停止，包括非預期通道變化、讀值矛盾/遺失、失聯、新增 RTU 訊框錯誤、不安全負載、操作者疑慮或緊急狀況。工具在 `finally` 盡力關閉測試通道，明確記錄復原失敗；斷電、連線、韌體或驅動故障時，無法保證成功。

需要時使用獨立緊急斷電，直接確認實體安全狀態並按核准程序復原。先調查原因並建立新紀錄再重測，不把部分執行當成完整驗收。保留失敗與未完成報告。

## 5. 執行工具及留存證據

完整輸出測試須有 `--confirm-safe-loads`，先完成基準檢查及全 OFF 預檢，失敗不啟動。保持時間須大於 0 且最多 3600 秒，循環 1–100 次。下列會操作輸出，僅限核准後執行，每次使用新報告名稱：


```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.0.2.10 --port 502 --unit 1 --timeout 5 `
  --exercise-all-relays --toggle-cycles 3 `
  --toggle-on-seconds 1 --toggle-off-seconds 1 `
  --on-seconds 30 --off-seconds 5 --confirm-safe-loads `
  --output evidence/modbus-tcp-relay-fat-sat.json
```

確認指示權限及現場觀察者後，可另跑 RGB/按鍵案例：

```powershell
py scripts/verify_modbus.py --transport tcp `
  --host 192.0.2.10 --port 502 --unit 1 --timeout 5 `
  --exercise-device-io --confirm-indicator-observation `
  --button-timeout-seconds 60 `
  --output evidence/modbus-tcp-device-io-fat-sat.json
```

`safeLoadsConfirmed` 與指示確認只記錄人員聲明，不是感測器或硬體互鎖。請將 JSON、實體 OFF/ON/OFF 量測、人員簽核、映像身份、組態快照、治具及選用封包擷取一起保存，移除秘密。結束碼 0 代表要求的軟體檢查通過，1 為檢查失敗，2 為參數或準備無效；軟體結果不能代替實體驗收。
