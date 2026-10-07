"""Check the generated ETS/application binary layout and packaged identity."""

import json
import unittest
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NS = {"k": "http://knx.org/xml/project/20"}


class ProductContract(unittest.TestCase):
    def setUp(self):
        self.model = json.loads((ROOT / "knx/product-model.json").read_text())
        self.xml = ET.parse(ROOT / "knx/Edge_S3_Relay_6CH.xml")

    def test_objects_and_types(self):
        objects = self.xml.findall(".//k:ComObject", NS)
        self.assertEqual([int(x.get("Number")) for x in objects], list(range(1, 19)))
        for number, obj in enumerate(objects, 1):
            self.assertEqual(obj.get("DatapointType"), ["DPST-1-2", "DPST-1-1", "DPST-1-3"][number % 3])
            self.assertEqual(obj.get("WriteFlag"), "Disabled" if number % 3 == 0 else "Enabled")

    def test_parameter_layout(self):
        offsets = [int(x.get("Offset")) for x in self.xml.findall(".//k:Parameter/k:Memory", NS)]
        self.assertEqual(offsets, [0, 1] + [4 + 8 * c + p for c in range(6) for p in (0, 1, 2, 4, 6)])
        self.assertEqual(int(self.xml.find(".//k:RelativeSegment", NS).get("Size")), 52)

    def test_packaged_identity(self):
        with zipfile.ZipFile(ROOT / "knx/Edge_S3_Relay_6CH.knxprod") as archive:
            self.assertIsNone(archive.testzip())
            candidates = [name for name in archive.namelist() if "_A-" in name and name.endswith(".xml")]
            self.assertEqual(len(candidates), 1)
            app = ET.fromstring(archive.read(candidates[0])).find(".//k:ApplicationProgram", NS)
            self.assertEqual(int(app.get("ApplicationNumber")), self.model["applicationNumber"])
            self.assertEqual(int(app.get("ApplicationVersion")), self.model["applicationVersion"])
            self.assertEqual(app.get("MaskVersion"), self.model["maskVersion"])
            manufacturer = ET.fromstring(archive.read(candidates[0])).find(".//k:Manufacturer", NS)
            self.assertEqual(manufacturer.get("RefId"), f"M-{self.model['manufacturerId']:04X}")

    def test_packaged_product_labels_and_registration(self):
        with zipfile.ZipFile(ROOT / "knx/Edge_S3_Relay_6CH.knxprod") as archive:
            roots = [ET.fromstring(archive.read(n)) for n in archive.namelist()
                     if n.endswith(".xml") and n != "knx_master.xml"]
            products = [p for root in roots for p in root.findall(".//k:Product", NS)]
            self.assertEqual(len(products), 1)
            self.assertEqual(products[0].get("Text"), self.model["name"])
            self.assertEqual(products[0].get("OrderNumber"), self.model["orderNumber"])
            catalogs = [p for root in roots for p in root.findall(".//k:CatalogSection", NS)]
            self.assertEqual(catalogs[0].get("Name"), self.model["catalogName"])
            if self.model["registrationStatus"] == "Unregistered":
                self.assertFalse([p for root in roots for p in root.findall(".//k:RegistrationInfo", NS)])
            else:
                for root in (self.xml, *roots):
                    for parent in root.findall(".//k:Product", NS) + root.findall(".//k:Hardware2Program", NS):
                        registration = parent.find("k:RegistrationInfo", NS)
                        self.assertIsNotNone(registration)
                        self.assertEqual(registration.get("RegistrationStatus"), "Registered")
                        if parent.tag.endswith("Hardware2Program"):
                            self.assertEqual(registration.get("RegistrationNumber"), self.model["registrationNumber"])
                        if root is not self.xml:
                            self.assertTrue(registration.get("RegistrationSignature"))

    def test_packaged_parameters_match_source(self):
        with zipfile.ZipFile(ROOT / "knx/Edge_S3_Relay_6CH.knxprod") as archive:
            name = next(n for n in archive.namelist() if "_A-" in n and n.endswith(".xml"))
            packaged = ET.fromstring(archive.read(name))
            def layout(root):
                return [
                    (p.get("Name"), p.get("Value"), dict(p.find("k:Memory", NS).attrib, CodeSegment="segment"))
                    for p in root.findall(".//k:Parameter", NS)
                ]
            self.assertEqual(layout(packaged), layout(self.xml))
            for tag in ("RelativeSegment", "AddressTable", "AssociationTable", "LdCtrlWriteRelMem"):
                source = self.xml.find(f".//k:{tag}", NS)
                actual = packaged.find(f".//k:{tag}", NS)
                self.assertIsNotNone(actual)
                self.assertEqual(
                    {k: v for k, v in actual.attrib.items() if k != "Id"},
                    {k: v for k, v in source.attrib.items() if k != "Id"},
                )

    def test_packaged_objects_match_source(self):
        with zipfile.ZipFile(ROOT / "knx/Edge_S3_Relay_6CH.knxprod") as archive:
            name = next(n for n in archive.namelist() if "_A-" in n and n.endswith(".xml"))
            packaged = ET.fromstring(archive.read(name))
            def objects(root):
                return [
                    {k: v for k, v in obj.attrib.items() if k != "Id"}
                    for obj in root.findall(".//k:ComObject", NS)
                ]
            self.assertEqual(objects(packaged), objects(self.xml))


if __name__ == "__main__":
    unittest.main()
