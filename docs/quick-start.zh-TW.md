# 初次使用

本指南適用於已安裝 Edge Switching Actuator 的 Waveshare 六路板。若尚未燒錄，請先完成[編譯與燒錄](gettingstarted.md)。安裝及驗收完成前，請隔離現場負載。

## 連線前準備 { #before-you-connect }

備妥 `waveshare-relay-6ch` 韌體、合適電源、瀏覽器、2.4 GHz Wi-Fi，以及裝置專屬設定資訊。向網管確認目標網路，閱讀[硬體配線指南](esp32-s3-relay-6ch-hardware.md)，並記錄板卡版本及映像檔識別資訊。

已完成設定的裝置請使用既有位址與帳號。不要為了重走教學而恢復原廠設定，否則會清除組態。

## 連線及登入 { #connect-and-sign-in }

1. 從支援的電源介面供電。透過私有設定標籤或 115200 baud 序列主控台取得 **Device setup password**，勿公開標籤或主控台擷取內容。
2. 新裝置尚無上行連線時，以該密碼加入 `ESP32-SvelteKit-<unique_id>`，開啟 `http://192.168.4.1`。
3. 使用 `admin` 與本機設定密碼登入。既有帳號維持原密碼；致動器並無共用的原廠密碼。
4. 在 **WiFi → WiFi Station** 輸入目標網路並套用。連線將隨之變更，之後請以路由器分配的 STA 位址存取；可在路由器用戶端清單或本機網路狀態頁確認。
5. 開啟 **Switching Actuator**，應有六張通道卡片、網路摘要、通訊協定狀態與運作時間。原始碼預設為總線 Off、全部輸出 OFF；已儲存的設定可能不同。

這是現場設定程序，不是本次[唯讀檢查](live-verification.md)執行過的工作。

## 下達命令前先確認狀態 { #read-the-dashboard-before-commanding-anything }

先看[介面導覽](interface-tour.md)。**OFF** 代表韌體命令關閉，不保證電路已隔離或接點已斷開。通道旁的 `startup` 表示最後命令來源，並非開機策略設定為 ON。

核對通道名稱、啟用狀態、開機策略、脈衝時間、斷線關閉設定與按鍵指派。KNX 完成設定後，繼電器組態應以 KNX 應用參數為準。另請向安裝人員確認帳號角色及通道權限。

## 決定控制方式 { #choose-a-control-path }

可將總線設為 Off，單獨使用瀏覽器，也可擇一使用 RTU、TCP 或 KNX/IP。MQTT/Home Assistant 及小智 MCP 可另行啟用，條件見[整合方式](integrations.md)。新增整合會引入另一個命令來源，須先與現場負責人協調。

## 第一次受控輸出測試 { #first-controlled-output-test }

取得現場負責人授權並備妥隔離低壓治具後，才可執行；文件檢查並未進行此測試。

1. 選定已啟用且未鎖定的通道及核准負載，依測試計畫暫停其他自動化命令來源。
2. 記錄初始狀態與設定的脈衝時間。
3. 執行該通道的 **Turn ON**，確認 ON 標示，並另外量測治具或接點反應。
4. 執行 **Turn OFF** 並確認畫面與治具，再送出一次 **Pulse**；應先 ON，於設定時間到期後 OFF。
5. 留存結果，恢復約定狀態。發生非預期動作時立即停止，勿以全部開啟測試連線。

[試運轉驗收](commissioning.md)另有重啟、總線、故障及復原程序。交接前請分配適當帳號，妥善保管組態與通道負載對照；異常處理參閱[疑難排解](troubleshooting.md)。
