# SvelteKit 與佈景主題

專案以 `@sveltejs/adapter-static` 產生靜態瀏覽器應用。`routes/+layout.ts` 設定 `ssr = false`、`prerender = false`；ESP32 提供靜態檔案，REST/WebSocket 後端由 C++ 實作。

## 純瀏覽器應用 { #browser-only-application }

請使用用戶端路由與共用元件。SvelteKit 伺服器端點或 `+page.server.ts` 不能取代韌體 API。工具可能在建置時載入模組，瀏覽器專屬全域物件應放在合適的生命週期內存取。

執行 `npm run dev:sim` 可啟動隔離模擬環境；指定 `DEVICE_HOST` 再執行 `npm run dev:device`，即可透過 Vite 代理實機。正式建置不需裝置網址。參閱[前端開發](frontend-testing.md)。

## 應用名稱與識別 { #changing-the-app-name }

網頁產品中繼資料在 `routes/+layout.ts`，靜態圖示等位於 `interface/static/`。0.6.4 將 GitHub 識別值修正為 `betamoojw/edge_switch_actuator`；瀏覽連結與發行 API 使用的 `owner/repository` 應分開管理。

韌體身份來自 `platformio.ini` 的 `APP_NAME`、`APP_VERSION`、`BUILD_TARGET`，與畫面名稱或發行檔名前綴 `edge_switch_actuator` 不同。

## 佈景主題及在地化 { #themes-and-localization }

**System → UI** 提供 Light、Dark、Nord、Dim、Sepia，Automatic 跟隨作業系統明暗設定。七種語言隨程式封裝，偏好儲存在瀏覽器對應的裝置來源，同來源分頁及登入畫面同步。

主題定義在 `app.css`，偏好邏輯在 `lib/stores/preferences.ts`，翻譯來源為 `lib/i18n/translations.tsv`。以 `npm run i18n:build` 產生訊息；協定值、位址及使用者輸入名稱不要翻譯，詳見[偏好指南](ui-preferences.md)。圖表使用 `lib/DaisyUiHelper.ts`、`lib/chart-preferences.ts` 及共用主題變數，勿另設固定配色。

## 驗證 { #validation }

在 `interface/` 執行 `npm run test:i18n`、`npm run check`、`npm run build`、`npm run test:bundle`。互動調整需模擬器與瀏覽器回歸；若變更影響韌體契約或實體行為，仍需原生與硬體測試。
