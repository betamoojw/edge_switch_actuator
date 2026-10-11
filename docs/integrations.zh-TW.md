# 選擇整合方式

瀏覽器可獨立運作。Protocol 分頁的現場總線只能擇一：Off、Modbus RTU、Modbus TCP 或 KNX/IP。MQTT/Home Assistant 及小智 MCP 可並行；不同來源共用輸出狀態，較晚接受的命令可能覆蓋先前操作。

| 路徑 | 先備條件 | 第一次確認 | 限制 |
| --- | --- | --- | --- |
| 瀏覽器/REST | 位址與帳號 | 讀取儀表板或認證狀態 | HTTP，受角色與通道遮罩限制 |
| Modbus RTU | RS485 主站、站號/序列參數、終端匹配 | FC04 偏移 0、數量 5 | 信任實體總線，無個別主站認證 |
| Modbus TCP | IPv4 上行、允許的主站，預設 502 | 同一組暫存器 | 最多四個用戶端，精確 IP 限制不等於加密 |
| KNX/IP 路由 | IPv4 多播網路與位址規劃 | 查看快照及群組關聯 | 無 KNX TP，不保證隧道伺服器；ETS 驗收仍待完成 |
| Home Assistant | MQTT 代理、HA MQTT、啟用探索 | 先查探索與可用性 | 由代理 ACL 授權，不套用網頁遮罩 |
| 小智 MCP | 私有 WSS 端點、NTP、開放通道 | Ready 後查詢狀態工具 | 初始未開放通道，不自動重送變更 |

## Modbus：先讀取，再操作 { #modbus-example-a-read-before-a-write }

完成通訊設定後，以站號 **1** 讀取輸入暫存器零起算偏移 `0x0000`、數量 `5`。依序為映射版本、能力、協定、網路及故障；映射應為 `1`，RTU/TCP 為 `1`/`2`。逾時須先排解連線，不要轉而嘗試寫入。`30001` 等顯示編號因主站而異，可用時請選零起算偏移。

FC01 偏移 `0`、數量 `6` 回報命令輸出。FC05 以 `0xFF00` 開啟、`0x0000` 關閉，會實際操作硬體，僅限核准測試；不支援廠商示範的 `0x5500`。參閱[對照表](actuator-modbus-map.md)、[驗證工具](modbus-verifier-quickstart.md)及 [RTU](modbus-rtu-relay-fat-sat.md)/[TCP](modbus-tcp-relay-fat-sat.md) 驗收。

## KNX：分開命令與狀態回報 { #knx-example-separate-command-and-feedback }

依拓撲挑選唯一個別位址，例如可用時的 `1.1.20`。通道 1 物件 1 為 Switch、2 為 Block、3 為 Status；可將 `1/0/1` 給 Switch、`1/1/1` 給 Status，不用 Block 時留白。這只是範例，不應直接套用現場。三者使用已實作的單位元行為，DPT/旗標設計背景見[歷史設計](actuator-knx-design.md)。

網頁套用會變更組態。先協調 ETS/web 擁有權、讀取修訂並檢查關聯，再依核准程序儲存及讀回。Ready 不能證明切換正常，須有受控群組寫入與獨立接點觀察，見[位址設定](knx-address-entry.md)。

## Home Assistant：先探索，再自動化 { #home-assistant-example-discover-before-automating }

確認裝置與 HA 連到預定代理，才開啟探索。應出現一部 Edge Switch、六路繼電器及診斷實體。`edge_switch/<id>/relay/1/set` 接受精確 `ON`/`OFF`，**請勿保留發布**。`<id>` 是裝置身份，不是顯示名稱；發布會改變硬體。

先查看可用性及狀態，再操作核准的一路。停用或鎖定的通道應不可用，ON 不是負載量測。重新命名、改代理、停用探索或移除裝置前，請看[生命週期](home-assistant.md)。

## 小智：從狀態查詢開始 { #xiaozhi-example-begin-with-status }

依[MCP 指南](xiaozhi-mcp.md)設定私有端點，不可公開。Ready 後從實際工具清單取得 `actuator_status_<device-identity>`，輸入 `{}`，僅回傳開放通道；不要自行猜測後綴。

設定繼電器與脈衝皆為主動操作。脈衝需 `request_id`；回應遺失時先查狀態，再決定新 ID。重複請求保護有容量與時間限制，且不跨重啟保存。文件檢查的裝置未提供 MCP 選單，因此本節依原始碼說明，並非小智實連結果。
