# 介面元件

共用元件位於 `interface/src/lib/components/`，將 DaisyUI 樣式與 Svelte 5 響應式互動、動畫整合。現行程式採 snippets 與回呼屬性，舊版命名 slot/`on:closed` 教學不再適用。

## Collapsible { #collapsible }

```svelte
<script lang="ts">
  import Collapsible from '$lib/components/Collapsible.svelte';
  let open = $state(false);
  function doSomething() {}
</script>
<Collapsible bind:open class="shadow-lg" closed={doSomething}>
  {#snippet title()}Title{/snippet}
  <p>Content</p>
</Collapsible>
```

`open` 可綁定，預設 false。`title`、`icon`、`children` 為 snippets；`class` 加樣式，`isDirty` 提醒未儲存。使用者切換後呼叫 `opened`/`closed`，不是 Svelte 4 自訂事件。容器本身不附背景或陰影。

## InputPassword { #inputpassword }

```svelte
<InputPassword id="pwd" bind:value={password} />
```

從 `$lib/components/InputPassword.svelte` 匯入，眼睛按鈕控制密碼顯示，沿用 DaisyUI 外觀。顯示真實密碼時不可擷取公開畫面。

## RSSIIndicator { #rssiindicator }

由 `$lib/components/RSSIIndicator.svelte` 匯入 `RssiIndicator`，負數 `rssi_dbm` 決定訊號圖樣；`showDBm` 預設 false，可開啟 dBm 標示，沒有 slots 或事件。

```svelte
<RssiIndicator showDBm={true} rssi_dbm={-85} class="text-base-content h-10 w-10" />
```

## SettingsCard { #settings-card }

由 `$lib/components/SettingsCard.svelte` 匯入。這是帶樣式的設定卡，使用 `title`、`icon`、`children` snippets；`open`、`collapsible` 預設 true，`maxwidth` 為 `max-w-2xl`，`isDirty` 預設 false，不提供開啟/關閉回呼。

```svelte
<SettingsCard collapsible={true} open={false}>
  {#snippet icon()}<Icon class="mr-2 h-6 w-6" />{/snippet}
  {#snippet title()}Title{/snippet}
  <p>Content</p>
</SettingsCard>
```

## Spinner { #spinner }

匯入 `$lib/components/Spinner.svelte`，等待資料時放入 `<Spinner />`，不需屬性、slot 或事件。

## 浮動通知 { #toast-notifications }

```ts
import { notifications } from '$lib/components/toasts/notifications';
notifications.info('Message', 3000);
```

提供 `notifications.error`、`.warning`、`.info`、`.success`，接受 `msg:string` 與毫秒 `timeout:number`。訊息當作純文字呈現，勿依賴 HTML 注入排版。

## 更新對話框 { #github-update-dialog }

顯示更新進度及錯誤，OTA 成功五秒後重新載入整頁。

## 更新指示器 { #update-indicator }

右上狀態列透過 GitHub Latest Release API 尋找新版，點擊可能啟動更新。0.6.4 修正了儲存庫網址，但資產比對仍有限制，先閱讀[映像選擇](buildprocess.md#updating-a-device)。若使用自訂更新服務，須配合修改 `UpdateIndicator.svelte` 的請求與契約。

## InfoDialog { #info-dialog }

匯入 `$lib/components/InfoDialog.svelte`，使用 `modals.open` 傳入 `title`、`message`、`dismiss: {label, icon}`。`onDismiss` 必須呼叫 `modals.close()`，可另加關閉後處理。

## ConfirmDialog { #confirm-dialog }

匯入 `$lib/components/ConfirmDialog.svelte`，提供 `title`、`message`、`labels: {cancel: {label, icon}, confirm: {label, icon}}`；`onConfirm` 負責關閉與執行已授權動作。兩者基於 [svelte-modals](https://svelte-modals.mattjennings.io/)。實際畫面請將 Title/Content/Message 換為翻譯鍵，讓取消與確認意義明確。
