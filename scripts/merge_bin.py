"""Keep the legacy merged-image export alongside the root release package."""

from pathlib import Path

from release_artifacts import build_identity, merge_firmware

Import("env")


def merge_bin(source, target, env):
    version, app_name, environment = build_identity(env)
    output = Path(env.subst("$PROJECT_DIR")) / "build" / "merged"
    output.mkdir(parents=True, exist_ok=True)
    destination = output / f"{app_name}_{environment}_{version.replace('.', '-')}_webflash.bin"
    merge_firmware(env, destination)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
