# Changelog

All notable changes to this project will be documented in this file. The format is inspired by [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Layout cache manager that fingerprints family tree relationships per algorithm, reuses previously computed node
  positions between ticks, and ships with unit tests covering cache invalidation and multi-algorithm reuse.
- Memory diagnostics suite that wraps the unit tests with Valgrind/Dr. Memory helpers, adds leak-focused regression cases for tree and persistence lifecycles, and fails the runner when outstanding allocations are detected.
- GitHub Actions build/test automation spanning Windows, Linux, and macOS with Debug/Release matrices, dependency provisioning, artifact uploads, and Valgrind-backed regression gates.
- Automated coverage instrumentation toggled via CMake, with CI jobs emitting filtered lcov reports, archiving results, and publishing metrics to Codecov for ongoing regression tracking.
- Release workflow that builds platform-specific bundles on tagged pushes, packages assets alongside binaries, and publishes archives through GitHub Releases for testers.
- Dedicated quality gate workflow running clang-format checks, clang-tidy static analysis, cppcheck scans, and strict C99 warning enforcement.
- Documentation workflow generating Doxygen HTML, archiving artifacts, and publishing to GitHub Pages, with a shared `.clang-format` style to keep generated reports aligned with coding standards.
- Startup bootstrap planner that deterministically selects between CLI-specified trees, bundled samples, and placeholder holograms with new unit tests covering each scenario and improved logging when falling back.
- Adaptive Nuklear initialisation that scales fonts to the active render resolution, attempts to load preferred UI typefaces, falls back gracefully to built-in glyphs, and adds regression tests for the scaling heuristics.

- Cross-platform file dialog integration for opening and saving trees, using Win32 common dialogs, AppleScript-backed panels on macOS, and zenity/kdialog fallbacks on Linux, with `.json` extension enforcement and new unit tests for the dialog helpers.
- Placeholder holographic preview SVG (`docs/preview-placeholder.svg`) linked from the README until captured screenshots and GIFs are published.
- Add-person workflow now routes the Nuklear panel through AppState commands, importing profile/certificate assets, focusing the new holographic node, and shipping regression tests for person creation validation.
- Certificate gallery holograms sourced from person metadata, rendering birth, marriage, death, and archival documents with PDF-aware badges, keyboard-controlled zoom, and immersive column animations inside the detail view.
- Timeline holoboard now supports Shift+scroll navigation for dense event histories and surfaces associated media via marker badges and in-room preview cards, with regression tests validating detail content metadata.
- Timeline holoboard overlay that converts timeline entries into a generated track with hover tooltips, dynamic easing, and supporting `timeline_init`, `timeline_render`, and `timeline_event_hover` helpers plus new content-builder regression coverage.
- Immersive image panel subsystem that lays out conical anchors with sightline prechecks, animates focus/zoom interactions, resolves timeline occlusion, and renders holographic panels with light cones; accompanied by a dedicated regression test suite.
- Detail view accessors for content readiness, timeline/panel phase queries, and sanitised metadata retrieval, backed by new regression tests covering initialization, content clamping, and activation smoothing.
- Expansion animation regression suite exercising full enter/exit cycles with camera focus persistence and lighting transitions to guard against future behavioural regressions.
- Integration tests now exercise the expansion enter/exit workflow and UI event queue saturation to protect immersive detail transitions and menu dispatch behaviour.
- Detail view rendering system with holographic interior rooms, orbiting timeline nodes, and floating information panels triggered by sphere expansion animations.
- Expansion animation controller managing cubic-eased sphere growth, camera transitions, and bidirectional animation sequences for immersive person detail views.
- Unit tests for expansion state management and detail view system initialization ensuring null safety and basic functionality coverage.
- Initial repository scaffolding and documentation.
- Core unit tests for person and family tree modules covering relationships, metadata, and validation.
- ISO-8601 date utilities with strict validation for people and timeline entries, plus timeline unit coverage.
- Marriage metadata support on person records with reciprocal updates and regression tests.
- Family tree root enumeration API and cycle detection safeguards with new validation tests.
- JSON save support via `persistence_tree_save`, emitting metadata, people, and relationships with pretty formatting and escaping.
- Persistence unit tests covering full tree export, root detection, and error handling for invalid inputs.
- Example JSON tree asset demonstrating the current save format.
- Custom lightweight JSON parser with Unicode escape handling, structural validation, and dedicated unit tests.
- Tree load pipeline rebuilding persons, links, and assets from JSON with schema version checks and validation.
- Persistence layer safeguards including UTF-8 enforcement and automatic `.bak` backup creation before overwrites.
- Round-trip persistence tests exercising save/load compatibility against `assets/example_tree.json`.
- Configurable persistence auto-save manager with periodic saves, explicit flush hooks, and dedicated regression tests.
- Cross-platform dependency setup guide plus automated checklist script for compilers, raylib, and Nuklear.
- Windows PowerShell automation (`scripts/setup_dependencies.ps1`) that fetches raylib, installs Nuklear, configures CMake builds, and optionally launches the prototype; CMake now auto-enables Nuklear when the header is present.
- Hierarchical layout module computing 3D node positions with unit tests ensuring generation spacing and finiteness.
- Window management abstraction providing raylib-backed initialization, resize detection, and fullscreen toggling with fallback stubs for missing backends.
- Orbital camera controller with configurable bounds, pan/zoom handling, and comprehensive math-focused unit tests.
- Interactive prototype runtime combining raylib rendering, camera controls, and a Nuklear HUD (auto orbit, camera focus) powered by the new application loop and asset path utilities with accompanying unit tests.
- Rendering pipeline module with glow-shaded spheres, directional lighting, relationship connection lines, and supporting unit tests for spatial lookups and segment generation.
- Pointer interaction system with hover/selection ray casting, renderer highlights, adaptive link thickness, and regression tests covering hit math and state transitions.
- Hierarchical layout refinements centering roots, pairing spouses side-by-side, cascading generations vertically, and validating the geometry with new unit tests.
- Render-target based presentation layer with holographic screen overlay, custom glow vertex shader, responsive Nuklear menu bar/ dialogs, and regression tests for resize handling.
- Dynamic holographic name panel system with cached billboards, optional profile portraits, configurable font sizing (UI control + multi-font caching with mipmaps), animated glow shader timing, and dedicated regression tests.
- Camera orbit controller now eases towards new targets (half-life smoothing) and focus transitions, reducing snap while preserving precise controls, plus accompanying smoothing regression tests.
- Batched sphere rendering that groups alive and deceased nodes to minimize shader state changes, validated by new batching planner unit tests.
- Connection rendering upgrades with antialiased lines and optional bezier-style curves for parent/child links, plus
  configuration validation tests ensuring style ranges remain safe.
- Nuklear view menu toggles now expose smooth line rendering and straight/Bezier connection styling so testers can
  evaluate holographic link aesthetics live.
- Persistence regression suite now exercises corrupted files, missing asset references, and escaped string decoding,
  guarding JSON edge cases with new targeted unit tests.
- Layout test coverage now verifies single-person, small family, large sibling groups, multi-generation stacks, and
  complex relationship graphs to enforce non-overlapping holographic placement.
- Force-directed layout option adds physics-based separation with spring connections and generational anchoring,
  validated by new layout regression tests.
- Layout transition system blends between layout variants using `app_state_tick`, delivering smooth holographic
  animations with dedicated AppState and layout unit coverage.
- Quick Help overlay captures onboarding notes, troubleshooting tips, and the active keyboard shortcut map for testers.
- Keyboard shortcut mapper consolidates Ctrl+N/O/S/Shift+S/Z/Y plus Space and Escape into UI events with automated
  regression tests guarding the input routing.
- Settings control panel offering graphics presets, camera sensitivities, color themes, and auto-save cadence with
  disk persistence, runtime application, and dedicated unit coverage for settings runtime helpers.
- Search and filter panel providing live name queries, alive/deceased toggles, birth-year constraints, and one-click
  camera focus for selected results, supported by a direct-selection interaction helper.
- Search execution module with targeted unit tests plus an integration workflow test that exercises create/save/load
  round-tripping via the persistence layer.
- Centralised `AppState` layer managing UI mode flags, selection state, undo/redo stacks, and tree dirty tracking with
  integration into the main loop and Nuklear event handling (including functional Undo/Redo menu actions).
- AppState unit tests covering command execution failure paths, undo/redo cycling, and history resets to guard the new
  state manager contract.
- Undoable person command suite (`add`, `edit`, `delete`) preserving relationships, refreshing holographic layouts, and
  shipping with regression tests that exercise undo/redo restoration semantics.
- Logging subsystem upgrades enabling optional file sinks, console toggles, and timestamped levels, alongside defensive
  error-handling macros (`AT_CHECK_NULL`, `AT_CHECK_ALLOC`, `AT_TRY`/`AT_THROW`) with dedicated regression tests.
- Modal error dialog surfaces runtime failures to the user, while the logger now attaches a default on-disk sink for
  persistent diagnostics.
- Asset pipeline gains `asset_copy` utility to relocate external media into project-managed imports with unique naming
  and relative-path emission, guarded by automated tests.
- Asset cleanup routine walks family tree references, validates imported media, prunes unreferenced files from the
  project assets directory, and ships with dedicated unit tests covering removal and integrity failure scenarios.
- Asset export packaging now assembles deterministic `.atpkg` archives containing the tree JSON and all referenced
  media, enabling portable sharing with automated regression coverage.
- Debug builds now activate a central allocation tracker with leak diagnostics, runtime stats queries, and new unit
  tests verifying realloc safety and outstanding allocation reporting.
- Centralised `event_process` orchestration ties together window resize handling, pointer interactions, and shortcut
  routing while powering new unit tests that exercise shortcut and UI queue dispatch without raylib dependencies.
