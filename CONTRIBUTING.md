# Contributing to AncestryTree

Thank you for supporting the AncestryTree project! This document outlines how to propose changes and keep the codebase healthy.

## Development Workflow

1. **Fork and Branch**

   - Fork the repository and create a feature branch named `feature/<short-description>` or `fix/<issue-id>`.
   - Keep branches focused; one feature or fix per branch.

2. **Coding Standards**

   - Use strict C99 (`-std=c99`), avoid compiler extensions when possible.
   - No global variables; pass state through explicit context structures.
   - Keep functions under 50 lines where reasonable.
   - Add descriptive comments for non-obvious logic and all public APIs.
   - Format code using `clang-format` with the project style once published.

3. **Testing**

   - Add or update unit tests in `tests/` for every change.
   - Run the full test suite: `make test` or `ctest` after configuring with CMake.
   - Ensure the project builds without warnings on all supported platforms.

4. **Documentation**

   - Update `roadmaps/implementation.txt` with `[x]` markers for completed tasks.
   - Extend the changelog with a concise summary of user-visible changes.
   - Document new public APIs in the corresponding header files.

5. **Pull Requests**
   - Rebase on `main` before submitting.
   - Provide a clear description of the change, testing performed, and potential impacts.
   - Reference related issues or roadmap tasks.

## Code of Conduct

All contributors must adhere to the [Code of Conduct](CODE_OF_CONDUCT.md). Violations should be reported to the maintainers at `opensource@chubbychuckles.inc`.

## Getting Help

If you have questions, open a discussion thread or file an issue. We're happy to help new contributors get started.
