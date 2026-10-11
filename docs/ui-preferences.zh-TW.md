# 介面偏好設定

在 **System → UI Preferences** 選擇語言與佈景主題，變更即時生效，儲存於此瀏覽器對應裝置來源的本機資料。無須寫入韌體或重新啟動；同來源分頁會同步，登入頁也沿用選項。這不是帳號設定，換瀏覽器、網址或來源後不會自動帶入。

## 語言與佈景主題

裝置介面支援 English、Deutsch、Español、Français、Polski、简体中文、繁體中文。翻譯隨韌體封裝，可離線載入；原始通訊錯誤與診斷文字不一定有翻譯。文件網站的三種語言選擇與裝置偏好設定各自獨立。

可選 light、dark、nord、dim、sepia，或以 Auto 跟隨作業系統。圖表應採用目前佈景配色，避免固定顏色。

## 開發維護

修改翻譯 TSV 後執行 `npm run i18n:build` 產生訊息 JSON。各語言須有相同的完整鍵集合及具名插值參數。介面文字以 `$t` 取得，日期、數字與時間長度以目前 locale 的 `Intl` 格式化；API 欄位、協定值及設定鍵不可翻譯。

於 `interface/` 執行 `npm run test:i18n`、`npm run check`、`npm run build`。瀏覽器案例 UI-01 至 UI-05 涵蓋偏好保存、分頁同步與畫面行為，詳見[前端測試](frontend-testing.md)。
