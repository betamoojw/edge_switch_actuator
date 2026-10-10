"""Package the current PlatformIO firmware without relying on cached exports."""

import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


RELEASE_NAME = "edge_switch_actuator"


def build_identity(env):
    defines = env.ParseFlags(env["BUILD_FLAGS"]).get("CPPDEFINES", [])
    values = {
        item[0]: str(item[1]).replace('\\"', '"').strip('"')
        for item in defines
        if isinstance(item, (list, tuple)) and len(item) == 2
    }
    identity = (values.get("APP_VERSION"), values.get("APP_NAME"), env["PIOENV"])
    for name, value in zip(("APP_VERSION", "APP_NAME", "PIOENV"), identity):
        if not value or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._+-]*", value):
            raise ValueError(f"{name} must be a nonempty, filename-safe build identifier")
    return identity


def merge_firmware(env, destination):
    board = env.BoardConfig()
    flash_mode = board.get("build.flash_mode", "dio")
    if flash_mode in ("qio", "qout"):
        flash_mode = "dio"
    if board.get("build.arduino.memory_type", "qio_qspi") in ("opi_opi", "opi_qspi"):
        flash_mode = "dout"
    frequency = int(str(board.get("build.f_flash", "40000000L")).rstrip("L")) // 1000000
    images = env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])) + [
        "$ESP32_APP_OFFSET", "$BUILD_DIR/${PROGNAME}.bin"
    ]
    # An argument list keeps Windows paths containing spaces intact.
    command = [
        env.subst("$PYTHONEXE").strip('"'),
        env.subst("$OBJCOPY").strip('"'),
        "--chip", board.get("build.mcu", "esp32"),
        "merge-bin", "-o", str(destination),
        "--flash-mode", flash_mode, "--flash-freq", f"{frequency}m",
        "--flash-size", board.get("upload.flash_size", "4MB"),
    ] + [env.subst(str(item)).strip('"') for item in images]
    subprocess.run(command, check=True, cwd=env.subst("$PROJECT_DIR"))


def package_release(source, target, env):
    version, _, environment = build_identity(env)
    stem = f"{RELEASE_NAME}_{environment}_{version}"
    output = Path(env.subst("$PROJECT_DIR")) / "buildRelease"
    output.mkdir(parents=True, exist_ok=True)

    # Finish all generation before replacing any previously successful release.
    with tempfile.TemporaryDirectory(prefix=f".{stem}-", dir=output) as temporary:
        staging = Path(temporary)
        ota = staging / f"{stem}_ota.bin"
        shutil.copyfile(env.subst("$BUILD_DIR/${PROGNAME}.bin"), ota)
        shutil.copyfile(env.subst("$BUILD_DIR/${PROGNAME}.elf"), staging / f"{stem}.elf")
        (staging / f"{stem}_ota.md5").write_text(
            hashlib.md5(ota.read_bytes()).hexdigest(), encoding="ascii"
        )
        merge_firmware(env, staging / f"{stem}_webflash.bin")
        for artifact in staging.iterdir():
            artifact.replace(output / artifact.name)
    print(f"Release artifacts: {output / stem}*")
