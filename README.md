# AncestryTree

AncestryTree is a cross-platform 3D genealogical tree visualization written in pure C99. It leverages [raylib](https://www.raylib.com/) for realtime 3D rendering and window management, and [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) for an immersive holographic user interface. The application enables users to explore complex family relationships using an interactive, glowing sphere metaphor accompanied by per-person media, documents, and life timelines.

## Project Goals

- **Portable**: Builds cleanly on Windows, Linux, and macOS using Make or CMake.
- **Self-contained**: No runtime dependencies beyond raylib and Nuklear.
- **Maintainable**: Modular architecture, strict C99 compliance, and comprehensive tests.
- **Immersive**: Futuristic UI overlays layered on the 3D tree visualization.

## Repository Layout

```
assets/            # Art assets, fonts, textures, icons (placeholders by default)
docs/              # Design documents and additional documentation
include/           # Public header files for the engine and subsystems
  external/        # Third-party single-header dependencies (e.g., Nuklear)
roadmaps/          # Project planning and implementation roadmaps
scripts/           # Build, packaging, and automation scripts
src/               # Application source files
tests/             # C99 unit and integration tests
```

## Getting Started

1. **Install Dependencies**

   - Install a C99-compliant toolchain (MSVC, clang, or gcc).
   - Install [raylib 5.x](https://github.com/raysan5/raylib/releases) for your platform.
   - Download the latest `nuklear.h` from the official repository and place it in `include/external/`.
   - On Windows, ensure `raylib.lib` is reachable by the linker. On Linux/macOS, ensure `pkg-config` can locate raylib.

2. **Configure Environment Variables** _(optional but recommended)_

   - `RAYLIB_HOME`: Path to the root of your raylib installation.
   - `NUKLEAR_INCLUDE`: Path to `nuklear.h` if it differs from the default location.

3. **Build**

   ```powershell
   # Using Make (MSYS2/MinGW or GNU make on macOS/Linux)
   make

   # Using CMake
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ```

4. **Run Tests**

   ```powershell
   # After building, run the test binary
   ./build/tests/ancestrytree_tests
   ```

## Development Guidelines

- Follow the established coding standards in `docs/coding-standards.md` (to be finalized).
- Avoid introducing global variables; use dependency injection via context structures.
- Keep functions short and focused, and document all public APIs in headers.
- When adding features, update the roadmap at `roadmaps/implementation.txt` with `[x]` completed tasks.
- Ensure all new code is covered by unit tests in `tests/`.

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for coding conventions, branch naming guidance, review expectations, and the workflow for submitting patches.

## License

This project is released under the MIT License. See [`LICENSE`](LICENSE) for details.
