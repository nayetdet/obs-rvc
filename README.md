# OBS RVC

**Retrieval-based Voice Conversion as an OBS plugin.**

OBS RVC adds a real-time RVC audio filter to OBS Studio. Add it to an audio source, choose an RVC model, and convert the source voice while streaming or recording.

## Performance notice

> [!WARNING]
> Running OBS with CPU/software rendering competes with RVC for CPU resources. It can increase lag, cause audio dropouts, and make OBS less responsive. Use GPU rendering in OBS whenever possible.

> [!WARNING]
> RVC processing without GPU acceleration can also be much slower and may not keep up with real-time audio on lower-end CPUs. The current worker uses the CPU PyTorch build. Reserve CPU threads for OBS and test your scene before going live.

## Use

1. In OBS, open **Filters** for the audio source you want to convert.
2. Add **Retrieval-based Voice Conversion** under **Audio Filters**.
3. Select an RVC `.pth` model. An `.index` file is optional.
4. Make sure the HuBERT and RMVPE model paths are valid, then adjust the filter settings as needed.

If audio stutters, increase the conversion-block duration, reduce the RVC CPU thread count, reserve more threads for OBS, or simplify the OBS scene.

## Dependencies

The build has two parts: a native OBS plugin (C/C++) and a bundled RVC worker (Python). Install the dependencies for **your operating system** before running `make build`.

> [!NOTE]
> GPU rendering in OBS is strongly recommended for performance, but the current RVC worker is built with the **CPU-only PyTorch** packages. CUDA, ROCm, and other GPU-compute runtimes are not build dependencies for this version.

### Required on every platform

| Dependency | Why it is needed |
| --- | --- |
| Git | Clones the source code. |
| Git LFS | Downloads the HuBERT, RMVPE, and RVC model files in `models/`. |
| CMake 3.28+ | Configures and builds the native OBS plugin. |
| Python 3.12 | Builds the bundled RVC worker. |
| `uv` | Installs the locked Python environment and packages for the worker. |
| Rust and Cargo (`rustup`) | Builds the `iceoryx2` IPC dependency. |
| Internet access | Lets CMake download OBS/iceoryx2 dependencies and lets `uv` download Python packages. |

You do **not** need to install the worker's Python libraries manually. `uv` installs the exact locked versions from [`plugin-worker/uv.lock`](plugin-worker/uv.lock), including `rvc`, `torch`, `torchaudio`, `iceoryx2`, `pydantic`, and `pydantic-settings`.

### Ubuntu 24.04 (x86_64)

Install the native build dependencies:

```sh
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config git git-lfs \
  ffmpeg obs-studio libobs-dev \
  libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavutil-dev \
  libswresample-dev libswscale-dev \
  libgles2-mesa-dev libsimde-dev libsndfile1 libportaudio2 \
  qt6-base-dev qt6-base-private-dev libqt6svg6-dev \
  software-properties-common unzip
```

Then install Python 3.12, `uv`, and Rust/Cargo. The project's development container uses Python 3.12 and Rust 1.85, and `uv` must be available in your `PATH`.

`obs-studio` and `libobs-dev` are required on Linux because the plugin links against the system OBS/libobs installation. `Ninja` is the generator used by the Linux CMake preset.

### Windows (x64)

Install all of the following and make sure their commands are available in `PATH`:

1. Git and Git LFS.
2. CMake 3.28 or newer.
3. Python 3.12 and `uv`.
4. Rust and Cargo through `rustup`.
5. Visual Studio 2022 with **Desktop development with C++**.
6. Windows 10/11 SDK version **10.0.22621**. This is the SDK selected by the `windows-x64` CMake preset.

OBS source code, prebuilt OBS dependencies, and Qt are downloaded by CMake during configuration on Windows; they do not need to be installed separately.

### macOS

Install all of the following and make sure their commands are available in `PATH`:

1. Xcode 16 or newer, plus the Xcode Command Line Tools.
2. Git and Git LFS.
3. CMake 3.28 or newer.
4. Python 3.12 and `uv`.
5. Rust and Cargo through `rustup`.

The macOS preset builds a universal Apple Silicon and Intel plugin. OBS source code, prebuilt OBS dependencies, and Qt are downloaded by CMake during configuration; they do not need to be installed separately.

## Build and install

Clone the project with its model files:

```sh
git lfs install
git clone https://github.com/nayetdet/obs-rvc.git
cd obs-rvc
git lfs pull
```

Build it from the repository root:

```sh
make build
```

The build output is placed in `plugin/release/RelWithDebInfo`.

To install a local build into OBS:

```sh
make install
```

On Linux, the default installation prefix is `/usr`. Use another prefix if your OBS installation is elsewhere:

```sh
make install OBS_INSTALL_PREFIX=/path/to/obs-prefix
```
