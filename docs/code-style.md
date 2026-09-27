# Source style and maintenance

C++ uses `.clang-format`: Microsoft-based formatting with four spaces, Allman braces, explicit control-flow braces and a 140-column target. Python uses `ruff.toml` with conventional four-space formatting, a 140-column target and import/lint checks. Svelte and TypeScript retain the existing interface Prettier style: tabs, single quotes and a 100-column target. `requirements-dev.txt` pins the Python-distributed formatters; install them in a development virtual environment.

Format handwritten code, not generated `WWWData.h`, generated KNX headers/XML, packaged products, build outputs or third-party dependencies. Update a generator when its output needs changing. Keep protocol addresses and persisted field names stable during readability refactors.

Typical commands (from the repository root unless noted):

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

Shell wildcard expansion varies; pass explicit file paths when the shell does not expand these patterns. Use `clang-format --dry-run --Werror`, `ruff format --check`, and `prettier --check` for verification.

The actuator owns hardware state and relay GPIO writes. Immutable pins belong in `src/device/BoardProfile.h`; protocol adapters call the owner to disable configured channels. HTTP registration and dispatch live in `ActuatorApi.cpp`, with the runtime loop in `Actuator.cpp`. Dashboard API shapes live in `interface/src/lib/device/types.ts`. Python commands expose guarded `main()` entry points; KNX generation separates parameter, object and product construction from file writes.

After behavior-sensitive refactors, run `python scripts/test_native.py` and build both the actuator and generic S3 environments. Verify generated KNX artifacts remain unchanged when the product contract has not changed. Hardware and ETS qualification remain separate from host tests and compilation.
