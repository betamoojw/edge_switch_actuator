# 前端狀態儲存

## 使用者 { #user }

```ts
import { user } from '$lib/stores/user';
```

安全功能啟用時，`$user.bearer_token` 為 JWT 字串，`$user.username` 是帳號名稱，`$user.admin` 是管理員布林旗標。`user.init(token)` 解碼身份並初始化；`user.invalidate()` 清除資料後導向登入入口。

JWT 放在瀏覽器 localStorage，同來源腳本能存取，須避免指令碼注入。預設 HTTP 不加密密碼或權杖，管理應限定受控網路；此限制不代表已承諾未來修復時程。

## 事件 Socket { #event-socket }

```ts
import { socket } from '$lib/stores/socket';
```

以 `socket.on(event, callback)` 訂閱，元件卸載時 `socket.off(event)` 清理。`socket.sendEvent(event, data)` 只適用已有寫入回呼的框架事件。產品 `device.state` **唯讀**，繼電器命令須用 [REST](restfulapi.md)；範本的 `led`/`LightState` 不是致動器 API。

```ts
onMount(() => {
  socket.on('device.state', (data) => {
    // Pass the snapshot to the application's state reconciler.
  });
});
onDestroy(() => socket.off('device.state'));
```

上例回呼應交由應用狀態整合器處理，避免蓋掉未儲存草稿。訂閱未註冊事件會在序列紀錄留下警告；store 處理重連、心跳及過期連線，另見[框架事件](statefulservice.md#event-socket)。

## 遙測 { #telemetry }

```ts
import { telemetry } from '$lib/stores/telemetry';
```

| 屬性 | 定義 |
| --- | --- |
| `$telemetry.rssi.rssi` | 數值，Wi-Fi RSSI dBm |
| `$telemetry.rssi.ssid` | 字串，目前 SSID |
| `$telemetry.rssi.disconnected` | 布林，RSSI 無法取得 |
| `$telemetry.battery.soc`、`.charging` | 電量百分比及充電狀態，依選用功能 |
| `$telemetry.ota_status.status`、`.progress`、`.error` | OTA 狀態字串、進度值及錯誤字串 |
| `$telemetry.ethernet.connected` | 乙太網路連線布林值，依板型 |

## 分析歷程 { #analytics }

```ts
import { analytics } from '$lib/stores/analytics';
```

每個屬性都是 `number[]`，不是單一讀值。`uptime` 是運作秒數，`core_temp` 沿用輸入溫度單位。`free_heap`、`used_heap`、`total_heap`、`min_free_heap`、`max_alloc_heap`、`fs_used`、`fs_total`、`free_psram`、`used_psram`、`psram_size` 進入 store 時除以 1000，單位為十進位 kB，不是原始位元組。各陣列上限 1000 點，預設兩秒一筆約 33 分鐘；這是瀏覽器記憶體歷程，不是裝置永久稽核紀錄。
