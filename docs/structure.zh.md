# 前端结构

生产界面位于 `interface/src/`，采用 Svelte 5、SvelteKit 静态输出、Tailwind CSS 4 和 DaisyUI 5。浏览器通过 `/rest`、`/ws` 访问固件；ESP32 不运行 Svelte 服务端。

## 路由与模块 { #routes-and-modules }

| `interface/src/` 下的位置 | 职责 |
| --- | --- |
| `routes/+layout.ts`、`+layout.svelte` | 功能加载、产品元数据、登录、共用框架 |
| `routes/+page.ts`、`+page.svelte` | 根入口，转到 `/device` |
| `routes/device/` | 输出、指示器、按键、协议、KNX、维护 |
| `routes/connections/` | MQTT、NTP、小智 MCP |
| `routes/wifi/`、`routes/ethernet/` | 网络状态和设置，以太网受功能标志控制 |
| `routes/user/` | 管理员用户管理 |
| `routes/system/` | 偏好、状态、指标、核心转储、升级 |
| `routes/demo/` | 模板演示，不是产品启动页 |
| `routes/menu.svelte`、`statusbar.svelte`、`login.svelte` | 导航、状态栏、登录 |
| `lib/device/types.ts`、`reconcile.ts`、`knx-address.ts` | 数据契约、部分状态合并、KNX 地址校验 |
| `lib/stores/` | 用户、套接字、偏好及遥测 |
| `lib/i18n/` | 翻译源文件、生成消息和响应式 API |
| `lib/components/` | 共用控件、设置卡片、对话框、通知 |
| `lib/types/models.ts` | 框架共用模型 |

## 功能与权限 { #features-and-permissions }

布局将 `/rest/features` 加载到 `page.data.features`，菜单据此显示编译功能。执行器状态/配置还返回当前用户能力。隐藏控件不是授权检查，固件在修改前执行角色及通道掩码校验。

0.6.4 使用 `github: 'betamoojw/edge_switch_actuator'`，发布组件需要 `owner/repository`，侧栏单独使用文档 URL。文件匹配限制仍存在，见[固件更新](buildprocess.md#updating-a-device)。

## 状态与交互 { #state-and-interaction }

使用 MessagePack 事件读取状态，以 REST 修改。已保存配置、编辑草稿和传入状态必须分开，后台遥测不能覆盖未保存表单。KNX 有独立修订号，保存返回已提交快照。所有控件，包括批量操作，都要保留禁用、锁定及通道权限语义。

`lib/device/reconcile.ts` 合并部分更新并保留已有嵌套值；`lib/stores/socket.ts` 管理订阅、心跳、重连及过期连接。生产 UI 不应添加模拟器专用分支，模拟器从外部提供同一契约。

## 扩展主菜单 { #customize-the-main-menu }

在 `+page.ts` 添加路由元数据，在 `routes/menu.svelte` 增加入口。标题使用翻译键，并与活动导航匹配；执行对应功能及权限检查。组件沿用共用设置/对话框模式，在显示边界调用 `$t()`。另见[偏好](ui-preferences.md)、[主题](sveltekit.md)、[前端测试](frontend-testing.md)。
