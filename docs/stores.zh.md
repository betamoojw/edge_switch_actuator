# 前端状态存储

## 用户 { #user }

```ts
import { user } from '$lib/stores/user';
```

安全功能启用时，`$user.bearer_token` 是 JWT 字符串，`$user.username` 是用户名，`$user.admin` 是布尔管理员标志。`user.init(token)` 解码身份并初始化；`user.invalidate()` 清除凭据并导航到登录入口。

JWT 保存在浏览器 localStorage，同源脚本可访问，应防止脚本注入。设备默认 HTTP 不加密传输密码/令牌，管理应在受控网络；不能据此承诺未来版本必然修复。

## 事件 Socket { #event-socket }

```ts
import { socket } from '$lib/stores/socket';
```

`socket.on(event, callback)` 订阅，组件销毁时 `socket.off(event)` 清理。框架的 `socket.sendEvent(event, data)` 仅适用于注册了写回调的事件；产品 `device.state` **只读**，继电器修改走 [REST](restfulapi.md)。泛型演示中的 `led`/`LightState` 不属于执行器 API。

```ts
onMount(() => {
  socket.on('device.state', (data) => {
    // Pass the snapshot to the application's state reconciler.
  });
});
onDestroy(() => socket.off('device.state'));
```

上例回调应交给应用状态合并器，不覆盖未保存草稿。未注册事件在串口记录警告；重连、心跳及过期连接由 store 管理，详见[框架事件](statefulservice.md#event-socket)。

## 遥测 { #telemetry }

```ts
import { telemetry } from '$lib/stores/telemetry';
```

| 属性 | 含义 |
| --- | --- |
| `$telemetry.rssi.rssi` | 数值，Wi-Fi RSSI dBm |
| `$telemetry.rssi.ssid` | 字符串，连接的 SSID |
| `$telemetry.rssi.disconnected` | 布尔，RSSI 不可用 |
| `$telemetry.battery.soc`、`.charging` | 电量百分比、充电状态，依可选功能 |
| `$telemetry.ota_status.status`、`.progress`、`.error` | OTA 状态字符串、数值进度、错误字符串 |
| `$telemetry.ethernet.connected` | 布尔，以太网连接，依板型 |

## 分析数据 { #analytics }

```ts
import { analytics } from '$lib/stores/analytics';
```

属性均为 `number[]`，不是单一数值。`uptime` 为运行秒数，`core_temp` 保留传入温度单位；`free_heap`、`used_heap`、`total_heap`、`min_free_heap`、`max_alloc_heap`、`fs_used`、`fs_total`、`free_psram`、`used_psram`、`psram_size` 在存储时除以 1000，使用十进制 kB，而非原始字节。每数组最多保留 1000 个点，默认两秒一点，约 33 分钟；仅浏览器内存记录，不是设备长期审计。
