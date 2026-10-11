# 文件維護與發佈

公開站位於 [GitHub Pages](https://betamoojw.github.io/edge_switch_actuator/)，來源為 **dev** 的 `docs/`，設定在 `mkdocs.yml`。儲存庫預設分支雖是 main，文件仍對應 dev。

## 預覽及驗證 { #preview-and-validate }

在儲存庫根目錄的 Python 環境執行：

```sh
python -m pip install -r requirements-docs.txt
python -m mkdocs serve
```

開啟終端顯示的 localhost，發佈前執行：

```sh
python -m mkdocs build --strict
python scripts/check_docs.py
```

產出 `site/` 已由 Git 忽略。內部頁面、檔案及錨點警告會讓嚴格建置失敗。外部 GitHub 連結不由 MkDocs 連網查驗，移動程式時另行確認。`docs/` 外檔案需明確 GitHub 連結，不會自動複製。

## 自動發佈 { #automatic-publication }

`.github/workflows/ci.yaml` 的 **Documentation** 在相關文件/設定推送 dev 後嚴格建置並檢查，PR 僅驗證。成功的 dev 作業只上傳 `site/`，部署透過 `github-pages` 環境及官方 Actions，`pages: write`、`id-token: write` 僅授予部署工作。

於 **Settings → Pages → Build and deployment → Source** 選 **GitHub Actions**，環境需允許 dev。不要選 `main:/docs`，Markdown 必須建置；舊 gh-pages 分支也不是此流程來源。

亦支援 `workflow_dispatch`；GitHub 手動執行介面要求預設分支有可派送工作流程，main 尚未同步時，正常方式是推送 dev 文件。失敗可從執行頁重跑。完成後查驗部署 URL、三語首頁、內頁、搜尋及導覽；建置成功不代表已部署或公開服務成功。

官方參考：[GitHub Pages 自訂工作流程](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)、[MkDocs 部署](https://www.mkdocs.org/user-guide/deploying-your-docs/)。

## 維護內容 { #updating-the-content }

目前行為以原始碼為準。更新基準時同步首頁、README、審查的提交與版本，留下實際檢查紀錄；歷史結果保留原日期及限制。新增現行頁須加入導覽與三語清單。`docs/tasks/` 可保留原文，但私有端點仍要去識別。

網站會發佈所有納入的 `docs/` 檔案，不只導覽中顯示者。不得放入權杖、真實設定 QR、組態匯出或裝置秘密。

## 語言及翻譯維護 { #languages-and-translation-maintenance }

英文在根目錄，簡體為 `/zh/`，繁體為 `/zh-TW/`。`mkdocs-static-i18n` 使用 `guide.md`、`guide.zh.md`、`guide.zh-TW.md`，Material 選擇器連到對應頁。導覽翻譯在 `mkdocs.yml`，需要時指定穩定錨點，避免跨語連結失效。Material 中文搜尋搭配固定版本 `jieba`。

`docs-languages.json` 區分現行與歷史，`scripts/docs_hook.py` 發現現行翻譯缺漏即中止。歷史回退有當地語言的原文提示，不能充當已完成翻譯。

簡體採自然的大陸技術用語，繁體採適合臺灣讀者的表達，不得僅轉換字形。識別值與可執行範例維持一致，說明、圖說、替代文字及圖表標籤在地化。圖片註明語言、版本及已知落差。審閱完整性和流暢度，不能只檢查檔案存在。自動檢查涵蓋連結、錨點、替代文字、選擇器及搜尋索引；仍須用瀏覽器查搜尋結果、語言切換、圖表及桌面/行動排版。
