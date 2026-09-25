from pathlib import Path

from PyInstaller.utils.hooks import collect_data_files, collect_dynamic_libs, get_package_paths


datas = collect_data_files(
    "torch",
    include_py_files=True,
    excludes=[
        "**/*.h",
        "**/*.hpp",
        "**/*.cuh",
        "**/*.lib",
        "**/*.cpp",
        "**/*.pyi",
        "**/*.cmake",
        "**/test/**",
    ],
)

binaries = collect_dynamic_libs("torch")
torch_package = Path(get_package_paths("torch")[1])
torch_shm_manager = torch_package / "bin" / "torch_shm_manager"
if torch_shm_manager.is_file():
    binaries.append((str(torch_shm_manager), "torch/bin"))
