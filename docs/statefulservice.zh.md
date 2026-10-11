# 框架服务

本页说明固件 0.6.4 的 `lib/framework/` 扩展 API，框架源自 [ESP32-SvelteKit](https://github.com/theelims/ESP32-sveltekit)。[旧教程](framework-tutorial-legacy.md)保留英语灯光示例，其中部分签名已过时，不作为当前操作说明。产品所有权和公开接口见[架构](architecture.md)、[REST](restfulapi.md)。

## 初始化框架 { #initializing-the-framework }

`src/main.cpp` 创建 `PsychicHttpServer` 和产品用 `ESP32SvelteKit(&server, 160)`，先设安全 GPIO、加载设置身份。由 `esp32sveltekit.begin()` 管理启动顺序，不要单独启动 HTTP 服务器。容量须计入每个注册方法及生成的静态路由；`HttpEndpoint<T>` 注册 GET/POST，CORS 时还加 OPTIONS，不能套用旧示例端点数量。

## StatefulService { #stateful-service }

`StatefulService<T>` 用 FreeRTOS 递归互斥锁保护状态。`read(callback)`、`update(callback, originId)` 管理访问，结果为 `StateUpdateResult::CHANGED`、`UNCHANGED`、`ERROR`，只有 CHANGED 传播更新。`updateWithoutPropagation` 跳过 hook 和通知，调用者负责所需提交及通知。

### 更新回调 { #update-handler }

`addUpdateHandler(callback, allowRemove = true)` 返回 ID，用 `removeUpdateHandler(id)` 移除，参数为 `const String &originId`。来源用于防止回声循环，常见 `http`、`mqtt`、客户端 ID、`websocketserver:<clientId>`；它不是授权令牌。

### Hook 回调 { #hook-handler }

`addHookHandler(callback, allowRemove = true)` 接收 `(const String &originId, StateUpdateResult &result)`，普通 update 后、CHANGED 传播前执行，也会收到 UNCHANGED/ERROR；移除用 `removeHookHandler(id)`。无传播更新不会自动调用 hook。

### 读取和更新 { #read-update-state }

示例只改内存，不写产品 GPIO：

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

硬件写入仍由执行器任务负责；通用锁不能替代队列、传输权限或修订。

### JSON 序列化 { #json-serialization }

```cpp
void read(T &state, JsonObject &root);
StateUpdateResult update(JsonObject &root, T &state, const String &originId);
```

分别对应 `JsonStateReader<T>`、`JsonStateUpdater<T>`。使用 `service.read(root, reader)` 和 `service.update(root, updater, originId)`；updater 第三参数必需，旧两参数示例不匹配。序列化前校验类型/范围并脱敏。

### HTTP 端点 { #http-restful-endpoint }

`HttpEndpoint<T>` 参数为 reader、updater、service、server、route、security manager、predicate（默认 IS_ADMIN），`begin()` 注册。GET 返回当前状态；POST 要求对象，无自动传播地应用，ERROR 返回400，CHANGED 调更新回调，再序列化当前状态。该类不实现产品角色/通道/队列，产品修改使用 `ActuatorApi.cpp`。

### 文件持久化 { #file-system-persistence }

`FSPersistence<T>` 将 reader/updater/service 接到 FS 和路径，默认状态更新触发写入，可 `disableUpdateHandler()` 改手动。原地写不同于 DurableStore 验证多代，不能保证中断恢复。详见[持久化](architecture.md#persistence-and-reset)。

### 事件端点 { #event-socket-endpoint }

`EventEndpoint<T>` 接受 reader、updater、service、共享 socket 和事件名；`begin()` 注册事件、写回调、订阅时同步。它不增加逐事件角色校验，不可直接用于可写继电器事件；产品 `device.state` 只读。

### 独立 WebSocket { #websocket-server }

`WebSocketServer<T>` 是另一通用状态封装，参数含 reader/updater、service、server、路径、安全管理器和 predicate，调用 `begin()` 注册。演示 `/ws/lightState` 不是产品事件路径，可写状态必须检查操作权限。

### MQTT { #mqtt-client }

启用 FT_MQTT 后 `getMqttClient()` 返回共享客户端。`MqttEndpoint<T>` 将状态桥接到订阅/发布主题，`configureBroker(subTopic, pubTopic)` 改主题。产品用有界 HA 适配器，网络回调不能写 GPIO。授权由代理 ACL 决定，见 [HA](home-assistant.md)。

## 事件 Socket { #event-socket }

共享 `/ws/events` 接入认证，默认 MessagePack，`EVENT_USE_JSON=1` 用 JSON。信封含 `event`、`data`，如 `{"event":"subscribe","data":"device.state"}`，取消用 `unsubscribe`，心跳 `ping`/`pong`。

`registerEvent(name)` 注册；`onEvent` 参数为 `(JsonObject &root, int originId)`，`onSubscribe` 为 `const String &originId`。`emitEvent(name, root, originId, false)` 发给除来源外的订阅者，true 仅来源，默认 origin 空字符串。`getConnectedClients()` 取连接数，帧/待发有界。账号修改不保证立即撤销既有连接，见[审查](actuator-source-review.md)。通知可用 `getNotificationService()->pushNotification(message, type)`，类型为 PUSHERROR/PUSHWARNING/PUSHINFO/PUSHSUCCESS。

## 安全功能 { #security-features }

通过 security manager 的 `wrapRequest`/`wrapCallback` 使用 `AuthenticationPredicates::NONE_REQUIRED`、`IS_AUTHENTICATED`、`IS_ADMIN`。它们不能替代执行器细粒度授权。启动绑定八小时 JWT、脱敏及 HTTP 局限见[认证](restfulapi.md#authentication-and-permissions)。

## 占位符 { #placeholder-substitution }

配置默认处理支持 `#{platform}`、`#{unique_id}`、`#{random}`。模板保留原样，它们不是秘密存储，也不同于执行器 NVS 身份。示例不要嵌入真实密码。

## 访问设置和服务 { #accessing-settings-and-services }

`ESP32SvelteKit` 提供 `getFS`、`getServer`、`getSecurityManager`、`getSocket`、`getWiFiSettingsService`、`getAPSettingsService`、`getNotificationService`、`getFeatureService`、`getRestartService`。按功能提供 `getSecuritySettingsService`、`getNTPSettingsService`、`getMqttSettingsService`、`getMqttClient`、`getSleepService`、`getBatteryService`、`getXiaozhiMcpService`。通过服务 read/update 操作，不直接改私有状态；需要跟随已提交设置时注册回调。

## 其他功能 { #other-functions-provided }

- `setMDNSAppName(name)` 设置发现名称。
- `addLoopFunction(callback)` 添加框架任务工作，避免阻塞；头文件默认循环间隔10ms不是实时保证。
- `factoryReset()` 是破坏性操作，产品先关闭输出并保留独立设置身份；`recoveryMode()` 请求 AP 恢复，也不是只读。
- 可选 SleepService 提供 `sleepNow()`、`attachOnSleepCallback(callback)`、`setWakeUpPin(pin, level, pinTermination)`；termination 为 FLOATING/PULL_UP/PULL_DOWN，默认引脚/电平来自 `WAKEUP_PIN_NUMBER`、`WAKEUP_SIGNAL`。产品禁用睡眠，改 BOOT 前须审查启动和复位。
- 可选电池提供 `updateSOC`、`setCharging`、`isCharging`、`getSOC`，不会凭空产生测量电路，产品禁用。
- `getConnectionStatus()` 返回 OFFLINE/AP/AP_CONNECTED/NETWORK/NETWORK_CONNECTED/NETWORK_MQTT；STA/STA_CONNECTED/STA_MQTT 为兼容别名。产品上行用 NetworkSupport，不只看 AP 关联。
- `getFeatureService()->addFeature(name, enabled)` 报告真实功能，不应只作界面承诺。

## OTA 更新 { #ota-firmware-updates }

手动上传和服务器下载均须 admin，只用匹配应用 `_ota.bin`，不用合并镜像。自定义服务器须遵守 API 及证书信任。0.6.4 URL 已修复，混合镜像/板型匹配仍宽泛，完整流程见[构建与更新](buildprocess.md)。
