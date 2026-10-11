# 框架服務

本頁說明韌體 0.6.4 的 `lib/framework/` 擴充 API，框架衍生自 [ESP32-SvelteKit](https://github.com/theelims/ESP32-sveltekit)。[舊教學](framework-tutorial-legacy.md)保留英文燈光範例，但部分簽章已過時，不是現行使用說明。產品狀態管理與介面見[架構](architecture.md)、[REST](restfulapi.md)。

## 框架初始化 { #initializing-the-framework }

`src/main.cpp` 建立 `PsychicHttpServer` 與產品用 `ESP32SvelteKit(&server, 160)`，先設安全 GPIO 並載入設定身份。啟動順序由 `esp32sveltekit.begin()` 控制，不要自行先啟 HTTP。端點容量須計入各註冊方法及產生的靜態路由；`HttpEndpoint<T>` 有 GET/POST，CORS 另加 OPTIONS，舊範例數量不能直接沿用。

## StatefulService { #stateful-service }

`StatefulService<T>` 以 FreeRTOS 遞迴互斥鎖管理狀態，`read(callback)`、`update(callback, originId)` 保護存取。結果為 `StateUpdateResult::CHANGED`、`UNCHANGED`、`ERROR`，僅 CHANGED 傳播通知。`updateWithoutPropagation` 略過 hook 及通知，呼叫端必須補上所需提交/通知。

### 更新回呼 { #update-handler }

`addUpdateHandler(callback, allowRemove = true)` 回傳 ID，可用 `removeUpdateHandler(id)` 移除。回呼接收 `const String &originId`，以來源避免回送循環。常見來源有 `http`、`mqtt`、用戶端 ID、`websocketserver:<clientId>`，來源字串不是授權權杖。

### Hook 回呼 { #hook-handler }

`addHookHandler(callback, allowRemove = true)` 接收 `(const String &originId, StateUpdateResult &result)`，一般 update 後、CHANGED 傳播前呼叫，包括 UNCHANGED/ERROR。移除用 `removeHookHandler(id)`；無傳播更新不會自動呼叫 hook。

### 讀取及更新 { #read-update-state }

範例只更動記憶體，沒有致動器 GPIO：

```cpp
struct DemoState { bool on = false; };
StatefulService<DemoState> demo;
demo.update([](DemoState &state) {
    if (state.on) return StateUpdateResult::UNCHANGED;
    state.on = true;
    return StateUpdateResult::CHANGED;
}, "example");
bool current = false;
demo.read([&](DemoState &state) { current = state.on; });
```

硬體寫入仍由致動器任務管理，通用鎖不能取代命令佇列、傳輸授權或修訂。

### JSON 序列化 { #json-serialization }

```cpp
void read(T &state, JsonObject &root);
StateUpdateResult update(JsonObject &root, T &state, const String &originId);
```

分別對應 `JsonStateReader<T>` 與 `JsonStateUpdater<T>`，呼叫 `service.read(root, reader)`、`service.update(root, updater, originId)`。updater 第三參數不可省略，舊兩參數範例不符合目前 API。序列化前驗證型別/範圍並遮蔽秘密。

### HTTP 端點 { #http-restful-endpoint }

`HttpEndpoint<T>` 接受 reader、updater、service、server、route、安全管理器及 predicate，預設 IS_ADMIN，以 `begin()` 註冊。GET 序列化目前狀態；POST 要求物件，以無自動傳播方式套用，ERROR 回400，CHANGED 呼叫更新處理，再回傳目前狀態。此抽象不提供產品角色/通道規則或佇列，產品變更走 `ActuatorApi.cpp`。

### 檔案持久化 { #file-system-persistence }

`FSPersistence<T>` 將 reader/updater/service 接到 FS 與路徑，預設更新時儲存，`disableUpdateHandler()` 可改手動控制。原地寫入不同於 DurableStore 驗證多代，不應保證寫入中斷復原，見[持久化](architecture.md#persistence-and-reset)。

### 事件端點 { #event-socket-endpoint }

`EventEndpoint<T>` 參數為 reader、updater、service、共用 Socket 及事件名。`begin()` 註冊事件、寫回呼及訂閱同步，沒有逐事件角色授權。不可直接用它開放繼電器寫入，產品 `device.state` 是唯讀。

### 獨立 WebSocket { #websocket-server }

`WebSocketServer<T>` 是另一種通用狀態封裝，接受 reader/updater、service、server、路徑、安全管理器及 predicate，以 `begin()` 註冊。示範 `/ws/lightState` 並非產品事件路徑，開放可寫狀態時須另查操作權限。

### MQTT { #mqtt-client }

FT_MQTT 啟用後，`getMqttClient()` 取得共用用戶端。`MqttEndpoint<T>` 連接狀態與訂閱/發布主題，`configureBroker(subTopic, pubTopic)` 更改主題。致動器使用有界 HA 配接器，MQTT 回呼不可直接寫 GPIO。存取依代理 ACL，非瀏覽器角色，詳見 [HA](home-assistant.md)。

## 事件 Socket { #event-socket }

共用 `/ws/events` 於接入認證，預設 MessagePack，`EVENT_USE_JSON=1` 改 JSON。封套包含 `event`、`data`，如 `{"event":"subscribe","data":"device.state"}`，取消用 `unsubscribe`，心跳為 `ping`/`pong`。

`registerEvent(name)` 註冊名稱，`onEvent` 接收 `(JsonObject &root, int originId)`，`onSubscribe` 接收 `const String &originId`。`emitEvent(name, root, originId, false)` 傳給來源以外的訂閱者，true 只傳來源，預設 origin 為空字串。`getConnectedClients()` 取得連線數，訊框及待送量有界。帳號修改不保證立即撤銷連線，見[審查](actuator-source-review.md)。通知可用 `getNotificationService()->pushNotification(message, type)`，type 為 PUSHERROR/PUSHWARNING/PUSHINFO/PUSHSUCCESS。

## 安全功能 { #security-features }

經安全管理器的 `wrapRequest`/`wrapCallback` 使用 `AuthenticationPredicates::NONE_REQUIRED`、`IS_AUTHENTICATED`、`IS_ADMIN`，不可取代致動器細部授權。綁定啟動的八小時 JWT、遮蔽及 HTTP 範圍見[認證](restfulapi.md#authentication-and-permissions)。

## 預留符號 { #placeholder-substitution }

預設設定處理支援 `#{platform}`、`#{unique_id}`、`#{random}`。範本保留原樣，不是秘密儲存，也不等於 NVS 專屬身份；範例不得含真實密碼。

## 取得設定及服務 { #accessing-settings-and-services }

`ESP32SvelteKit` 提供 `getFS`、`getServer`、`getSecurityManager`、`getSocket`、`getWiFiSettingsService`、`getAPSettingsService`、`getNotificationService`、`getFeatureService`、`getRestartService`。依功能另有 `getSecuritySettingsService`、`getNTPSettingsService`、`getMqttSettingsService`、`getMqttClient`、`getSleepService`、`getBatteryService`、`getXiaozhiMcpService`。使用 read/update API，不直接修改私有狀態，需要追蹤提交時註冊更新回呼。

## 其他功能 { #other-functions-provided }

- `setMDNSAppName(name)` 設定探索用應用名稱。
- `addLoopFunction(callback)` 加入框架任務工作，避免阻塞；標頭預設迴圈間隔10ms不代表即時保證。
- `factoryReset()` 為破壞性操作，產品先輸出 LOW 並保留設定身份；`recoveryMode()` 要求 AP 復原，也不是唯讀診斷。
- 選用 SleepService 提供 `sleepNow()`、`attachOnSleepCallback(callback)`、`setWakeUpPin(pin, level, pinTermination)`；termination 為 FLOATING/PULL_UP/PULL_DOWN，預設來自 `WAKEUP_PIN_NUMBER`、`WAKEUP_SIGNAL`。本產品停用睡眠，變更 BOOT 前需審查啟動及重設。
- 選用電池 API 為 `updateSOC`、`setCharging`、`isCharging`、`getSOC`，不會建立量測電路，本產品停用。
- `getConnectionStatus()` 為 OFFLINE/AP/AP_CONNECTED/NETWORK/NETWORK_CONNECTED/NETWORK_MQTT；STA/STA_CONNECTED/STA_MQTT 是相容別名。產品上行採 NetworkSupport，不僅看 AP 連接。
- `getFeatureService()->addFeature(name, enabled)` 應反映真實能力，不可只是 UI 宣告。

## OTA 更新 { #ota-firmware-updates }

手動上傳及伺服器下載需 admin，只能用相符 `_ota.bin`，不可用合併映像。自訂伺服器須維持下載 API 與憑證要求。0.6.4 修正 URL，但混合映像/板型比對仍有風險，詳見[編譯與更新](buildprocess.md)。
