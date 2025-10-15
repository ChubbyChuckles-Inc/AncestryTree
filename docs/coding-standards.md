# Coding Standards

This document captures the evolving coding standards for AncestryTree. All contributors must follow these guidelines when submitting patches.

## Language Version

- All source code must compile with `-std=c99` without relying on compiler-specific extensions.

## General Principles

- Prefer clarity and maintainability over micro-optimizations.
- Pass state through explicit context structures; avoid global variables.
- Limit function length to approximately 50 lines when practical.
- Name functions and variables using `snake_case`.
- Document all public functions in header files using brief descriptions of parameters, return values, and error behavior.
- Use assertions sparingly for programmer errors and return error codes for recoverable conditions.

## File Organization

- Keep files under 500 lines when feasible by splitting cohesive modules.
- Place public type definitions and function declarations in `include/`.
- Keep implementation details in `src/`.
- Group tests under `tests/` mirroring the module structure.

## Error Handling

- Validate all external inputs.
- Return enumerated error codes or `bool` flags to indicate failure.
- Log errors using the centralized logging subsystem.

## Formatting

- Use 4 spaces for indentation; tabs are disallowed.
- Wrap lines at 100 columns to preserve readability across displays.
- Align continuation lines for readability.

These standards will evolve as the codebase grows. Update this document whenever conventions change.
