# Dependency Setup Guide

This document describes how to install the toolchain and third-party libraries required to build **AncestryTree** on Windows, Linux, and macOS. All steps preserve the project's goals of portable C99 builds, minimal external dependencies, and a futuristic 3D interface rendered via raylib with a Nuklear overlay.

> **Manual validation**: After completing the steps for your platform, run `python scripts/check_dependencies.py` to confirm that the compiler and graphics libraries are detected correctly. Then rebuild the project and launch the prototype to ensure the window opens without runtime errors.

---

## Windows (MSVC or MinGW)

1. **Install a compiler**

   - **MSVC**: Install the "Desktop development with C++" workload in Visual Studio 2022 or newer.
   - **MinGW-w64**: Install via [MSYS2](https://www.msys2.org/) and update packages with `pacman -Syu`.

2. **Install raylib 5.x**

   - Download the prebuilt MSVC or MinGW package from the [raylib releases page](https://github.com/raysan5/raylib/releases).
   - Extract it to a stable location, e.g., `C:\Libraries\raylib`.
   - Copy `raylib.lib` (MSVC) or `libraylib.a` (MinGW) into the `lib` directory inside that folder.
   - Keep `include/raylib.h` inside the same root folder.

3. **Configure environment variables**

   - Set `RAYLIB_HOME` to the raylib root (e.g., `C:\Libraries\raylib`).
   - If you place `nuklear.h` elsewhere, set `NUKLEAR_INCLUDE` to the directory that contains it.

4. **Validate**
   - From a "x64 Native Tools" prompt (MSVC) or MSYS2 shell (MinGW), run `python scripts/check_dependencies.py`.
   - Build with `cmake --build build` or `ninja` to ensure raylib links successfully.

---

## Linux (GCC or clang)

1. **Install a compiler**

   - Use your distribution package manager: e.g., `sudo apt install build-essential` for GCC or `sudo apt install clang` for clang.

2. **Install raylib 5.x**

   - Preferred: install via package manager (`sudo apt install libraylib-dev`) if available.
   - Manual: download sources and run `cmake -S . -B build -DBUILD_EXAMPLES=OFF`, then `cmake --build build --target install` with `sudo`.

3. **Integrate Nuklear**

   - Copy the latest `nuklear.h` to `include/external/nuklear.h` inside the repository.

4. **Validate**
   - Ensure `pkg-config` can locate raylib: `pkg-config --libs raylib` should output linker flags.
   - Run `python scripts/check_dependencies.py` to double-check compiler and raylib visibility.
   - Build with `cmake --build build` (or `make`) and launch the prototype; verify the window opens and closes cleanly.

---

## macOS (Apple clang)

1. **Install Command Line Tools**

   - Run `xcode-select --install` to install Apple clang and the required SDKs.

2. **Install raylib 5.x via Homebrew**

   - `brew install raylib`
   - Confirm headers under `/opt/homebrew/include` (Apple Silicon) or `/usr/local/include` (Intel).

3. **Integrate Nuklear**

   - Place `nuklear.h` at `include/external/nuklear.h` in the repository.

4. **Validate**
   - Run `python scripts/check_dependencies.py` in a terminal window.
   - Perform a debug build: `cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug` followed by `cmake --build build`.
   - Launch `./build/bin/ancestrytree`; verify that the raylib window appears and that closing it terminates the process.

---

## Additional Notes

- **GPU Drivers**: Ensure your graphics drivers are up to date to support raylib's OpenGL backend. On laptops with hybrid GPUs, force the discrete GPU for smoother holographic rendering.
- **Testing**: Every time you upgrade raylib or the compiler, re-run `python scripts/check_dependencies.py` and the full C test suite (`ctest --output-on-failure --test-dir build`).
- **CI Integration**: The dependency check script exits with a non-zero status if a required component is missing, making it suitable for future CI gating.
