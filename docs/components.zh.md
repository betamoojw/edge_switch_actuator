# 界面组件

共用组件位于 `interface/src/lib/components/`，结合 DaisyUI 与 Svelte 5 的响应式交互及动画。当前源码采用 snippets 和回调属性，不应继续使用旧教程的命名 slot/`on:closed`。

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

`open` 可绑定，默认 false；`title`、`icon`、`children` 为 snippets，`class` 额外样式，`isDirty` 标记未保存修改。用户切换后调用 `opened` 或 `closed` 回调，不派发 Svelte 4 事件。组件默认没有背景或阴影装饰。

## InputPassword { #inputpassword }

```svelte
<InputPassword id="pwd" bind:value={password} />
```

从 `$lib/components/InputPassword.svelte` 导入，眼睛按钮切换可见性，沿用 DaisyUI 样式。显示真实密码时勿截取公开图片。

## RSSIIndicator { #rssiindicator }

从 `$lib/components/RSSIIndicator.svelte` 导入 `RssiIndicator`，以负数 `rssi_dbm` 决定信号图标，`showDBm` 默认 false，可显示 dBm 数值，无 slots 或事件。

```svelte
<RssiIndicator showDBm={true} rssi_dbm={-85} class="text-base-content h-10 w-10" />
```

## SettingsCard { #settings-card }

从 `$lib/components/SettingsCard.svelte` 导入。带样式的设置卡片用 `title`、`icon`、`children` snippets；`open`、`collapsible` 默认 true，`maxwidth` 默认 `max-w-2xl`，`isDirty` 默认 false，没有打开/关闭回调。

```svelte
<SettingsCard collapsible={true} open={false}>
  {#snippet icon()}<Icon class="mr-2 h-6 w-6" />{/snippet}
  {#snippet title()}Title{/snippet}
  <p>Content</p>
</SettingsCard>
```

## Spinner { #spinner }

从 `$lib/components/Spinner.svelte` 导入，加载时使用 `<Spinner />`，无需属性、slot 或事件。

## 通知 { #toast-notifications }

```ts
import { notifications } from '$lib/components/toasts/notifications';
notifications.info('Message', 3000);
```

支持 `notifications.error`、`.warning`、`.info`、`.success`，参数 `msg:string` 和 `timeout:number`（毫秒）。消息按纯文本显示，不依靠 HTML 注入。

## 更新对话框 { #github-update-dialog }

显示更新进度和错误，OTA 成功五秒后整页刷新。

## 更新指示器 { #update-indicator }

状态栏右上角通过 GitHub Latest Release API 查找新版，点击可能启动更新。0.6.4 已修复仓库 URL，但资产匹配仍有风险，使用前看[镜像选择](buildprocess.md#updating-a-device)。自定义更新服务器时修改 `UpdateIndicator.svelte` 对应请求与契约。

## InfoDialog { #info-dialog }

导入 `$lib/components/InfoDialog.svelte`，经 `modals.open` 传 `title`、`message`、`dismiss: {label, icon}`。`onDismiss` 回调必须调用 `modals.close()`，也可执行关闭后的处理。

## ConfirmDialog { #confirm-dialog }

导入 `$lib/components/ConfirmDialog.svelte`，传 `title`、`message`、`labels: {cancel: {label, icon}, confirm: {label, icon}}`，`onConfirm` 负责关闭及获授权操作。两种模态框基于 [svelte-modals](https://svelte-modals.mattjennings.io/)。实际界面将示例 Title/Content/Message 替换为翻译键，保持取消与确认清晰。
