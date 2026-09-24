from PyInstaller.utils.hooks import collect_data_files, collect_dynamic_libs


datas = collect_data_files(
    "torch",
    excludes=[
        "**/*.h",
        "**/*.hpp",
        "**/*.cuh",
        "**/*.lib",
        "**/*.cpp",
        "**/*.pyi",
        "**/*.cmake",
        "**/bin/*",
        "**/test/**",
    ],
)
binaries = collect_dynamic_libs("torch")
