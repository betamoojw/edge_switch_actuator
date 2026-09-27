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


if __name__ == "__main__":
    unittest.main()
