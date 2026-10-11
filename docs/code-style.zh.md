# 代码规范与维护

C++ 使用 `.clang-format`：Microsoft 基础、四空格、Allman 大括号、控制流显式大括号、目标 140 列。Python 按 `ruff.toml` 使用四空格、140 列以及导入/静态检查。Svelte/TypeScript 沿用 Prettier：制表符、单引号、100 列。开发虚拟环境安装 `requirements-dev.txt` 中固定的格式化工具。

只格式化手写代码，不修改生成的 `WWWData.h`、KNX 头文件/XML、产品包、构建结果或第三方依赖；需要变更生成结果时改生成器。可读性重构不要改变协议地址和持久化字段名。

在仓库根目录执行，前端命令除外：

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

Shell 不展开通配符时应列出具体文件。只验证可用 `clang-format --dry-run --Werror`、`ruff format --check`、`prettier --check`。

执行器拥有硬件状态及 GPIO 写权限，不可变引脚放在 `src/device/BoardProfile.h`；协议适配器调用所有者禁用通道。HTTP 注册/分派在 `ActuatorApi.cpp`，运行循环在 `Actuator.cpp`，网页契约在 `interface/src/lib/device/types.ts`。Python 使用受保护的 `main()` 入口；KNX 生成将参数、对象、产品构造与文件写入分开。

行为敏感重构后运行 `python scripts/test_native.py`，构建执行器和通用 S3，产品契约未改时确认 KNX 生成物无变化。主机测试和编译不能代替硬件或 ETS 验证。
