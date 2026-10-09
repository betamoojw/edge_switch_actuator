"""Enable the linker side of -flto for the constrained 4 MB Xtensa profiles."""
Import("env")
# PlatformIO places build_flags=-flto on compilation; the GCC driver also needs
# it at link time to load the LTO plugin for these objects and archives.
env.Append(LINKFLAGS=["-flto"])
