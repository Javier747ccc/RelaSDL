# AGENTS.md
Guidance for coding agents working in `C:\Javier\Prog\Repo\RelaSDL`.

## Project Snapshot
- Language: C++.
- Main app standard: C++17 (`CMAKE_CXX_STANDARD 17`).
- Build system: CMake.
- Main target: `RelaSDL`.
- Primary files: `RelaSDL.cpp`, `PracticalSocket.cpp`, `PracticalSocket.h`, `RelaUtiles.h`.
- Third-party dependency: `third_party/yaml-cpp`.
- Platform orientation: Windows-first (SDL DLL copy + `ws2_32` link).

## Repository Policy Files
- `.cursorrules`: not present.
- `.cursor/rules/`: not present.
- `.github/copilot-instructions.md`: not present.
- If these appear later, they take precedence over this document.

## Build Commands
Use commands from repository root.

### Configure (MinGW debug)
```bash
cmake -S . -B build-mingw-debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
```

### Build everything
```bash
cmake --build "build-mingw-debug"
```

### Build only the app
```bash
cmake --build "build-mingw-debug" --target RelaSDL
```

### Run the app
```bash
"build-mingw-debug/RelaSDL.exe"
```

### Clean artifacts
```bash
cmake --build "build-mingw-debug" --target clean
```

### Visual Studio alternative (if needed)
```bash
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build "build-vs" --config Debug
```

## Test Commands
There are no first-party tests for `RelaSDL` in the root project.

### Generic test entrypoint
```bash
ctest --test-dir "build-mingw-debug" --output-on-failure
```

### Enable yaml-cpp tests (optional)
`yaml-cpp` tests are OFF by default (`YAML_CPP_BUILD_TESTS=OFF`).
```bash
cmake -S . -B build-tests -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DYAML_CPP_BUILD_TESTS=ON
cmake --build "build-tests" --target yaml-cpp-tests
```

### Run all yaml-cpp tests
```bash
ctest --test-dir "build-tests" --output-on-failure -R "yaml-cpp::test"
```

### Run one test case (single test)
`yaml-cpp-tests` is a GoogleTest binary. Use `--gtest_filter`:
```bash
"build-tests/third_party/yaml-cpp/test/yaml-cpp-tests.exe" --gtest_filter="SuiteName.TestName"
```

### List test names first
```bash
"build-tests/third_party/yaml-cpp/test/yaml-cpp-tests.exe" --gtest_list_tests
```

## Lint / Static Analysis
- No root lint target is defined.
- No root `.clang-format` is defined.
- Recommended quality gate: compile after edits and fix warnings/errors in touched code.
- Do not introduce repo-wide formatting sweeps unless explicitly requested.

## Code Style Guidelines
Always match existing style in nearby code. Avoid style-only churn.

### File Scope and Structure
- Keep functions near existing related logic.
- Prefer small, focused diffs.
- Do not move large blocks unless required by the task.

### Includes and Imports
- Add the minimal include needed.
- Keep include style consistent with the current file.
- Keep third-party includes explicit (`<yaml-cpp/yaml.h>`, SDL headers).
- Avoid adding new dependencies unless asked.

### Formatting
- Preserve indentation style per file:
  - `RelaSDL.cpp`: tabs are common.
  - `PracticalSocket.cpp/.h`: spaces are common.
- Keep brace placement consistent with local conventions.
- Keep spacing and blank-line rhythm consistent with surrounding code.
- Avoid reformatting untouched regions.

### Types and Data Handling
- Use `const` where values do not change.
- Use explicit types at API boundaries.
- Keep existing constant naming like `kWindowCount`, `kVelocityLimit`.
- Validate external input before conversion (especially YAML nodes).

### Naming
- Follow local naming in the file being edited.
- Existing conventions are mixed; preserve nearby patterns:
  - Constants: `kCamelCase`.
  - Globals: mixed (`Times`, `Velocidades`, `Pause`, `eventos`).
  - Functions: mixed (`DrawScene`, `LoadEventosFromYaml`, `handle_key_event_`).
- Do not do broad rename refactors unless requested.

### Error Handling
- Guard against malformed config and runtime inputs.
- Prefer safe fallback behavior (skip invalid entries, continue processing).
- Log runtime failures at established call sites (`SDL_Log` in app code).
- Use try/catch at integration boundaries (file parsing, external resources).

### State Management
- `RelaSDL.cpp` relies heavily on global mutable state.
- Minimize adding new globals.
- If state is added, wire it into reset/restart flows where needed.
- Keep one-shot event behavior idempotent (`triggered` flags).

### Comments
- Add comments only for non-obvious intent or invariants.
- Keep comments short and behavior-focused.
- Remove stale comments when changing behavior.

### Config and Assets
- `config.yaml` is copied to output directory by CMake post-build commands.
- Keep YAML schema changes backward compatible unless asked otherwise.
- Keep parser behavior tolerant: continue loading valid records.

## Boundaries for Agent Changes
- Do not edit `third_party/` unless the user explicitly asks.
- Do not revert unrelated local changes in the working tree.
- Avoid destructive git operations.
- Build after meaningful C++ edits.
- If tests are unavailable, report that clearly with exact command(s) run.

## Quick Checklist
- Confirm target files and scope.
- Apply minimal code changes.
- Build with `cmake --build`.
- Run `ctest` when tests exist in the current build dir.
- For a single test, use `--gtest_filter` on `yaml-cpp-tests.exe`.
- Summarize what was verified and what was not.
