# 前端結構

正式介面位於 `interface/src/`，使用 Svelte 5、SvelteKit 靜態輸出、Tailwind CSS 4 與 DaisyUI 5。瀏覽器經 `/rest`、`/ws` 與韌體通訊，ESP32 上沒有 Svelte 伺服器應用。

## 路由與模組 { #routes-and-modules }

| `interface/src/` 內的位置 | 職責 |
| --- | --- |
| `routes/+layout.ts`、`+layout.svelte` | 載入功能、產品中繼資料、登入及共用外框 |
| `routes/+page.ts`、`+page.svelte` | 根入口，開啟 `/device` |
| `routes/device/` | 輸出、指示、按鍵、協定、KNX 與維護 |
| `routes/connections/` | MQTT、NTP、小智 MCP |
| `routes/wifi/`、`routes/ethernet/` | 網路狀態及設定，乙太網路依功能旗標顯示 |
| `routes/user/` | 管理員帳號管理 |
| `routes/system/` | 偏好、狀態、指標、核心傾印及更新 |
| `routes/demo/` | 範本示範，不是產品啟動頁 |
| `routes/menu.svelte`、`statusbar.svelte`、`login.svelte` | 導覽、狀態列及登入 |
| `lib/device/types.ts`、`reconcile.ts`、`knx-address.ts` | 資料契約、部分狀態整合、KNX 位址驗證 |
| `lib/stores/` | 使用者、Socket、偏好及遙測 |
| `lib/i18n/` | 翻譯來源、產生的訊息及響應式 API |
| `lib/components/` | 共用控制項、設定卡片、對話框與通知 |
| `lib/types/models.ts` | 框架共用模型 |

## 功能與權限 { #features-and-permissions }

版面將 `/rest/features` 放進 `page.data.features`，供選單及畫面判斷編譯功能。致動器狀態/組態另回傳帳號能力。畫面是否顯示不等於授權，韌體必須在變更前檢查角色與通道遮罩。

0.6.4 採用 `github: 'betamoojw/edge_switch_actuator'`。發行版本元件需要 `owner/repository`，側邊欄則使用獨立文件網址。檔案比對仍有限制，見[韌體更新](buildprocess.md#updating-a-device)。

## 狀態與互動 { #state-and-interaction }

以 MessagePack 接收事件、REST 執行變更。請分開處理已儲存組態、編輯草稿及傳入狀態，背景遙測不可覆蓋未儲存表單。KNX 有獨立修訂，儲存後回傳已提交快照。全部操作也須遵守停用、鎖定與通道權限語義。

`lib/device/reconcile.ts` 整合部分更新並保留既有巢狀資料；`lib/stores/socket.ts` 管理訂閱、心跳、重連與過期連線。正式 UI 不可加入模擬器分支，外部模擬器應提供同一組契約。

## 擴充主選單 { #customize-the-main-menu }

在 `+page.ts` 加上路由中繼資料，並於 `routes/menu.svelte` 增加入口。標題使用翻譯鍵，需與目前頁面的導覽判斷一致；加上適當功能及權限檢查。元件沿用共用設定/對話框模式，顯示文字時使用 `$t()`。另見[偏好設定](ui-preferences.md)、[佈景主題](sveltekit.md)、[前端測試](frontend-testing.md)。
