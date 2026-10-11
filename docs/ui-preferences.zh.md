# 界面偏好设置

在 **System → UI Preferences** 选择语言和主题，修改会立即应用并保存在当前浏览器、当前设备来源下。无需保存到固件或重启；同源标签页会同步，登录页也会沿用。它不是用户账号配置，换浏览器、地址或来源后不会自动迁移。

## 语言与主题

界面支持 English、Deutsch、Español、Français、Polski、简体中文、繁體中文。翻译随固件打包，可离线使用；原始协议错误和诊断文本不保证翻译。文档网站提供三种语言，与设备界面的语言设置相互独立。

主题包括 light、dark、nord、dim、sepia；Auto 跟随操作系统。图表也应采用当前主题的颜色，避免硬编码。

## 开发维护

编辑翻译 TSV，再运行 `npm run i18n:build` 生成消息 JSON。各语言须保持完整键集合及同名插值参数。显示文字使用 `$t`，日期、数字和时长使用当前 locale 的 `Intl` 格式；不要翻译 API 字段、协议值或配置键。

在 `interface/` 运行 `npm run test:i18n`、`npm run check` 和 `npm run build`。浏览器用例 UI-01 至 UI-05 检查偏好保存、标签页同步及界面行为，详见[前端测试](frontend-testing.md)。
