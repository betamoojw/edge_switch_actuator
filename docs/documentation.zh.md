# 文档维护与发布

站点位于 [GitHub Pages](https://betamoojw.github.io/edge_switch_actuator/)，源码为 **dev** 的 `docs/`，配置 `mkdocs.yml`。即使默认分支是 main，文档仍描述 dev。

## 预览与验证 { #preview-and-validate }

在仓库根目录的 Python 环境执行：

```sh
python -m pip install -r requirements-docs.txt
python -m mkdocs serve
```

打开输出的 localhost。发布前运行：

```sh
python -m mkdocs build --strict
python scripts/check_docs.py
```

输出 `site/` 已被 Git 忽略。内部页面、文件、锚点警告会使严格构建失败；MkDocs 不联网验证外部 GitHub 链接，移动源码时应另查。`docs/` 之外的文件用明确 GitHub 链接，不假定被复制。

## 自动发布 { #automatic-publication }

`.github/workflows/ci.yaml` 的 **Documentation** 工作流：相关文档/配置推送 dev 后严格构建并检查；PR 只验证不部署；成功的 dev 构建只上传 `site/`，部署任务用 `github-pages` 环境和官方 Actions，只有该任务有 `pages: write`、`id-token: write`。

仓库 **Settings → Pages → Build and deployment → Source** 选 **GitHub Actions**，环境允许 dev。不要选 `main:/docs`，Markdown 需要渲染，旧 gh-pages 分支也不是此流程来源。

支持 `workflow_dispatch`；GitHub 的手动运行 UI 要求默认分支也有该工作流，未同步 main 时以 dev 文档推送触发，失败可在运行页重试。发布后核对部署 URL、三语首页、嵌套页、搜索及导航；构建成功不等于部署或公开访问成功。

官方说明：[GitHub 自定义 Pages 工作流](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)、[MkDocs 部署](https://www.mkdocs.org/user-guide/deploying-your-docs/)。

## 内容更新 { #updating-the-content }

源码是当前行为依据。升级基线时同步首页、README、审查中的提交/版本，记录实际检查，历史结果保留原日期及限制。新当前页面加入导航及三语清单。`docs/tasks/` 保留原文历史，但私有端点仍需脱敏。

此站发布所有纳入 `docs/` 的文件，不只是导航页面；不得放令牌、真实设置二维码、配置导出或设备秘密。

## 语言与翻译维护 { #languages-and-translation-maintenance }

英语在根路径，简体 `/zh/`，繁体 `/zh-TW/`。`mkdocs-static-i18n` 使用 `guide.md`、`guide.zh.md`、`guide.zh-TW.md`，Material 选择器跳到等价页面。`mkdocs.yml` 翻译导航，必要时用稳定显式锚点保持链接。Material 中文搜索配合固定的 `jieba`。

`docs-languages.json` 明列当前与历史；`scripts/docs_hook.py` 发现任何当前译文缺失会中止构建。历史回退显示当前语言的原文提示，不能替代当前译文。

简体使用自然的大陆技术表达，繁体使用适合台湾读者的术语，不能只做字形转换。标识符/可执行示例保持一致，解释、图注、替代文本、图表标签本地化。截图标语言、版本及偏差。人工审阅内容完整性和流畅度，不能仅看文件存在。自动检查覆盖生成链接、锚点、图片替代文字、选择器和索引，浏览器仍须验证搜索结果、语言切换、图表及桌面/移动布局。
