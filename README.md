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

1. **Prepare Dependencies**

   - Follow the platform-specific setup instructions in [`docs/dependencies.md`](docs/dependencies.md).
   - On Windows you can automate the download of raylib and Nuklear with:

     ```powershell
     pwsh -File scripts/setup_dependencies.ps1 -PersistEnvironment
     ```

     The script fetches prebuilt raylib binaries, installs `nuklear.h`, configures CMake (defaulting to Ninja), and launches a debug build unless you pass `-SkipBuild` or `-SkipRun`.

   - Place `nuklear.h` in `include/external/` (the dependency guide explains where to download it). The setup script performs this automatically.
   - Run the automated checklist:

     ```powershell
     python scripts/check_dependencies.py
     ```

     The script validates compiler availability, raylib discovery, and the presence of Nuklear. Address any warnings before continuing.

2. **Build**

   ```powershell
   # Using Make (MSYS2/MinGW or GNU make on macOS/Linux)
   make

   # Using CMake + Ninja
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ```

3. **Run Tests**

   ```powershell
   ctest --output-on-failure --test-dir build
   ```

4. **Launch the Prototype**

   ```powershell
   ./build/bin/ancestrytree
   ```

   A raylib window opens with a prototype 3D layout rendered from `assets/example_tree.json`. Hold the right mouse button to orbit, middle mouse or WASD/arrow keys to pan, and use the scroll wheel to zoom. The Nuklear HUD (top-left) exposes camera shortcuts and an auto-orbit toggle.

> **Manual validation**: After major dependency upgrades, re-run the checklist script, rebuild, launch the app, and interact with the sample data to ensure rendering and UI responsiveness remain stable.

## Usage Guide

Until the GUI is fully realised, you can experiment with persistence features and the command-line stub:

1. Run the test suite (`ctest`) to confirm persistence save/load pipelines remain intact.
2. Execute `build/bin/ancestrytree` to start the prototype. The logger reports initialization status and the window renders the sample family tree with orbit/pan/zoom controls.
3. Modify `assets/example_tree.json` and use the persistence API (see `tests/test_persistence.c`) to load and inspect data, ensuring UTF-8 and schema validation succeed.
4. When the rendering core and Nuklear overlay land, this section will expand with runtime controls for orbiting the camera, selecting spheres, and opening detail panels.

The roadmap in [`roadmaps/implementation.txt`](roadmaps/implementation.txt) lists the next milestones—including 3D rendering, holographic UI, and real-time interactions.

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
