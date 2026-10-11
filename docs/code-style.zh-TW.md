# 程式碼規範與維護

C++ 依 `.clang-format` 採 Microsoft 基礎、四空格、Allman 大括號、控制流程明確加括號，目標 140 欄。Python 依 `ruff.toml` 採四空格、140 欄與匯入/靜態檢查。Svelte/TypeScript 沿用 Prettier 的定位字元、單引號及 100 欄。請在開發虛擬環境安裝 `requirements-dev.txt` 固定版本的格式工具。

僅整理手寫程式，勿格式化產生的 `WWWData.h`、KNX 標頭/XML、產品套件、編譯輸出或第三方程式；需變更產物時修改產生器。可讀性重構應維持通訊位址及持久欄位名稱。

下列由儲存庫根目錄執行，前端部分再切換目錄：

```text
python -m pip install -r requirements-dev.txt
clang-format -i src/device/*.cpp src/device/*.h src/protocols/*.cpp src/protocols/*.h src/main.cpp tests/native_tests.cpp
ruff format scripts/generate_knx_product.py scripts/build_manifest.py scripts/test_native.py tests/product_contract_test.py
ruff check scripts/generate_knx_product.py scripts/build_manifest.py scripts/test_native.py tests/product_contract_test.py
cd interface
npx prettier --write src/routes/device src/lib/device
npm run check
npm run build
```

若 Shell 不展開萬用字元，請明列檔案。僅檢查時使用 `clang-format --dry-run --Werror`、`ruff format --check`、`prettier --check`。

致動器負責硬體狀態及 GPIO 寫入，固定腳位放在 `src/device/BoardProfile.h`，通訊配接層須呼叫擁有者停用通道。HTTP 註冊/派送在 `ActuatorApi.cpp`，迴圈在 `Actuator.cpp`，前端契約在 `interface/src/lib/device/types.ts`。Python 指令使用受保護的 `main()` 入口；KNX 產生器將參數、物件與產品組裝和檔案寫入分開。

影響行為的重構後，執行 `python scripts/test_native.py` 並編譯致動器及一般 S3。產品契約未變時，確認 KNX 產物一致。主機測試與編譯不取代實機或 ETS 驗證。
