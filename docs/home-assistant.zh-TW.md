# Home Assistant 整合

`waveshare-relay-6ch` 透過既有 MQTT 連線整合 Home Assistant，不需自訂 HA 元件。探索預設停用，載入舊版設定時也相同；範本板型仍保留燈光示範整合。

## 設定程序 { #setup }

1. 在 HA 設定 [MQTT 整合](https://www.home-assistant.io/integrations/mqtt/)，探索前綴使用 `homeassistant`，birth 主題使用 `homeassistant/status`。
2. 以管理員登入 **Connections → MQTT → Change MQTT Settings**，填入相同代理的 URI 與認證資訊，啟用 MQTT 及 **Home Assistant discovery** 後套用。
3. HA 應探索到一部 **Edge Switch**。繼電器名稱沿用致動器設定；改名或變更 MQTT client ID 不會改變實體識別。

以代理帳號及 ACL 限制只有可信控制器能發布命令。HTTP 帳號與通道權限不適用 MQTT。

## 實體及主題 { #entities-and-topics }

`<id>` 為 `edge_` 加上出廠 STA MAC 的 12 位小寫十六進位字元，切換 Wi-Fi/乙太網路也不變。基底 `<base>` 為 `edge_switch/<id>`。

| 實體 | 行為 |
| --- | --- |
| 六路繼電器開關 | 使用統一繼電器方法，包含啟用/鎖定及 KNX 回報；停用或鎖定時不可用 |
| 六個鎖定二元感測器 | 回報鎖定**或停用**，不授予解除鎖定能力 |
| 運作時間、協定、協定狀態 | 唯讀診斷 |
| 故障、按鍵按下 | 目前故障及去彈跳輸入；短按可能介於一秒快照之間，自動化請用手勢事件 |
| Identify 按鈕 | 五秒識別，仍受 RGB 啟用與優先序限制 |
| 手勢事件 | `single`、`double`、`triple`，僅按鍵功能啟用時發出；本機指派仍會執行，重設長按不發自動化事件 |

| 主題 | 內容與保留設定 |
| --- | --- |
| `homeassistant/<component>/<id>/<entity>/config` | 探索 JSON，QoS 1，保留 |
| `<base>/state` | 不含私有稽核紀錄的狀態 JSON，每秒及命令後發布，QoS 1，保留 |
| `<base>/relay/1/set` … `/relay/6/set` | 精確 `ON` 或 `OFF`，不保留 |
| `<base>/identify/set` | 精確 `PRESS`，不保留 |
| `<base>/event` | `{"event_type":"single"}` 或 `double`/`triple`，QoS 1，不保留 |
| 既有框架狀態主題 | 保留 `online` 及遺囑 `offline`，所有實體共用 |

目前探索前綴及 birth 主題固定。代理重連、收到 HA `online` birth 或通道重新命名時，重新發布探索與狀態。帶 retained 旗標的命令會被丟棄，錯誤主題/內容忽略；不提供 toggle、重設、協定選擇或組態寫入。佇列最多 16 筆，超額丟棄。狀態仍是命令結果，不是接點或負載量測。

## 相容性及生命週期 { #compatibility-and-lifecycle }

MQTT 可與 RTU、TCP、KNX/IP 或 Off 並行，不變更協定、開機輸出、脈衝、按鍵、斷線策略、Modbus 權限或 KNX 設定。MQTT 命令不會重置 RTU 看門狗，後續協定/按鍵命令仍可改變輸出；代理斷線不新增關閉策略。

連線時停用 **Home Assistant discovery** 會清除保留的探索與快照。離線停用則利用 `/config/home-assistant-discovery` 標記，在重啟後補做清理；完成前請讓舊代理保持可達。換代理會在舊主機留下保留紀錄，須自行移除或事先關閉探索。原廠重設或永久移除前，也應連線停用探索；重設無法清理連不到的代理。

`FT_MQTT=0` 時配接器不編入。此選項只在致動器組態提供；既有 REST 路由與致動器 schema 不變，`/rest/mqttSettings` 新增預設 `home_assistant_discovery: false`。

## 審查與驗證 { #review-and-validation }

[10 月 10 日唯讀檢查](live-verification.md)時 MQTT 停用，沒有測試探索。原實作基準 `16e5935` 的網路回呼只將已驗證命令排入佇列或要求重新探索，不寫 GPIO，也不取得致動器鎖。執行任務統一採樣 REST、按鍵、Modbus、KNX、脈衝及看門狗造成的狀態變化。

安裝開發相依項目後，可執行 `python scripts/test_home_assistant.py`，以真實 ArduinoJson 與替身傳輸/RTOS/硬體測試探索、狀態過濾、命令交接、互鎖、保留命令拒絕、非保留手勢、改名、移除、重試及重連。`python scripts/test_native.py` 檢查解析器及產品契約。於 `interface/` 執行 `npm run check`、`npm run test:sim`、`npm run test:i18n`；瀏覽器案例為 `npx playwright test --project=chromium --grep MQTT`。

歷史紀錄曾通過致動器/通用 S3 編譯、原生與配接器測試、Svelte 零錯誤警告、13 項模擬器、兩項翻譯及兩項 Chromium MQTT 測試，不代表本次重跑。

實機驗收仍需代理與 HA，核准後測試六路和各命令來源、鎖定/停用、改名、HA/代理重啟、裝置斷線及停用探索，並量測接點與可用性。主機測試不會燒錄或完成現場設定。參考 [MQTT discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)、[switch](https://www.home-assistant.io/integrations/switch.mqtt/)、[event](https://www.home-assistant.io/integrations/event.mqtt/)。
