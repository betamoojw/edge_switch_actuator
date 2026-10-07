"""Read-only KNX archive preflight; does not validate signatures or ETS licences.

Exit 0: registration metadata present (not authenticated).
Exit 1: archive/XML failure. Exit 2: external licensing/registration review needed.
"""

import argparse
import json
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def inspect_package(path):
    with zipfile.ZipFile(path) as archive:
        bad = archive.testzip()
        if bad:
            raise ValueError(f"ZIP CRC failure: {bad}")
        documents = {
            name: ET.fromstring(archive.read(name))
            for name in archive.namelist() if name.endswith(".xml")
        }
    master = documents.get("knx_master.xml")
    manufacturers = {}
    if master is not None:
        manufacturers = {
            el.get("Id"): el.attrib for el in master.iter()
            if el.tag.rsplit("}", 1)[-1] == "Manufacturer"
        }
    products = []
    for name, root in documents.items():
        if name == "knx_master.xml":
            continue
        for el in root.iter():
            if el.tag.rsplit("}", 1)[-1] == "ApplicationProgram":
                identity = el.get("Id", "")
                manufacturer = identity.split("_A-", 1)[0]
                info = manufacturers.get(manufacturer, {})
                products.append({
                    "applicationId": identity,
                    "manufacturerId": manufacturer,
                    "manufacturerName": info.get("Name", "unknown"),
                    "importRestriction": info.get("ImportRestriction", "unspecified"),
                })
    if not products:
        raise ValueError("No application program found in archive")
    registrations = sum(
        el.tag.rsplit("}", 1)[-1] == "RegistrationInfo"
        for name, root in documents.items() if name != "knx_master.xml"
        for el in root.iter()
    )
    return {"archiveIntegrity": "passed", "applications": products,
            "registrationRecords": registrations,
            "externalReviewRequired": True,
            "signatureTrustVerified": False, "etsLicenceVerified": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", nargs="?", type=Path,
                        default=ROOT / "knx/Edge_S3_Relay_6CH.knxprod")
    args = parser.parse_args()
    try:
        report = inspect_package(args.package)
    except (OSError, ValueError, zipfile.BadZipFile, ET.ParseError) as error:
        print(f"ERROR: {error}")
        return 1
    print(json.dumps(report, indent=2))
    if report["registrationRecords"] == 0:
        print("No registration metadata found; ETS may require a testing entitlement. "
              "See knx/README.md.")
        return 2
    print("Registration metadata exists; signature trust and ETS acceptance remain unverified.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
