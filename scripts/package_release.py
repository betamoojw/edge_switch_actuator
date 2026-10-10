"""Export project-board-version files on normal and incremental builds."""

from release_artifacts import package_release

Import("env")

# The alias runs even when firmware.bin is up to date, so deleted exports recover.
# It is not a dependency of clean, buildfs, or erase targets.
env.AlwaysBuild(env.Alias("buildprog"))
env.AddPostAction("buildprog", package_release)
