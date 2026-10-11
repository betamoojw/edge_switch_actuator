# 目前 dev 原始碼審查

於 **2026-10-11** 審查 [`bf8cb12`](https://github.com/betamoojw/edge_switch_actuator/tree/bf8cb12ab9ada4337751968332c7d5e17e056ccc)，韌體 **0.6.4**，當時遠端 `dev` 相符。之後的文件提交不改動韌體。本頁不是全面安全稽核，也不是新的硬體/ETS 資格驗證。[原設計](actuator-production-design.md)保留當時意圖，[架構](architecture.md)描述實作。

## 相較 10 月 10 日審查的更新 { #changes-since-the-10-october-review }

0.6.4 修正發行 API 儲存庫識別，側邊欄改連文件網站；預設映像更新為 0.6.4，另追蹤 `buildRelease/Edge_S3_Relay_6CH.knxprod`。與 `ced3e6d` 相比，後端 API 及致動器行為未變，詳見[版本更新](release-notes.md)。

[10 月 10 日原紀錄](source-review-2026-10-10.md)保留先前發現，含該次 C3 映像大小失敗，不能視為 0.6.4 新測結果。本次未更新韌體或主動測試硬體。

## 目前仍存在的問題 { #findings-still-present-at-the-reviewed-baseline }

| 優先級 | 原始碼依據 | 影響與後續方向 |
| --- | --- | --- |
| P1 | `FSPersistence.h` 用 `w` 開啟有效檔，讀失敗後寫預設值 | 網路/帳號/MQTT 設定可能因寫入中斷遺失；需改驗證提交並測試復原 |
| P1 | `UpdateIndicator.svelte`、`GithubFirmwareManager.svelte` 以 `.bin` 及板型子字串選檔 | 可能誤選合併映像或 MCP 變體；應精確比對目標及 `_ota.bin`，目前優先手動操作 |
| P2 | `build_interface.py` 只比 `interface/src/` 時間戳 | 靜態資源、鎖定檔、Vite 更新可能漏建；暫以刪除產生的 `WWWData.h` 強制重建 |
| P2 | `factory_settings.ini` 的 `Europe/Berlin` 配 `GMT0BST,M3.5.0/1,M10.5.0` | 標籤與時差不符，設定時請選一致時區 |
| P2 | `EventSocket.cpp` 取消訂閱以未驗證名稱作下標 | 已認證用戶端可新增空映射；需查找/註冊驗證及上限。尚未重現遠端資源耗盡 |
| P2 | EventSocket 僅在接入認證，後續收送不重查權杖/帳號 | 既有連線可能超過登入到期或帳號異動，需明確撤銷機制；唯讀 `device.state` 仍可能持續可見 |

這些是審查發現，文件更新未修正對應韌體。網址已修復，但檔案挑選問題仍可由原始碼看出，未實測 OTA。[實機紀錄](live-verification.md)另發現 KNX 三擊提示可能與儲存指派不同，以及 0.6.3 實機無 MCP 選單；未確認映像身份，不能認定是目前程式缺陷。

## 自範本階段完成的改進 { #implemented-improvements-since-the-template-review }

- 啟動網路前先設安全 GPIO，由獨立致動器任務接收有界 HTTP/整合命令；產品排除示範燈程式。
- `NetworkSupport` 依選定 IPv4 上行判斷，排除僅 AP，介面變更後重啟監聽。
- REST 驗證角色與通道遮罩，狀態事件無寫回呼；通用 `EventEndpoint<T>` 不是角色授權層。
- NVS 專屬設定身份、加鹽 PBKDF2、API 遮蔽密碼與簽章秘密；權杖綁定啟動、最長八小時，帳號編輯會輪換密鑰。
- DurableStore 支援驗證、多代、KNX 分塊與復原；掛載保留損壞資料，重設使用持久標記。
- KNX 位址/表格/參數提交有回復、獨立修訂及持久擁有權，產品參數有契約檢查。
- 訂閱修改採互斥與去重，透過 HTTP 任務執行有界發送。
- MCP 提供 WSS 驗證、資料遮蔽、有界解析/佇列、開放遮罩、脈衝去重及 OTA 協調，與 MQTT 無相依。
- 韌體從本次編譯輸出封裝，CI 檢查實際分割區容量。

## 操作及資格驗證範圍 { #operational-and-qualification-limits }

管理服務仍為 HTTP；Modbus 沒有認證或加密，MQTT 由代理授權。通道遮罩不是通用框架權限。未建立 Flash 加密或韌體簽章保證；OTA 略過憑證選項實際並未啟用。

RTU 具 UART 閒置事件與 CRC，卻不嚴格拒絕每一個超過 1.5 字元的訊框內間隔。KNX 套件生成不等於 ETS 匯入、完整/部分下載、產品登錄或認證。啟動電氣表現、接點壽命、RS485 轉向、實機 TLS 拒絕、負載下資源及長期復原，都需依部署條件另行驗證。

部分相依項目仍使用版本範圍。內嵌檔案的新鮮度限制及已追蹤二進位，表示不能假定產物與後續程式一致，須重新建置並核對雜湊。

## 文件改善 { #documentation-defects-corrected-by-this-refresh }

現行指南以致動器為主，補上 0.6.4 網址/映像說明、Svelte 5 元件用法與三語文件。嚴格建置、翻譯覆蓋及連結檢查配合既有 `dev` Pages 工作流程。歷史資料保留原日期與範圍，不當作本次重新驗證，詳見[驗證紀錄](validation.md)。
