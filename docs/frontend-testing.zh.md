# 无硬件前端开发与测试

真实 Svelte UI 可连接本地有状态模拟器或固件。模拟器在 `interface/simulator/`，生产 UI 没有模拟器分支或伪 API 客户端。使用 Node 24，在 `interface/` 执行命令；固件打包流程不变。

## 快速开始 { #quick-start }

```sh
npm ci
npm run dev:sim
```

打开 Vite 打印的 localhost 地址，默认六路执行器。测试账号为 `admin`/`sim-admin`、`installer`/`sim-installer`、`operator`/`sim-operator`（仅通道1）、`viewer`/`sim-viewer`（只读）。它们只用于模拟器，不是真实设备凭据，不连接物理设备、MQTT 或 KNX。

MCP 也仅模拟；`mcp-only`、`mcp-off` 检查按功能显示导航。对认证的 `/__sim/actions` 发送 `{"type":"mcp","state":"ready"}` 设置状态，见[MCP](xiaozhi-mcp.md)。

真实设备需明确指定 `DEVICE_HOST`（示例地址须替换）：

```powershell
$env:DEVICE_HOST='device.example'
npm run dev:device
```

```sh
DEVICE_HOST=device.example npm run dev:device
```

两模式均原样代理 `/rest`、`/ws`。`npm run dev`、`npm run preview` 也需该变量；允许主机、主机:端口或 HTTP(S) 来源，不接受凭据、路径、查询串。构建无需目标，`.env.example` 仅说明变量，不自动加载。

## 宣传演示视频 { #marketing-demo-video }

录制器使用真实 UI 和模拟器，输出 1280×720 深色主题视频，登录后依次演示 Outputs、Indicators、Button、Protocol、KNX、Maintenance，最终全 OFF，并保留 **Simulator demo** 标记。[原需求](tasks/device-marketing-demo-video-agent-prompt.md)为历史资料。

```sh
npm ci
npx playwright install chromium
npm run dev:sim
```

保留模拟器终端，另一终端在 `interface/` 执行 `npm run video:marketing`。主产物 `interface/test-results/marketing/edge-switching-actuator-demo.webm`；尝试转 MP4，但内置 FFmpeg 可能无 H.264，此时保留受支持 WebM 并报告限制。

`DEMO_BASE_URL` 可改本地端口，`DEMO_OUTPUT_DIR` 改目录，`DEMO_OUTPUT_NAME` 为无路径/扩展名名称。检查开场、切换、六页和结束画面，分辨率、动态帧、深色主题及模拟器披露。仅用本地测试账号，不保存配置，结束全 OFF。按键模拟可设 `SIM_CONTROL_TOKEN`、可选 `SIM_CONTROL_URL`；完成 Ctrl+C 停止。

## 配置、持久化与生命周期 { #profiles-persistence-and-lifecycle }

`SIM_PROFILE` 支持 `actuator`（默认）、`knx`（ETS 所有的已调试状态）、`template`（LED/代理/睡眠）、`ethernet`、`battery`、`json`（替代 MessagePack）、`security-off`，以及 MCP 配置。每种是独立能力，不假装所有路由存在。首页仍转 `/device`，演示须显式 `/demo`，执行器不伪造 lightState/brokerSettings。未注册 REST 返回小 HTML 标记，模拟非 JSON SPA 回退，不要求字节一致。以太网/电池/安全关闭测试不说明产品硬件有这些能力。

默认仅内存；`SIM_STATE_FILE=.sim-state/actuator.json` 可跨进程保存，原子写入、测试密码 salted scrypt，不能跨 profile 加载。重启保留设置/KNX，清运行状态、应用启动策略、关 socket、令牌失效。模拟器复位恢复测试账号，不同于真实 NVS 专属身份。

端口默认 `FRONTEND_PORT=5173`、`SIM_PORT=3080`、`SIM_CONTROL_PORT=3081`，全部绑定 127.0.0.1，冲突则启动失败。每套端口只跑一套，CI 每隔离作业一个 worker。

## 控制 API 与场景 { #control-api-and-reproducible-scenarios }

控制 API 单独回环端口，不经生产代理。`GET /__sim/health` 公开，其余需 `Authorization: Bearer <token>`，拒绝带浏览器 Origin 请求。设置 `SIM_CONTROL_TOKEN` 或用启动打印的随机值；测试使用自己的本地固定值，勿公开实际控制令牌。

| 方法/路径 | 请求或效果 |
| --- | --- |
| `GET /__sim/state` | 脱敏状态/配置/KNX、连接数、OTA，无用户密码令牌 |
| `POST /__sim/reset` | `{"profile":"actuator"}` 重置夹具、计时、会话、socket、故障 |
| `POST /__sim/clock` | `{"advanceMs":61000}`，单次最多一天 |
| `POST /__sim/actions` | `{"type":"relay","channel":0,"value":true}` 模拟外部输出 |
| 同上 | `{"type":"network","wifi":false,"ap":true}`，仅 AP 不算上行 |
| 同上 | `{"type":"knx-object","number":1,"value":true}`，对象1 Switch、2 Block、3 Status |
| 同上 | `{"type":"knx","busy":true,"owner":"ets"}` 模拟下载互锁/所有权 |
| 同上 | `{"type":"modbus","channel":0,"value":true,"clients":1}` 已解码效果/计数 |
| 同上 | `{"type":"gesture","clicks":1}`，或 pressed/resetArmed/holdMs |
| 同上 | `{"type":"offline","active":true,"durationMs":5000}` API 离线但控制可用 |
| 同上 | `{"type":"failure","target":"persistence"}` 或 protocol，下次保存失败 |
| 同上 | `{"type":"reboot"}`、`{"type":"factory-reset"}` |
| 同上 | `{"type":"ota","outcome":"Simulated write failure"}` 或 success |
| 同上 | scan/networks、mqtt/connected/error、battery/soc/charging、coredump/available、fault/code/error、notification/level/message |
| `POST /__sim/faults` | `{"path":"/rest/device/status","effect":"http","status":503,"count":2}` |
| 同上 | latency/ms、malformed JSON、disconnect、hold，可选 method/count（默认1） |
| 同上 | `{"clear":true}` 清规则，将挂起响应以503释放，恢复事件 |
| `POST /__sim/socket` | action 为 close/malformed/pause/refuse，pause/refuse 可用 active:false 恢复 |

协议/设置应走生产路由，控制动作模拟设备来源刺激。KNX/Modbus 要求当前协议已运行；继电器遵守启用/锁定。模拟一般/KNX 独立修订、ETS 接管、账号掩码、去重、脉冲、断连/看门狗、亮度与时长。响应保留固件信封和框架401/产品403区别，契约还覆盖空响应、扫描202、二进制转储和 multipart。

## 自动检查 { #automated-checks }

```sh
npm run check
npm run build
npm run test:bundle
npm run test:sim
npx playwright install chromium firefox webkit
npm run test:e2e
npm run test:e2e:built
```

`test:sim` 用 Node 测试器及真实 HTTP/WebSocket；`test:e2e` 启停新模拟器/Vite，在 Chromium、Firefox、WebKit、移动 Chromium 跑真实 UI，快速迭代可加 `-- --project=chromium`。built 套件需先构建，使用静态输出/preview。测试中勿编辑或重建导致热刷新，也勿另占端口。

只拦截外部 GitHub 发布请求，设备 REST/socket 仍是真实本地流量。每例重置上下文和夹具，使用响应/DOM断言及有界轮询，不依赖 networkidle。报告、JUnit、trace、截图/视频在被忽略的 `playwright-report/`、`test-results/`，开发/built 分开。`test:bundle` 检查产物无模拟器/控制/测试标记。企业代理干扰回环时仅调整测试 shell 的代理变量，不全局修改。

`.github/workflows/frontend-tests.yml` 对相关 dev/main 推送及 PR 执行检查和隔离浏览器任务，失败上传产物，Chromium 任务另跑 built。

## 覆盖与实机验证 { #coverage-and-physical-device-verification }

HOME-01/KNX-04 验证首页和地址输入；测试覆盖登录、权限、外部输出、保存/丢弃/重启、错误/挂起、指示/按键、协议/KNX、网络、MQTT/NTP/用户、OTA、遥测、转储、复位及模板。完整矩阵还含未模拟的物理和更多字段组合。旧记录的37/39场景数属于当时版本，不应充当当前总数。

实机只读套件需要私下提供 `DEVICE_HOST`、`DEVICE_USERNAME`、`DEVICE_PASSWORD`（用安全输入方式，不写历史命令或共享日志）：

```sh
npm run test:e2e:device -- --project=chromium
npm run contract:capture -- test-results/device.json
```

`E2E_BASE_URL` 可指定设备 SPA。套件仅登录及读取，不调用控制/修改/复位/刷写；capture 保存脱敏响应形状，按 `interface/tests/contracts/README.md` 与对应模拟器比较。接点、RS485 时序、ETS、掉电、OTA 仍需独立获授权硬件验证。

## 模拟边界与既有行为 { #deliberate-simulation-limits-and-existing-ui-behavior }

- 测量、扫描、MAC/IP、转储、NTP 时间都是合成；不运行真实 POSIX 时区引擎或 NTP，不测触点。
- KNX/Modbus 注入是应用效果，不模拟线路栈、ETS 下载、TCP 对端认证、RF/UART/PHY/Flash、FreeRTOS 调度或逐字节 ArduinoJson；可注入队满错误。
- OTA 只接受有界、ESP32-S3 头的 multipart 夹具，模拟进度/校验/错误；不刷写或抓取 `download_url`，二进制仅暂存内存。MD5 分块和分区需实机比较。
- 默认值/归一化按已实现源码，库转换及设备专属配网仍需保真比对，保留 Wi-Fi updater 双计数增量行为。
- 手动指示颜色/计时可模拟，但自主提示音和 RTOS 闪烁不保证物理时序。
- UI 无 fetch 截止时间，挂起请求持续忙直到传输结束，测试显式释放故障；会话失效转首页，socket 限制不被模拟器“修复”。
- 不附场景仪表盘，认证控制 API 即为自动化入口。

## 历史验证（2026-09-28） { #implementation-validation-2026-09-28 }

当时 Windows/Node24：契约12项，Chromium/WebKit/移动各37项共111，built Chromium37，类型零错误警告，构建/隔离、产品契约3项、格式通过；保留大块警告。Firefox 在页面前被 SideBySide/mozglue 错误阻止，未计通过。首次 built 服务中断，隔离重试后完整通过。原始记录留在英文同名段落，这些不是本次重测或硬件验收。
