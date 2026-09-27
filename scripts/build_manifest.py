"""Record hashes for built artifacts; run after the validation commands."""

import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

root = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    paths = [root / "knx/Edge_S3_Relay_6CH.knxprod"]
    paths += sorted((root / "build/release").glob("*waveshare-relay-6ch*.bin"))
    paths += sorted((root / "build/merged").glob("*waveshare-relay-6ch*.bin"))
    if len(paths) < 3 or any(not p.exists() for p in paths):
        raise SystemExit("Build actuator artifacts first")
    log = (root / ".pio/final-build.log").read_text(encoding="utf-8-sig")
    checks = {
        name: any(line.startswith(name) and "SUCCESS" in line for line in log.splitlines())
        for name in ("waveshare-relay-6ch", "esp32-s3-devkitc-1", "Kincony-B16M")
    }
    if not all(checks.values()):
        raise SystemExit("Final firmware validation has not passed all required environments")
    manifest = {
        "generatedUtc": datetime.now(timezone.utc).isoformat(),
        "baseCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "workingTreeChangesIncluded": True,
        "firmwareBuilds": checks,
        "arduinoEsp32Core": "3.3.12",
        "platformRelease": "55.03.312-1",
        "platformioCore": "6.2.0",
        "knxCommit": "980c047ad7fc5e27bf2fae95e48acde5d5e0b4fd",
        "knxProductIdentity": "development only; manufacturer 0x00FA, application 600, version 1",
        "hardwareTested": False,
        "etsImportTested": False,
        "artifacts": [
            {
                "path": p.relative_to(root).as_posix(),
                "bytes": p.stat().st_size,
                "sha256": sha(p),
            }
            for p in paths
        ],
    }
    (root / "docs/actuator-build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Wrote docs/actuator-build-manifest.json")


if __name__ == "__main__":
    main()
