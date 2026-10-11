# Edge Switching Actuator

透過瀏覽器或自動化網路控制六路繼電器。本專案結合 ESP32-S3 韌體、Svelte 5 / SvelteKit 操作介面，以及試運轉、測試與韌體封裝工具。

本文件以 **`dev` 分支、韌體 0.6.4** 為準，原始碼審查基準為 [`bf8cb12`](https://github.com/betamoojw/edge_switch_actuator/tree/bf8cb12ab9ada4337751968332c7d5e17e056ccc)，日期為 **2026 年 10 月 11 日**。開發分支的功能說明不等於已通過正式環境資格驗證。請先參閱[版本更新](release-notes.md)。

## 開始使用 { #start-here }

[![配備外接天線及螺絲端子的六路致動器外殼。](edge_switch_actuator_waveshare-relay.png)](esp32-s3-relay-6ch-hardware.md#hardware-overview)

*專案提供的硬體圖片。[硬體參考](esp32-s3-relay-6ch-hardware.md)列出腳位、配線原則與[外殼尺寸](esp32-s3-relay-6ch-hardware.md#enclosure-dimensions)。*

[連接第一部裝置](quick-start.md){ .md-button .md-button--primary }
[瀏覽實機介面](interface-tour.md){ .md-button }

![唯讀檢查時，實機六路輸出皆顯示 OFF。](media/live/outputs.jpg)

*英文介面，韌體 0.6.3，擷取日期為 2026 年 10 月 10 日。儲存設定可能與預設值不同；畫面無法證明接點實際動作。詳見[檢查紀錄](live-verification.md)。*

| 想完成的工作 | 建議文件 |
| --- | --- |
| 連接已燒錄的裝置 | [初次使用](quick-start.md) |
| 編譯韌體或試用模擬器 | [開發入門](gettingstarted.md) |
| 規劃安裝、配線與交接 | [硬體](esp32-s3-relay-6ch-hardware.md)、[驗收](commissioning.md) |
| 操作繼電器、指示燈與按鍵 | [裝置操作](device-operation.md) |
| 取得裝置密碼或製作設定標籤 | [認證資訊](device-credentials.md) |
| 接上 Modbus 或 KNX | [暫存器對照](actuator-modbus-map.md)、[KNX 設定](knx-address-entry.md) |
| 串接自動化服務 | [Home Assistant](home-assistant.md)、[小智 MCP](xiaozhi-mcp.md) |
| 了解或擴充程式 | [架構](architecture.md)、[API](restfulapi.md)、[前端](structure.md) |
| 評估部署與處理異常 | [原始碼審查](actuator-source-review.md)、[疑難排解](troubleshooting.md) |

## 應用情境 { #practical-uses }

- **現場操作面板：** 讓操作人員依角色與通道權限控制具名輸出、發出定時脈衝，並查看命令狀態。
- **樓宇自動化評估：** 透過 Modbus 或 KNX/IP 路由整合獨立負載；須先確認負載條件並完成試運轉。本專案不具備馬達互鎖或安全控制器功能。
- **居家自動化：** 以 MQTT 及 Home Assistant 自動探索繼電器與診斷實體，由訊息代理的存取規則控管權限。
- **語音與代理程式控制：** 經由私有 WSS 端點，將指定通道開放給小智 MCP。
- **開發與工作臺測試：** 在取得實體操作授權前，先以模擬器及主機端測試確認軟體契約。

[整合方式](integrations.md)說明所需條件。應用說明不代表硬體已取得認證或通過用途適用性評估。

## 已實作功能 { #what-the-product-implements }

預設 `waveshare-relay-6ch` 組態提供六路繼電器、BOOT 按鍵、RGB 指示燈、蜂鳴器及 RS485。每路可設定啟用狀態、名稱、開機狀態、脈衝時間與斷線處理。入口為 `/device`，所有操作受認證與權限控管。

現場通訊協定只能擇一啟用：**Off、Modbus RTU、Modbus TCP 或 KNX/IP**。MQTT/Home Assistant 與小智 MCP 可同時搭配使用；兩者預設停用，MCP 初始未開放任何繼電器。

管理項目涵蓋 Wi-Fi STA/AP、NTP、使用者、遙測、當機診斷及 OTA。[介面偏好設定](ui-preferences.md)提供七種語言與五種佈景主題。乙太網路僅用於其他框架板型，Waveshare 預設組態未啟用。

## 適用範圍 { #boundaries }

- 狀態代表韌體命令，並無繼電器接點回授。
- 設定用 AP 不算 TCP、KNX 或對外服務需要的上行連線。
- 管理服務使用 HTTP，請置於受控網路。
- KNX 產品套件、既有硬體紀錄及模擬器測試都有範圍限制，參閱[驗證紀錄](validation.md)。
- 其他 PlatformIO 板型仍使用框架範本或示範程式，無法直接取代六路致動器組態。

## 專案與授權 { #project-and-license }

[原始碼儲存庫](https://github.com/betamoojw/edge_switch_actuator/tree/dev)。本專案衍生自 [ESP32-SvelteKit](https://github.com/theelims/ESP32-sveltekit) 及其 ESP8266 React 前身。後端使用 LGPL-3.0，前端使用 MIT，詳見 [LICENSE](https://github.com/betamoojw/edge_switch_actuator/blob/dev/LICENSE)。

## 畫面版本註記 { #screenshot-currency }

`media/live/` 圖片為 **0.6.3 英文介面**。0.6.4 已將側邊欄的 Discord 連結改為 **Project website**。使用者提供的 KNX 圖片未確認韌體版本；這些截圖只記錄當時畫面，並非 0.6.4 的硬體驗證。
