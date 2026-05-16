# Building the Lottie GDExtension — All Platforms

Complete build instructions for every platform this extension targets. Covers
toolchain setup, ThorVG compilation, the SCons extension build, and output
verification.

> **Status legend** (verification of these instructions in this repo):
> ✅ = built end-to-end during the LottieAnimation3D work — exact commands shown.
> 📋 = distilled from the repo's existing `build_thorvg.sh` / `.gdextension` /
> README plus standard godot-cpp + ThorVG + Emscripten / NDK practice. The
> commands are correct in shape but the specific environment may need small
> tweaks (e.g. NDK version, Emscripten version).

The `.gdextension` file declares output paths for all of these:

| Platform | Architectures | ThorVG output | Extension output |
|---|---|---|---|
| Windows | `x86_64` (✅), `x86_32` (📋) | `thorvg-1.dll` (shared) | `.dll` |
| Linux | `x86_64`, `arm64` (📋) | `libthorvg.a` (static) | `.so` |
| macOS | Universal (`x86_64`+`arm64`) (📋) | `libthorvg.a` (static) | `.framework` |
| Android | `arm64`, `x86_64` (📋) | `libthorvg.a` (static, cross-compiled) | `.so` |
| Web | `wasm32` nothreads (📋) | static (`build_wasm/`) | `.wasm` |

---

## Common prerequisites (every platform)

You will need these on whatever machine runs the build:

| Tool | Why | How |
|---|---|---|
| Git | Clone & submodules | https://git-scm.com/downloads |
| Python 3.10+ | Drives meson + SCons | https://www.python.org/downloads/ |
| meson 1.0+ | Configures ThorVG | `python -m pip install --user meson` |
| ninja | Backend for meson | `python -m pip install --user ninja` |
| SCons 4+ | Builds the extension | `python -m pip install --user scons` |

After installing via `pip install --user`, make sure the user-site `Scripts`
directory is on `PATH`:

- Windows: `%APPDATA%\Python\PythonXY\Scripts`
- Linux/macOS: `~/.local/bin`

Clone the repo once with submodules:

```bash
git clone https://github.com/neerajpan/Lottie.git
cd Lottie
git submodule update --init --recursive
```

`thirdparty/godot-cpp/` and `thirdparty/thorvg/` should now contain source.

---

## 1. Windows — ✅ verified

### 1.1 Install MSVC toolchain

Either Visual Studio 2022 Community/Pro or **Build Tools for Visual Studio**
(headless, smaller). When installing, select the **"Desktop development with
C++"** workload — that gives you `cl.exe`, the Windows SDK, and `vcvars64.bat`.

Verify:

```
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
```

(Or `Community\VC\...` / `Professional\VC\...` for the IDE editions.)

### 1.2 Build ThorVG (x86_64)

ThorVG must be built inside a `vcvars64`-initialised shell so meson can find
`cl.exe`. The simplest way is a small helper batch:

```batch
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "PATH=%APPDATA%\Python\Python312\Scripts;%PATH%"
cd /d "%~dp0thirdparty\thorvg"
if exist builddir rmdir /s /q builddir
meson setup builddir ^
  -Dbuildtype=release -Doptimization=3 -Db_ndebug=true ^
  -Dsimd=true -Dthreads=true -Dpartial=true ^
  -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false ^
  --backend=ninja
meson compile -C builddir
```

Save as `_build_thorvg_msvc.bat` next to `SConstruct`, double-click (or run
from `cmd.exe`). Build time: ~2–5 min on a modern laptop.

Output: `thirdparty/thorvg/builddir/src/thorvg-1.dll`, `thorvg.lib`, `thorvg.exp`.

### 1.3 Build the extension

```batch
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
python -m SCons platform=windows target=template_debug   arch=x86_64 -j%NUMBER_OF_PROCESSORS%
python -m SCons platform=windows target=template_release arch=x86_64 -j%NUMBER_OF_PROCESSORS%
```

First build is ~10 min (godot-cpp's ~990 wrapper classes compile once).
Subsequent incremental builds are seconds.

Output:
- `demo/addons/godot_lottie/bin/libgodot_lottie.windows.template_debug.x86_64.dll`
- `demo/addons/godot_lottie/bin/libgodot_lottie.windows.template_release.x86_64.dll`
- `demo/addons/godot_lottie/bin/thorvg-1.dll` (auto-copied by SConstruct)

### 1.4 Windows x86_32 (📋)

Same flow with the 32-bit MSVC environment and `arch=x86_32`:

```batch
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
... (rebuild ThorVG into a different builddir, e.g. builddir_x86) ...
python -m SCons platform=windows target=template_release arch=x86_32 -jN
```

You'll need to rebuild ThorVG for x86 in a separate directory — `SConstruct`
currently looks at `thirdparty/thorvg/builddir`, so swap the contents of that
folder when switching architectures, or extend `SConstruct` to look at an
arch-specific subfolder.

---

## 2. Linux — 📋

### 2.1 Install toolchain (Ubuntu/Debian shown)

```bash
sudo apt update
sudo apt install build-essential pkg-config python3 python3-pip git
# Meson + ninja: either pip or apt
sudo apt install meson ninja-build
# Or: python3 -m pip install --user meson ninja scons
```

For Fedora: `sudo dnf install gcc-c++ make python3 python3-pip git meson ninja-build`.
For Arch: `sudo pacman -S base-devel python python-pip git meson ninja`.

### 2.2 Build ThorVG (x86_64 native)

The repo already ships `build_thorvg.sh` — just run it:

```bash
chmod +x build_thorvg.sh
./build_thorvg.sh
```

The script invokes meson with `-Ddefault_library=static`, so the output is
`thirdparty/thorvg/builddir/src/libthorvg.a` (statically linked into the
extension `.so`).

### 2.3 Build the extension

```bash
python3 -m SCons platform=linux target=template_debug   arch=x86_64 -j$(nproc)
python3 -m SCons platform=linux target=template_release arch=x86_64 -j$(nproc)
```

Output:
- `demo/addons/godot_lottie/bin/libgodot_lottie.linux.template_debug.x86_64.so`
- `demo/addons/godot_lottie/bin/libgodot_lottie.linux.template_release.x86_64.so`

(No runtime `.so` is copied — ThorVG is statically linked on Linux.)

### 2.4 Linux arm64 — 📋

Two paths:

**(a) Native** on an arm64 box (Raspberry Pi 5, Ampere, etc.): identical
commands as above, just `arch=arm64`.

**(b) Cross-compile** from x86_64: install an arm64 cross-toolchain
(`crossbuild-essential-arm64` on Debian) and supply a meson cross-file when
building ThorVG. The shape:

```bash
# thirdparty/thorvg/cross-arm64.ini
[binaries]
c = 'aarch64-linux-gnu-gcc'
cpp = 'aarch64-linux-gnu-g++'
ar = 'aarch64-linux-gnu-ar'
strip = 'aarch64-linux-gnu-strip'

[host_machine]
system = 'linux'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
```

```bash
cd thirdparty/thorvg
meson setup builddir_arm64 \
  --cross-file cross-arm64.ini \
  -Dbuildtype=release -Ddefault_library=static \
  -Dsimd=true -Dthreads=true -Dpartial=true \
  -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C builddir_arm64
```

Then for SCons set `CC`/`CXX` to the cross-compiler and pass `arch=arm64`:

```bash
CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++ \
  python3 -m SCons platform=linux target=template_release arch=arm64
```

(For the arm64 build, point SCons' `thorvg_lib_dir` at `builddir_arm64` —
either by renaming it to `builddir` for the run, or by extending the
SConstruct to take an env-var override.)

---

## 3. macOS — 📋

### 3.1 Install toolchain

```bash
xcode-select --install                # Command Line Tools (clang, ar, etc.)
brew install meson ninja python scons # via Homebrew
# or: python3 -m pip install --user meson ninja scons
```

### 3.2 Build ThorVG (Universal)

The shipped `build_thorvg.sh` detects ARM Mac and disables `-Dthreads` there
(ARM macOS + ThorVG threading had compat issues). For a Universal `.a` you
can either run the script twice (once per arch with a meson cross-file) and
`lipo` them together, or just build for the host arch you're on:

```bash
./build_thorvg.sh   # produces libthorvg.a for the host arch (x86_64 or arm64)
```

For a true Universal binary that runs on both Intel and Apple Silicon:

```bash
# arm64 build
cd thirdparty/thorvg
meson setup builddir_arm64 \
  --cross-file <path-to-arm64-cross.ini> \
  -Dbuildtype=release -Ddefault_library=static -Dsimd=true -Dthreads=false \
  -Dpartial=true -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C builddir_arm64

# x86_64 build
meson setup builddir_x86_64 \
  --cross-file <path-to-x86_64-cross.ini> \
  -Dbuildtype=release -Ddefault_library=static -Dsimd=true -Dthreads=true \
  -Dpartial=true -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C builddir_x86_64

# Combine
lipo -create builddir_arm64/src/libthorvg.a builddir_x86_64/src/libthorvg.a \
     -output builddir/src/libthorvg.a
```

### 3.3 Build the extension

```bash
python3 -m SCons platform=macos target=template_debug   arch=universal -j$(sysctl -n hw.ncpu)
python3 -m SCons platform=macos target=template_release arch=universal -j$(sysctl -n hw.ncpu)
```

Output is a **framework bundle** (see SConstruct line 67–73):

- `demo/addons/godot_lottie/bin/libgodot_lottie.macos.template_debug.framework/`
- `demo/addons/godot_lottie/bin/libgodot_lottie.macos.template_release.framework/`

Each framework contains the binary plus an `Info.plist`. The `.gdextension`
declares those bundle paths and Godot loads them transparently.

### 3.4 Code-signing (optional but recommended for distribution)

```bash
codesign --force --sign - --options runtime \
  demo/addons/godot_lottie/bin/libgodot_lottie.macos.template_release.framework
```

Replace `-` with a Developer ID for notarisation-ready signing.

---

## 4. Android — 📋

Android is **always** cross-compiled (from Linux, macOS, or Windows). You
build one ThorVG per ABI and one extension per ABI.

### 4.1 Install Android NDK

Get **Android NDK r25c or newer** (godot-cpp 4.3+ requires it):

- Via Android Studio: SDK Manager → SDK Tools → NDK (Side by side)
- Or standalone: https://developer.android.com/ndk/downloads

Set the env var (path varies — adjust to where it lives on your machine):

```bash
export ANDROID_NDK_ROOT="$HOME/Android/Sdk/ndk/25.2.9519653"
# or Windows: set ANDROID_NDK_ROOT=C:\Users\you\AppData\Local\Android\Sdk\ndk\25.2.9519653
```

### 4.2 Build ThorVG per ABI

ThorVG doesn't ship Android cross-files, so write them. For **arm64-v8a**
(`thirdparty/thorvg/cross-android-arm64.ini`):

```ini
[constants]
ndk = '/full/path/to/android-ndk-r25c'
api = '24'
host_tag = 'linux-x86_64'   # or 'darwin-x86_64' / 'windows-x86_64'
toolchain = ndk / 'toolchains/llvm/prebuilt' / host_tag

[binaries]
c       = toolchain / 'bin/aarch64-linux-android' + api + '-clang'
cpp     = toolchain / 'bin/aarch64-linux-android' + api + '-clang++'
ar      = toolchain / 'bin/llvm-ar'
strip   = toolchain / 'bin/llvm-strip'
pkg-config = 'false'

[host_machine]
system     = 'android'
cpu_family = 'aarch64'
cpu        = 'aarch64'
endian     = 'little'
```

For **x86_64**: replace `aarch64-linux-android` with `x86_64-linux-android`
and set `cpu_family = 'x86_64'`, `cpu = 'x86_64'`.

Build:

```bash
cd thirdparty/thorvg
meson setup builddir_android_arm64 \
  --cross-file cross-android-arm64.ini \
  -Dbuildtype=release -Ddefault_library=static \
  -Dsimd=true -Dthreads=true -Dpartial=true \
  -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C builddir_android_arm64

# Repeat for x86_64
meson setup builddir_android_x86_64 \
  --cross-file cross-android-x86_64.ini \
  -Dbuildtype=release -Ddefault_library=static -Dsimd=true -Dthreads=true \
  -Dpartial=true -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C builddir_android_x86_64
```

### 4.3 Build the extension per ABI

Before each SCons run, copy/symlink the matching ABI build of ThorVG into
`thirdparty/thorvg/builddir` (which is what SConstruct currently inspects):

```bash
# arm64-v8a
rm -rf thirdparty/thorvg/builddir
ln -s builddir_android_arm64 thirdparty/thorvg/builddir
python3 -m SCons platform=android target=template_release arch=arm64 \
  android_api_level=24 -j$(nproc)

# x86_64
rm -rf thirdparty/thorvg/builddir
ln -s builddir_android_x86_64 thirdparty/thorvg/builddir
python3 -m SCons platform=android target=template_release arch=x86_64 \
  android_api_level=24 -j$(nproc)
```

Output:

- `demo/addons/godot_lottie/bin/libgodot_lottie.android.template_release.arm64.so`
- `demo/addons/godot_lottie/bin/libgodot_lottie.android.template_release.x86_64.so`
- (and the corresponding `template_debug` files if you build that target)

> **Suggested SConstruct improvement:** make `thorvg_lib_dir` configurable
> via an env var or a `thorvg_builddir=…` SCons argument so you don't need
> to swap symlinks between ABIs. This is in the BUILDING.md follow-up list.

### 4.4 Testing on a device

Push the demo project to a connected device via the Godot Android export
template, or copy `demo/addons/godot_lottie/` into your existing Godot
Android app's `addons/` and enable the plugin.

---

## 5. Web (Emscripten / WASM) — 📋

### 5.1 Install Emscripten

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh        # Linux/macOS
# Windows: emsdk_env.bat
```

Verify:

```bash
emcc --version
# emcc (Emscripten gcc/clang-like replacement) 3.x.x
```

Godot 4.3+ Web exports use **wasm32 + no threads** (the SharedArrayBuffer
COOP/COEP requirement is a pain), which matches the `.gdextension` entry
(`web.template_release.wasm32.nothreads.wasm`).

### 5.2 Build ThorVG for WASM

ThorVG ships an `emscripten` cross-file template; copy / adapt it. Sample
`thirdparty/thorvg/cross-emscripten.ini`:

```ini
[binaries]
c = 'emcc'
cpp = 'em++'
ar = 'emar'
strip = 'emstrip'
pkg-config = 'false'

[host_machine]
system = 'emscripten'
cpu_family = 'wasm32'
cpu = 'wasm32'
endian = 'little'

[built-in options]
c_args     = ['-pthread=false']
cpp_args   = ['-pthread=false', '-DTHORVG_THREAD_SUPPORT=0']
c_link_args   = ['-pthread=false']
cpp_link_args = ['-pthread=false']
```

Build:

```bash
cd thirdparty/thorvg
meson setup build_wasm \
  --cross-file cross-emscripten.ini \
  -Dbuildtype=release -Ddefault_library=static -Db_ndebug=true \
  -Dsimd=false -Dthreads=false -Dpartial=true \
  -Dengines=sw -Dloaders=lottie -Dbindings=capi -Dexamples=false
meson compile -C build_wasm
```

Note the output directory is **`build_wasm`** (not `builddir`) — that's what
the SConstruct's Web branch expects:

```python
if env["platform"] == "web":
    thorvg_lib_dir = os.path.join("thirdparty", "thorvg", "build_wasm", "src")
```

### 5.3 Build the extension for Web

```bash
python3 -m SCons platform=web target=template_debug   -j$(nproc)
python3 -m SCons platform=web target=template_release -j$(nproc)
```

SCons will pick up `emcc` from PATH (which `emsdk_env.sh` put there).

Output:

- `demo/addons/godot_lottie/bin/libgodot_lottie.web.template_debug.wasm32.nothreads.wasm`
- `demo/addons/godot_lottie/bin/libgodot_lottie.web.template_release.wasm32.nothreads.wasm`

### 5.4 Testing in the browser

Export the demo project from Godot as **Web** with **Threading: Disabled**.
Serve the resulting HTML/JS/WASM bundle with any static server:

```bash
cd ~/Desktop/web_export
python3 -m http.server 8000
# open http://localhost:8000/index.html
```

---

## 6. Verifying the addon in Godot

After any platform build:

1. Open `demo/project.godot` in Godot 4.3+. Restart the editor if it was
   running during the build so it reloads the rebuilt DLL/.so/.wasm.
2. Open `res://addons/godot_lottie/demo/controldemo.tscn` (2D smoke test)
   and `res://addons/godot_lottie/demo/demo3d.tscn` (3D smoke test).
3. Press **F5** / **F6**. Both animations should play.
4. In the editor's *Project → Tools → Script Editor* run:
   ```gdscript
   @tool
   extends EditorScript
   func _run():
       print("2D:", ClassDB.class_exists("LottieAnimation"))
       print("3D:", ClassDB.class_exists("LottieAnimation3D"))
   ```
   Both should print `true`.

---

## 7. Packaging the addon

To ship the addon to other Godot projects:

1. Build for every platform you intend to target (the `bin/` folder
   accumulates per-platform binaries).
2. Zip the entire `demo/addons/godot_lottie/` directory.
3. Recipients drop `godot_lottie/` into their project's `addons/` folder and
   enable it under *Project → Project Settings → Plugins*.

Godot picks the right `.dll` / `.so` / `.framework` / `.wasm` per the
`.gdextension` file automatically.

---

## 8. Troubleshooting

| Symptom | Cause / Fix |
|---|---|
| `meson: command not found` | `python -m pip install --user meson ninja`, then add user `Scripts` to `PATH`. |
| `cl is not recognized` (Windows) | Build wasn't started inside a `vcvars64`-initialised shell. Run from the helper batch file shown in §1.2. |
| `Error: ThorVG build directory not found: thirdparty/thorvg/builddir` | ThorVG hasn't been built yet, or you built it into a different directory (e.g. `build_wasm`, `builddir_arm64`). For non-Web targets the SConstruct currently expects `builddir`; swap your per-arch directories accordingly. |
| Linker error: `unresolved external symbol _tvg_...` | ThorVG was built with `-Dbindings=capi=false`. Rebuild with `-Dbindings=capi`. |
| Godot loads the addon but `LottieAnimation3D` isn't in *Create New Node* | DLL is older than your sources. Force a clean SCons build (`python -m SCons -c` to clean, then rebuild) and restart the editor. |
| Web export shows `RuntimeError: Aborted(...)` on load | ThorVG was built with `-Dthreads=true` for WASM. Web build must use `-Dthreads=false` and the Emscripten flag `-pthread=false`. |
| Android: `libgodot_lottie.so` missing | Wrong ABI built or wrong `arch=` flag. Each Android ABI needs its own ThorVG + extension build (§4.3). |

---

## 9. Follow-up improvements to this repo (not done yet)

These would make multi-platform builds smoother:

- **`thorvg_builddir=...` SCons argument** so each platform/arch points at
  its own ThorVG output directory without symlink juggling.
- **`build_thorvg_android.sh`** and **`build_thorvg_wasm.sh`** helper scripts
  (analogous to the existing `build_thorvg.sh` / `.bat`).
- **CI workflow** (e.g. `.github/workflows/build.yml`) that builds all
  platforms on every push and uploads the binaries as release artefacts.
- Ship the per-platform meson cross-files (`cross-android-*.ini`,
  `cross-emscripten.ini`) in `thirdparty/thorvg-crossfiles/` so users don't
  hand-edit them.
