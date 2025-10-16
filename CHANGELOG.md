# Changelog

All notable changes to this project will be documented in this file. The format is inspired by [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

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
- Quick Help overlay captures onboarding notes, troubleshooting tips, and the active keyboard shortcut map for testers.
- Keyboard shortcut mapper consolidates Ctrl+N/O/S/Shift+S/Z/Y plus Space and Escape into UI events with automated
  regression tests guarding the input routing.
