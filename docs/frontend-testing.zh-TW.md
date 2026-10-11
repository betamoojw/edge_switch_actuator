# 不依賴硬體的前端開發與測試

真實 Svelte 介面可接本機有狀態模擬器或韌體。模擬器位於 `interface/simulator/`，正式 UI 沒有專用分支或假 API 用戶端。請用 Node 24，在 `interface/` 執行；韌體封裝程序不變。

## 快速開始 { #quick-start }

```sh
npm ci
npm run dev:sim
```

開啟 Vite 顯示的 localhost，預設六路致動器。測試帳號有 `admin`/`sim-admin`、`installer`/`sim-installer`、`operator`/`sim-operator`（僅通道1）、`viewer`/`sim-viewer`（唯讀）。全為本機測試資料，不是實機認證資訊；不接觸裝置、MQTT 或 KNX。

MCP 亦為模擬。`mcp-only`、`mcp-off` 檢查功能導覽，以認證 `/__sim/actions` 的 `{"type":"mcp","state":"ready"}` 指定狀態，另見[MCP](xiaozhi-mcp.md)。

連實機須明確提供 `DEVICE_HOST`，範例網域需替換：

```powershell
$env:DEVICE_HOST='device.example'
npm run dev:device
```

```sh
DEVICE_HOST=device.example npm run dev:device
```

兩種模式原樣代理 `/rest` 與 `/ws`。`npm run dev`、`npm run preview` 也要求此變數，允許主機、主機:埠、HTTP(S) 來源，拒絕認證資訊、路徑或查詢字串。正式建置不需目標，`.env.example` 只列說明，不自動載入。

## 宣傳示範影片 { #marketing-demo-video }

錄製器以真實 UI 配合模擬器輸出 1280×720 深色影片，登入後依序展示 Outputs、Indicators、Button、Protocol、KNX、Maintenance，最後全部 OFF 並保留 **Simulator demo** 註記。[原始需求](tasks/device-marketing-demo-video-agent-prompt.md)留作歷史資料。

```sh
npm ci
npx playwright install chromium
npm run dev:sim
```

讓模擬器保持運作，另一終端於 `interface/` 執行 `npm run video:marketing`。主要輸出為 `interface/test-results/marketing/edge-switching-actuator-demo.webm`；會嘗試 MP4，若隨附 FFmpeg 無 H.264 編碼器，則保留 WebM 並回報限制。

以 `DEMO_BASE_URL` 改本機埠、`DEMO_OUTPUT_DIR` 選輸出目錄、`DEMO_OUTPUT_NAME` 設不含路徑/副檔名的名稱。發佈前看開場、互動、六頁及結尾，確認尺寸、畫面有變化、深色主題與模擬揭露。錄製只用測試帳號、不保存組態、結束全 OFF。模擬手勢可設 `SIM_CONTROL_TOKEN`、選用 `SIM_CONTROL_URL`，完成後 Ctrl+C 停止。

## 組態、持久化與生命週期 { #profiles-persistence-and-lifecycle }

`SIM_PROFILE` 可選 `actuator`（預設）、`knx`（ETS 持有的已設定資料）、`template`（LED/代理/睡眠）、`ethernet`、`battery`、`json`（非 MessagePack）、`security-off` 與 MCP 組態。各自表示不同能力，不把所有端點假裝成可用。根頁仍轉 `/device`，範本明確開 `/demo`；致動器不虛構 lightState/brokerSettings。未註冊 REST 回小型 HTML 哨兵，模擬 SPA 非 JSON 回退但不保證位元組相同。乙太網路/電池/安全停用只是框架測試，非板卡能力聲明。

預設僅記憶體，`SIM_STATE_FILE=.sim-state/actuator.json` 可跨進程保存，原子寫入、測試密碼以加鹽 scrypt 儲存，不可跨 profile 載入。重啟保留設定與 KNX，清執行狀態、套用開機輸出、關 socket、作廢權杖。原廠重設恢復測試身份，與實機 NVS 配網身份不同。

預設埠 `FRONTEND_PORT=5173`、`SIM_PORT=3080`、`SIM_CONTROL_PORT=3081`，皆綁 127.0.0.1，衝突就停止啟動。每組埠一次只跑一套，CI 每隔離工作一個 worker。

## 控制 API 與重現場景 { #control-api-and-reproducible-scenarios }

控制 API 用獨立回環埠，不經正式路由代理。`GET /__sim/health` 公開，其餘需 `Authorization: Bearer <token>`，拒絕瀏覽器 Origin。指定 `SIM_CONTROL_TOKEN` 或用啟動顯示的隨機值；測試有專用固定值，不公開實際控制權杖。

| 方法/路徑 | 內容或效果 |
| --- | --- |
| `GET /__sim/state` | 遮蔽狀態/設定/KNX、連線數及 OTA，不含帳號秘密 |
| `POST /__sim/reset` | `{"profile":"actuator"}` 重置測試資料、計時、階段、socket 及故障 |
| `POST /__sim/clock` | `{"advanceMs":61000}`，單次上限一天 |
| `POST /__sim/actions` | `{"type":"relay","channel":0,"value":true}` 外部輸出效果 |
| 同上 | `{"type":"network","wifi":false,"ap":true}`，AP 不算上行 |
| 同上 | `{"type":"knx-object","number":1,"value":true}`，1 Switch、2 Block、3 Status |
| 同上 | `{"type":"knx","busy":true,"owner":"ets"}` 設定互鎖/擁有權 |
| 同上 | `{"type":"modbus","channel":0,"value":true,"clients":1}` 解碼後效果及計數 |
| 同上 | `{"type":"gesture","clicks":1}`，或 pressed/resetArmed/holdMs |
| 同上 | `{"type":"offline","active":true,"durationMs":5000}` API 離線但控制可用 |
| 同上 | `{"type":"failure","target":"persistence"}` 或 protocol，下一次組態失敗 |
| 同上 | `{"type":"reboot"}`、`{"type":"factory-reset"}` |
| 同上 | `{"type":"ota","outcome":"Simulated write failure"}` 或 success |
| 同上 | scan/networks、mqtt/connected/error、battery/soc/charging、coredump/available、fault/code/error、notification/level/message |
| `POST /__sim/faults` | `{"path":"/rest/device/status","effect":"http","status":503,"count":2}` |
| 同上 | latency/ms、malformed JSON、disconnect、hold，選用 method/count（預設1） |
| 同上 | `{"clear":true}` 清除規則、以503放行掛起回應、恢復事件 |
| `POST /__sim/socket` | action close/malformed/pause/refuse；pause/refuse 可用 active:false 復原 |

設定及協定變更用正式路由；控制 API 模擬裝置來源刺激。KNX/Modbus 須選中且運作，繼電器仍受啟用/鎖定。模型含獨立一般/KNX 修訂、ETS 接管、帳號遮罩、去重、脈衝、斷線/看門狗、亮度與期限。回應保留韌體封套及框架401/產品403差異，契約另覆蓋空回應、掃描202、二進位傾印與 multipart。

## 自動化檢查 { #automated-checks }

```sh
npm run check
npm run build
npm run test:bundle
npm run test:sim
npx playwright install chromium firefox webkit
npm run test:e2e
npm run test:e2e:built
```

`test:sim` 用 Node 測試執行器及真實 HTTP/WebSocket。`test:e2e` 啟停全新模擬器/Vite，以 Chromium、Firefox、WebKit、行動 Chromium 操作真實 UI，短迴圈可加 `-- --project=chromium`。built 套件先建置，再用靜態輸出與 preview。執行中不可修改/重建前端引發熱載入，也不可另占測試埠。

僅攔截外部 GitHub 發行查詢，裝置 REST/socket 為真實本機流量。各案例重設瀏覽器及資料，使用回應/DOM 斷言和有界輪詢，不用 networkidle。報告、JUnit、trace、截圖與影片存於忽略的 `playwright-report/`、`test-results/`，開發/built 路徑分開。`test:bundle` 排除模擬器/控制/測試標記。企業代理干擾回環時，只調整測試 shell 環境，不做全域修改。

`.github/workflows/frontend-tests.yml` 對相關 dev/main 推送與 PR 執行檢查及隔離瀏覽器工作，失敗上傳產物；Chromium 額外執行 built。

## 覆蓋範圍與實機檢查 { #coverage-and-physical-device-verification }

HOME-01/KNX-04 覆蓋首頁及位址驗證；案例另含登入、權限、外部狀態、儲存/放棄/重啟、錯誤/掛起、指示/按鍵、協定/KNX、網路、MQTT/NTP/帳號、OTA、遙測、傾印、重設及範本。完整計畫還有未模擬的物理及更多欄位組合。37/39 案例是舊階段的數字，不能當作目前總量。

唯讀實機測試需私下設定 `DEVICE_HOST`、`DEVICE_USERNAME`、`DEVICE_PASSWORD`，使用安全輸入，勿寫命令歷史或共用紀錄：

```sh
npm run test:e2e:device -- --project=chromium
npm run contract:capture -- test-results/device.json
```

`E2E_BASE_URL` 可改為裝置 SPA。套件只登入及讀取，不控制、不改設定、不重設或燒錄；capture 留下遮蔽的回應形狀，依 `interface/tests/contracts/README.md` 與對應模型比對。接點、RS485 時序、ETS、斷電與 OTA 仍需另行授權的實機驗證。

## 模擬限制與既有 UI 行為 { #deliberate-simulation-limits-and-existing-ui-behavior }

- 量測、掃描、MAC/IP、傾印與 NTP 時間為合成資料，沒有真正 POSIX 時區引擎、NTP 或接點感測。
- KNX/Modbus 注入只模擬解碼後效果，不實作線路堆疊、ETS 下載、TCP 對端認證、RF/UART/PHY/Flash、FreeRTOS 或逐位元組 ArduinoJson；可注入佇列滿。
- OTA 只接受有界、含 ESP32-S3 標頭的 multipart 測試資料，模擬進度/檢查碼/錯誤；不燒錄，也不抓取 `download_url`，僅暫存記憶體。MD5 分塊和分割容量需真實比較。
- 預設/正規化依已實作原始碼，函式庫轉型及裝置配網身份仍須保真檢查，保留 Wi-Fi updater 的雙次計數增量。
- 手動指示與顏色有模型，但自主提示聲及 RTOS 閃爍不是物理時序保證。
- UI 沒有 fetch 期限，掛起命令保持忙碌直到傳輸結束，測試明確釋放故障；階段失效回首頁，模擬器不修補 socket 既有限制。
- 未附場景儀表板，認證控制 API 即為自動化入口。

## 歷史驗證（2026-09-28） { #implementation-validation-2026-09-28 }

當時 Windows/Node24：契約12項，Chromium/WebKit/行動各37、共111；built Chromium37，型別零錯誤警告，建置/隔離、產品契約3項及格式通過，仍有大區塊警告。Firefox 在頁面前遭 SideBySide/mozglue 錯誤阻止，未列通過。首次 built 伺服器中斷，隔離重試及完整重跑後通過。英文同名段落保留原紀錄，非本次重測或硬體驗收。
