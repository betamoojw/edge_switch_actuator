# 启动入口与 KNX 地址配置

`/` 重定向到 `/device`，启用安全功能时仍需登录。标题和导航使用 Switching Actuator；`/demo` 仅供显式访问的模板开发。协议选择区域称为 **Protocol Interface**，先选 KNXnet/IP 并应用，再到 KNX 页调试。进入编程或应用配置均属主动操作。

## 个体地址 { #individual-address }

占位提示 `15.15.255 (Factory Default)` 不会覆盖已保存值。格式为 `area.line.device`：area、line 为 0–15，device 为 1–255；0 留给耦合器，本执行器拒绝。无效格式、缺项或越界会显示错误。须按拓扑分配唯一地址，浏览器不能检测其他设备的地址冲突。参考 [KNX 初始地址说明](https://support.knx.org/hc/en-us/articles/360011741860-Multiple-devices-with-the-same-Individual-Address)。

## 组地址 { #group-addresses }

提示为 `1/0/1, 1/0/2`。仅支持三级格式：主组 0–31、中组 0–7、子组 0–255，`0/0/0` 是广播，不能关联。使用逗号分隔，第一个是发送地址，留空表示无关联。每个对象最多八个唯一地址，重复数值、空条目和格式错误均拒绝；不支持二级或自由格式。固件还检查全局表容量和所有权，服务端结果为准。参见 [KNX 地址范围](https://support.knx.org/hc/en-us/articles/115001825304-Group-Address-Ranges)。

## ETS 下载与网页所有权：截图说明 { #ets-download-and-web-ownership-screenshot-walkthrough }

以下图片于 **2026 年 10 月 11 日**加入文档，显示英语网页及 ETS 界面；拍摄日期、固件提交和 ETS 版本未提供。本次没有执行下载或配置修改。图片中的地址属于示例安装，不是可直接套用的默认值。

### 所有权改变之前 { #before-the-ownership-change }

三张 KNX 图片中的私有网络地址和无关浏览器书签已用不透明遮罩覆盖，调试控件与结果保持原样。KNX 个体地址和组地址保留为协议示例值。

[![ETS 正在下载或重启；网页显示 Programming ON、web 所有权、修订 2、地址 1.2.1。](with_ets_download_before.png)](with_ets_download_before.png)

`with_ets_download_before.png` 已处于下载/重启过程，不是闲置的下载前画面。右侧显示 **Owner: web · Revision 2 · Ready**；ETS 列出六路 Switch、Block、Status 对象。地址为 `1.2.1`，通道 1 可见 Switch `8/0/0`、Status `8/1/0`。ETS 下载中不要编辑网页配置。

### 所有权改变之后 { #after-the-ownership-change }

[![ETS 仍显示下载中，网页已报告 ets 所有权、修订 3，地址仍为 1.2.1。](with_ets_download_after.png)](with_ets_download_after.png)

`with_ets_download_after.png` 显示 **Owner: ets · Revision 3 · Ready**，地址和通道 1 关联未变。但 ETS 仍显示 **Downloading**，不能根据文件名或 Ready 判断下载成功。应等待 ETS 最终结果，再读回地址、关联和应用参数，保留结果及镜像身份作为验收证据。

### 接管网页编辑 { #taking-over-for-web-editing }

[![网页勾选 Take over for web editing，但仍为 ETS 所有权、修订 3，并出现 HTTP 401 和控件禁用提示。](knx_config_web_edit.png)](knx_config_web_edit.png)

勾选接管仅表示保存时请求接管，不会立即转移所有权。之后的 ETS 下载可以覆盖网页修改，须协调 ETS 项目和本地编辑。

该图显示 **Connection unavailable. Controls are disabled. Error: Request failed (401)**，是认证异常示例，不是保存成功证据。重新登录并加载最新快照，断连页面残留值可能过期。

安装人员或管理员获授权后的流程：

1. 等待 ETS 操作结束，确认 KNX 活动且无下载。
2. 读取最新所有者和 KNX 修订号。
3. 选择 **Take over for web editing**，完成有效修改，执行 **Apply KNX commissioning**。
4. 检查成功响应，读回已提交配置和 web 所有权；冲突时重新加载并协调。

固件拒绝下载期间的网页保存，ETS 所有的配置必须明确接管。参见 [REST 修订](restfulapi.md#revisions-and-retries)和[验收](commissioning.md)。截图不证明继电器动作、完整 ETS 互操作或 KNX 认证。

## 历史浏览器验证 { #historical-browser-validation }

原更新记录：HOME-01 和 KNX-04 覆盖启动入口、占位、无效边界、保留/重复地址及有效极值保存；当时 39 个生产构建 Chromium 场景通过，两项重点用例在 Chromium、WebKit、移动 Chromium 的六项检查通过，类型检查零错误警告，构建、模拟器标记排除和格式检查通过。这些是历史软件证据，本次未重新执行，未涵盖物理 KNX 调试。
