# SvelteKit 与主题

项目用 `@sveltejs/adapter-static` 构建静态浏览器应用。`routes/+layout.ts` 设置 `ssr = false`、`prerender = false`；ESP32 提供静态文件，C++ 实现 REST/WebSocket 后端。

## 仅浏览器应用 { #browser-only-application }

使用客户端路由与共用组件；SvelteKit 服务端端点或 `+page.server.ts` 不能替代固件 API。可能在构建时求值的模块应把浏览器专属对象访问放入适当生命周期。

`npm run dev:sim` 启动隔离模拟环境；设置 `DEVICE_HOST` 后运行 `npm run dev:device` 可通过 Vite 代理设备。生产构建不需要目标地址。参见[前端开发](frontend-testing.md)。

## 应用名称和身份 { #changing-the-app-name }

网页产品元数据位于 `routes/+layout.ts`，静态图标等资源在 `interface/static/`。0.6.4 修正 GitHub 标识为 `betamoojw/edge_switch_actuator`；浏览链接和发布 API 的 `owner/repository` 应分开。

固件身份来自 `platformio.ini` 的 `APP_NAME`、`APP_VERSION`、`BUILD_TARGET`，不同于显示名称及发布文件前缀 `edge_switch_actuator`。

## 主题与本地化 { #themes-and-localization }

在 **System → UI** 选择 Light、Dark、Nord、Dim、Sepia，Automatic 按系统切换明暗。七种语言本地打包；偏好按浏览器设备来源保存，同源标签页及登录页同步。

维护 `app.css` 中的主题、`lib/stores/preferences.ts` 中的偏好逻辑，以及 `lib/i18n/translations.tsv`。运行 `npm run i18n:build` 生成消息，协议值、地址和用户输入名称不翻译。[偏好指南](ui-preferences.md)说明完整流程。图表使用 `lib/DaisyUiHelper.ts`、`lib/chart-preferences.ts` 和共用主题变量，不另设硬编码配色。

## 验证 { #validation }

在 `interface/` 运行 `npm run test:i18n`、`npm run check`、`npm run build`、`npm run test:bundle`。交互变化还需模拟器和浏览器回归；涉及固件契约或物理行为时仍须原生及硬件测试。
