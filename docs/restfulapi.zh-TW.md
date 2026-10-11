# REST API 與事件

本頁以 `dev` 提交 `bf8cb12` 的 [`ActuatorApi.cpp`](https://github.com/betamoojw/edge_switch_actuator/blob/bf8cb12ab9ada4337751968332c7d5e17e056ccc/src/device/ActuatorApi.cpp) 為依據。選用框架功能未編入時，對應路由不會註冊。

## 認證與權限 { #authentication-and-permissions }

`POST /rest/signIn` 接受包含 `username`、`password` 的 JSON；成功取得 `access_token`，失敗為 401。後續帶 `Authorization: Bearer <access_token>`，JSON 請求另帶 `Content-Type: application/json`。初始 `admin` 帳號使用每台裝置的設定密碼。權杖最長八小時且綁定此次啟動，帳號管理修改會輪換簽章秘密。`GET /rest/verifyAuthorization` 回傳 200/401；這些路由不會為 HTTP 服務加上 HTTPS。

裝置 GET 的 `capabilities` 提供 `configure`、`command`、`admin`、`channels`。遮罩位元 0–5 對應通道 1–6，63 開放全部。**REST 索引為 0–5，UI/MCP/MQTT 為 1–6。**

## 致動器路由 { #actuator-routes }

| 方法與路由 | 權限 | 契約 |
| --- | --- | --- |
| `GET /rest/device/status` | 已認證 | 輸出、網路/協定、指示、計數、故障與稽核 |
| `GET /rest/device/config` | 已認證 | 完整 schema 1 與目前修訂 |
| `POST /rest/device/config` | Installer/admin | 完整組態與目前一般修訂 |
| `POST /rest/device/commands` | Operator/Installer/admin | 操作命令及個別檢查 |
| `POST /rest/protocol/transition` | Installer/admin | `mode`、一般 `revision`，其餘設定保留 |
| `GET /rest/knx/config` | 已認證 | KNX 映像/擁有權/關聯與獨立修訂 |
| `POST /rest/knx/config` | Installer/admin | [KNX 設定交易](knx-address-entry.md) |
| `POST /rest/knx/programming` | Installer/admin | 布林 `active`，需 KNX 模式及 IPv4 上行 |

命令/切換/程式設定路徑也註冊 GET，回傳狀態快照；一般讀取請用 `/rest/device/status`。POST 非物件或序列化大於 8192 位元組回 400；變更由致動器佇列依序執行，滿時 503。

### 繼電器範例 { #relay-examples }

以下送至 `/rest/device/commands` 會實際操作**通道 1**，僅於授權後執行：

```json
{"command":"relay","channel":0,"value":true,"requestId":"panel-0001"}
```

```json
{"command":"pulse","channel":0,"requestId":"panel-0002"}
```

回應含 `ok`、`error`、一般 `revision`、`state`。單路停用/鎖定回 409，通道權限不足回 403。

### 命令 { #commands }

| `command` | 欄位與限制 |
| --- | --- |
| `relay` | `channel` 0–5、布林 `value`，需通道權限 |
| `pulse` | `channel` 0–5，使用儲存時間，需通道權限 |
| `all_on`、`all_off` | 先查全部啟用通道的權限與鎖定，任一失敗整筆 403 |
| `rgb` | `red`/`green`/`blue` 0–255，`brightness` 0–100 預設10，`seconds` 1–30 預設5，RGB 須啟用 |
| `identify` | 五秒識別，仍受 RGB 啟用及高優先序限制 |
| `tone` | `hz` 500–4000 預設2000，`ms` 10–2000 預設100，`duty` 1–50 預設25，蜂鳴器須啟用 |
| `acknowledge` | 靜音目前提示音 |
| `unblock` | Installer/admin，`channel` 0–5，不另外檢查此帳號通道遮罩 |
| `modbus_window` | Installer/admin，`seconds` 0–300 預設60，`peer` 預設 `rtu`，0 關閉 |
| `factory_reset` | admin 旗標及 `confirm: "ERASE"`，清除組態後重啟 |

RGB、聲音及識別依角色授權，不使用逐路繼電器權限，詳見[操作範圍](device-operation.md)。

### 修訂與重試 { #revisions-and-retries }

先 GET，再修改完整組態並以目前 `revision` POST，需六筆繼電器、三個按鍵指派及所有必要型別欄位。成功後修訂遞增。僅切換協定時可送：

```json
{"mode":"modbus_tcp","revision":1,"requestId":"mode-change-0001"}
```

`1` 必須改成剛讀取的修訂。可用模式是 `off`、`modbus_rtu`、`modbus_tcp`、`knx_ip`。回應含 `ok`、`error`、`revision`，成功後再讀確認。KNX 儲存回傳已提交 `knx` 快照及 **KNX 自己的修訂**。

選用 `requestId` 最長 64 字元。以使用者名稱+ID 保留最多 16 筆完成操作、60 秒；相同路徑及完全相同序列化內容回傳既有結果，同 ID 不同請求回 409。快取可能淘汰且重啟後消失，不保證持久恰好一次執行。遺失回應時先查狀態，脈衝尤其不宜直接改新 ID 重送。

| 狀態碼 | 常見意思 |
| --- | --- |
| 200 | 讀取或操作完成 |
| 400 | 格式錯誤或內容過大 |
| 401 | 未認證或權杖無效 |
| 403 | 角色/通道拒絕、批次鎖定或命令不支援 |
| 409 | 修訂/狀態衝突、單路停用/鎖定或套用失敗 |
| 422 | 欄位/schema/範圍無效 |
| 503 | 佇列無法使用或已滿 |

早期認證、資料形狀或佇列失敗可能沒有回應本文。套用失敗也可能是儲存或協定啟動失敗，不一定是修訂，重試前須判讀錯誤。

## 框架與整合路由 { #framework-and-integration-routes }

| 路由 | 用途及存取 |
| --- | --- |
| `GET /rest/features` | 公開編譯功能與韌體識別 |
| `GET /rest/wifiStatus`、`/rest/apStatus`、`/rest/systemStatus` | 已認證狀態 |
| `GET`, `POST /rest/wifiSettings`、`/rest/apSettings` | admin 網路組態 |
| `GET /rest/scanNetworks`、`/rest/listNetworks` | admin 非同步掃描 |
| `GET /rest/mqttStatus`、`/rest/ntpStatus` | 已認證，依編譯功能 |
| `GET`, `POST /rest/mqttSettings`、`/rest/ntpSettings` | admin，MQTT 含選用 HA 探索 |
| `GET`, `POST /rest/ethernetSettings`；`GET /rest/ethernetStatus` | 僅乙太網路板型，設定 admin、狀態認證 |
| `GET`, `POST /rest/securitySettings` | admin，序列化清空密碼/簽章秘密 |
| `GET /rest/generateToken?username=<name>` | admin 產生權杖 |
| `POST /rest/restart`、`/rest/factoryReset` | admin 生命週期操作 |
| `POST /rest/uploadFirmware` | admin multipart 上傳 |
| `POST /rest/downloadUpdate` | admin，JSON `download_url` 為 OTA 應用映像 |
| `GET /rest/coreDump` | 認證後下載當機診斷 |

致動器不註冊選用睡眠路由 `/rest/sleep`。五個 MCP 路由及遮蔽規則見[小智](xiaozhi-mcp.md)，主題授權見 [HA](home-assistant.md)。

## 事件 Socket { #event-socket }

`/ws/events` 於連線接入時認證，預設二進位 MessagePack；`EVENT_USE_JSON=1` 選文字 JSON。邏輯封套含 `event`、`data`：

```json
{"event":"subscribe","data":"device.state"}
```

`device.state` 約每秒發布且唯讀，變更走 REST。取消訂閱改 `event: "unsubscribe"`；`{"event":"ping"}` 回 `{"event":"pong"}`。超過 8192 位元組訊框被拒絕，待送工作有界。重連後重取 REST 快照並訂閱，`lib/device/reconcile.ts` 保留部分更新以外狀態。帳號編輯不保證立即關閉既有連線，參閱[審查](actuator-source-review.md)。
