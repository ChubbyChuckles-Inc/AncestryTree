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
