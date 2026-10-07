"""Verify that archive preflight distinguishes metadata from registration trust."""

import io
import runpy
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
inspect_package = runpy.run_path(str(ROOT / "scripts/check_knx_import.py"))["inspect_package"]


class ImportPreflight(unittest.TestCase):
    def test_checked_in_package_requires_external_review(self):
        report = inspect_package(ROOT / "knx/Edge_S3_Relay_6CH.knxprod")
        self.assertTrue(report["externalReviewRequired"])
        self.assertEqual(report["registrationRecords"], 2)
        self.assertEqual(report["applications"][0]["manufacturerName"], "KNX Association")
        self.assertEqual(report["applications"][0]["importRestriction"], "Own")

    def test_registration_metadata_is_not_treated_as_authenticated(self):
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as archive:
            archive.writestr("product.xml", '<KNX><ApplicationProgram Id="M-0001_A-0001"/>'
                              '<RegistrationInfo/></KNX>')
        data.seek(0)
        report = inspect_package(data)
        self.assertEqual(report["registrationRecords"], 1)
        self.assertTrue(report["externalReviewRequired"])
        self.assertFalse(report["signatureTrustVerified"])
        self.assertFalse(report["etsLicenceVerified"])

    def test_corrupt_archive_is_rejected(self):
        with self.assertRaises(zipfile.BadZipFile):
            inspect_package(io.BytesIO(b"not a KNX archive"))

    def test_missing_application_is_rejected(self):
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as archive:
            archive.writestr("knx_master.xml", "<KNX><RegistrationInfo/></KNX>")
        data.seek(0)
        with self.assertRaisesRegex(ValueError, "No application"):
            inspect_package(data)


if __name__ == "__main__":
    unittest.main()
