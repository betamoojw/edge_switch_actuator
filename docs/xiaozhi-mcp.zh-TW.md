# 小智 MCP

致動器透過主動連出的、已驗證憑證的 WSS 連線，向小智開放少量工具。可與 MQTT/Home Assistant 及選定的 Modbus/KNX 並行，不改變現場協定。

## 設定 { #setup }

本節依目前原始碼撰寫。[曾檢查的裝置](live-verification.md)顯示 0.6.3 卻無 MCP 選單，映像身份未確定。找不到選單時，先核對編譯功能與內嵌 UI。

1. 管理員開啟 **Connections → Xiaozhi MCP**，位於 MQTT/NTP 之下。
2. 填寫裝置別名及自己的 `wss://…` 端點；端點通常帶認證資訊，韌體不提供共用預設值。
3. 選擇小智可見及可控的通道，預設不開放任何一路。
4. 啟用後套用。端點預設隱藏，留空保留舊值，新值則取代。管理員勾 **Show endpoint** 才明確讀取，單純顯示不會改設定或重連；隱藏時清除讀取值，未儲存的新草稿仍保留但遮蔽。
5. 等到 **Ready**。儲存只證明持久化成功，Ready 才代表 MCP 初始化完成；若等待時間同步，請先設 NTP。

Reconnect 以儲存設定重新連線，停用仍保留端點。要移除須同次儲存中停用並選 **Remove saved endpoint**。原廠重設清除兩代設定；一般移除後，舊秘密可能仍留在復原代，直到覆寫或原廠重設。資料存於檔案系統，此功能不新增 Flash 加密。

Installer/Operator/Viewer 可讀狀態；管理員才能讀寫組態、查看工具清單或重連。一般 GET、狀態、紀錄及工具結果不含端點；明確顯示端點的 API 是例外。含認證資訊的版本不可開 WebSockets 相依套件除錯紀錄。

## 工具與繼電器行為 { #tools-and-relay-behavior }

工具名稱尾端為完整 12 位 STA MAC，切換上行也不變。別名支援 Unicode，變更會重連並刷新註冊表。外部通道編號 **1–6**。

| 工具前綴 | 功能 |
| --- | --- |
| `actuator_status_` | 僅回報開放通道、修訂、協定及故障；0 off、1 RTU、2 TCP、3 KNX |
| `actuator_get_alias_` | 別名與穩定裝置身份 |
| `actuator_set_relay_` | 必須有 `channel` 與布林 `on` |
| `actuator_pulse_relay_` | 必須有 `channel`、`request_id`，使用儲存的脈衝時間 |
| `actuator_all_off_` | 僅操作開放且啟用通道；事先全部檢查，任一受影響通道鎖定則拒絕整筆 |
| `actuator_identify_` | 五秒識別，RGB 未啟用即失敗 |

未開放通道時不提供繼電器變更工具。所有命令由致動器任務執行，遵守停用/鎖定，不能解鎖、切換協定、重設、更新或修改認證資訊。來源記為 `xiaozhi_mcp`，既有網頁、HA、KNX 狀態路徑會反映結果。

脈衝 ID 保留 60 秒，短暫重連仍有效，共 16 筆。滿時拒絕新脈衝，不淘汰尚未過期項目。同 ID 不同參數或設定修訂遭拒。不會重連後自動重送變更，也不保證跨重啟只執行一次。命令可能已套用但回應遺失，換新 ID 前請查狀態。小智斷線不直接關閉繼電器，既有總線/網路策略仍為準。

## 執行狀態與疑難排解 { #runtime-and-troubleshooting }

狀態包含 Disabled、Waiting for network/time、Connecting、Initializing、Ready、Retrying、Error、Paused。設定 AP 不算對外上行；服務跟隨選定 IPv4 Wi-Fi/乙太網路。支援 DNS 與 IPv4 主機，拒絕 IPv6 字面位址。

正式版本只接受 `wss://`，利用內嵌 CA 驗證主機名及憑證，並要求合理時間；沒有不安全 TLS 選項。`authentication_failed` 為握手 HTTP 401/403，需更新有效端點。`connection_failed` 涵蓋 DNS/TCP/TLS、握手及心跳，請檢查網路、時間及信任。原始遠端錯誤不外露。認證拒絕約 60 秒重試，其他錯誤採 1–60 秒加抖動的指數退避。

TCP 連線與 TLS 握手各設五秒，DNS 依 Arduino 網路堆疊。配接層先建立已驗證安全用戶端，再交由固定 WebSocket 函式庫處理訊框，以避開兩分鐘預設握手。停用函式庫自動重連，由服務安排嘗試。MCP 初始化期限 20 秒，皆是組態限制，不是即時量測保證。

`configuration_invalid` 保留損壞或不支援記錄，不自動覆蓋，需有意執行原廠重設；`storage_failed` 保留原有效設定。編輯衝突時重讀。OTA 會暫停 MCP，最多等三秒確認連線停止；仍在停止則 OTA 報錯供重試。失敗 OTA 恢復 MCP，重設/重啟/睡眠則停止接收命令，此行為不依賴瀏覽器在線。

上限為端點 2048 位元組、別名 64 UTF-8 位元組、重組訊息 8192、巢狀深度 8、回應 16384、單次 ArduinoJson 分配預算 49152；佇列四筆、執行期限兩秒。底層原始訊框配置上限 15 KiB；工具每秒 10 次、突發四次。正式發行仍需量測堆積、堆疊與延遲。

## API { #api }

無參數工具接受省略 `arguments` 或 `{}`，`_meta` 為請求中繼資料，不屬工具 schema。參閱[參數相容修復](tasks/xiaozhi-mcp-tool-parameters-fix.md)。曾發生儲存時 `Stack canary watchpoint triggered (httpd)`，詳見[當機修復紀錄](tasks/xiaozhi-mcp-stack-overflow-fix.md)。目前 HTTP 堆疊至少 8192 位元組，系統狀態 `http_stack_min_free_bytes` 可讀生命期最小剩餘量。

| 路由 | 權限 |
| --- | --- |
| `GET /rest/xiaozhiMcpStatus` | 已認證 |
| `GET`, `POST /rest/xiaozhiMcpSettings` | admin |
| `GET /rest/xiaozhiMcpTools` | admin |
| `POST /rest/xiaozhiMcpReconnect` | admin，排隊後 202 |
| `POST /rest/xiaozhiMcpEndpoint` | admin，明確顯示，內容 `{"revision": <current revision>}` |

顯示端點回應含 `endpoint`、`revision`，附 `Cache-Control: no-store`。修訂過期 409、請求格式異常 422，一般設定/狀態仍遮蔽。UI 在隱藏、重載、儲存成功、分頁隱藏及離開時清除取回值，從不寫 localStorage；新草稿隱藏時只遮蔽。

設定 POST 接受 `revision`、`enabled`、`alias`、`channel_mask`，選用僅寫 `endpoint`、布林 `clear_endpoint`。省略/空值保留端點，清除與替換不可並用，啟用須先有端點。GET 另含 `schema_version`、`endpoint_configured`、`endpoint_host`，這些唯讀欄位不要送回。

HTTP 400 為內容錯誤、401 未認證、403 無權、409 修訂/狀態衝突、422 設定無效、503 儲存/服務不可用。公開錯誤只有穩定 `error`、`field`，不回顯秘密；功能未編入就無路由。

## 編譯與開發 { #build-and-development }

`FT_XIAOZHI_MCP` 全域預設 0，Waveshare 編譯啟用但執行時仍關閉。需 `FT_SECURITY=1`、`FT_NTP=1`，禁止 `SERVE_CONFIG_FILES`，不依賴 MQTT。固定使用 `links2004/WebSockets@2.7.2`。這是專案原生實作，未複製 WLED/廠商包裝或試用授權程式。

`XiaozhiMcpService` 整合設定、狀態與生命週期；可攜式協定/設定與傳輸仍分層，`XiaozhiMcpAdapter` 管理有界致動器佇列，DurableStore 共用於 `lib/framework`。

```text
python scripts/test_xiaozhi_mcp.py
pio run -e waveshare-relay-6ch -e waveshare-relay-6ch-mcp-off -e waveshare-relay-6ch-mcp-no-mqtt
```

模擬器不連小智，以認證控制 API `{"type":"mcp","state":"ready"}` 指定狀態。`mcp-only`、`mcp-off` 只供 UI 測試，不是正式有效編譯組合。契約測試涵蓋設定、遮蔽、授權、儲存失敗與重設。

`esp32dev`、`esp32-wt32-eth01` 使用 LTO 保留雙 OTA，編譯後以 `python scripts/check_firmware_size.py <environment>` 核對真實映像，連結器估算可能漏區段。

隔離測試板可使用 `interface/scripts/mock-xiaozhi-mcp.mjs`，指定 `MCP_TEST_CERT`、`MCP_TEST_KEY`，可選 `MCP_TEST_BIND`、`MCP_TEST_PORT`。測試韌體須信任測試憑證，不能關閉 TLS。此對端初始化、列出工具並唯讀查詢，不操作繼電器。替身傳輸不能證明真實憑證拒絕或雲端相容。

發行驗收還需私有端點、隔離板、無效憑證/主機名、協定並行、OTA/重設，以及 24 小時/100 次重連長測。應記錄實測數據，不以模擬或成功編譯取代。

## 歷史驗證：2026-10-09 { #validation-record-2026-10-09 }

英文原始紀錄保留基於 `7fd6d69` 的軟體檢查及限制：模擬器 25 項、翻譯兩項、型別檢查零錯誤警告，產品及 MCP 變體編譯通過；Firefox 在該主機無法啟動，C3 本機工具鏈下載未完成，WT32 餘裕偏小。後續核准的實機測試見[歷史紀錄](tasks/xiaozhi-mcp-hardware-validation.md)。這些不是本次 0.6.4 重測，完整原始表留在英文版同名歷史段落。
