# Continuous Integration

This project uses a single GitHub Actions workflow located at `ci/github-actions.yml`.

## Job Overview
- **Checkout repository** – Pulls the latest sources for analysis and build.
- **Configure CMake** – Generates the build system with Release defaults in `build/`.
- **Build project** – Compiles all C and C++ targets for both tracks.
- **Run test suite** – Executes the full `ctest` matrix (smoke, spec, integration).

The workflow runs on pushes to `main` and for all pull requests.
