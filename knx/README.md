# Edge S3 Relay 6CH — production KNX package

Regenerated on 2026-10-07 with OpenKNXproducer 4.3.5.0 using the production profile and OpenKNX template registration metadata.

| Setting | Value |
| --- | --- |
| Product / catalog | Edge S3 Relay 6CH / OpenKNX |
| Order number | EDGE-S3-6CH |
| Profile / developmentOnly | production / false |
| Manufacturer / application / version | 250 (M-00FA) / 600 / 1 |
| Mask / medium | MV-57B0 / MT-5 (IP) |
| Channels / parameter bytes / objects | 6 / 52 / 18 |
| Declared registration status | Registered |
| Registration mode / template number | openknx-template / 0001/11 |

## Registration metadata

The previous package omitted registration records and ETS 6.2.2 reported “No valid license found to test the unregistered product(s) in this file”. Missing records are a likely cause; the earlier documentation was too categorical in saying only an external licence/registration action could help.

The generator now follows the [OpenKNXproducer template](https://github.com/OpenKNX/OpenKNXproducer/blob/main/TemplateApplication.xml): Product contains RegistrationInfo with RegistrationStatus="Registered"; Hardware2Program contains the same status and RegistrationNumber="0001/11" after its application reference. OpenKNXproducer added a RegistrationSignature to both packaged records.

[OpenKNX's hardware signer](https://github.com/OpenKNX/OpenKNX.Toolbox.Sign/blob/main/HardwareSigner.cs) uses installed ETS signing libraries with the knxconv registration key, including handling for ETS 6.2 and later. Rebuild through the producer after source changes; do not edit the signed archive manually.

These are OpenKNX-style package declarations, not evidence of KNX Association registration or certification issued for this actuator. The number comes from the template, not a device-specific assignment. OpenKNX endorsement or application allocation has not been established. Master data names M-00FA as KNX Association; this numeric identity already matched the OpenKNX template.

## Files and build

- product-model.json: source of product identity, production labels and explicit registration mode.
- Edge_S3_Relay_6CH.xml: generated producer input with template IDs.
- Edge_S3_Relay_6CH.knxprod: rebuilt production package for ETS import testing.
- Edge_S3_Relay_6CH.h: generated parameter accessors.

Run from the repository root:

```powershell
python scripts/generate_knx_product.py
& ./.pio/knx-producer/OpenKNXproducer.exe create knx/Edge_S3_Relay_6CH.xml
python tests/product_contract_test.py
python tests/knx_import_test.py
python scripts/check_knx_import.py
```

Archive size: **106,789 bytes**. SHA-256: `ecd52e438df7afc99a34ed535ce97cb7dbffdf8f12b22ecc3211ea76e5477cd1`.

Producer sanity checks passed. The producer could not find its XSD, so full schema validation remains unavailable. Contract tests check source/package identity, parameters, objects, labels, and both registration records including presence of generated signatures. Signature presence is not cryptographic trust verification.

Preflight exit 0 means registration metadata exists, not that ETS has accepted it. Exit 1 indicates archive/XML failure; exit 2 indicates absent metadata. External review remains required because preflight does not authenticate signatures or verify ETS licences.

## Verification in ETS

Import the regenerated package into ETS 6.2.2 and check whether the original licence prompt is resolved. Then check the six-channel parameters and 18 objects and perform a device download/function test in a controlled setup. If import fails, retain the exact new error for diagnosis.

Firmware identity and parameter layout are unchanged; this packaging change does not require firmware flashing. Successful ETS import and commissioning have not yet been verified.
