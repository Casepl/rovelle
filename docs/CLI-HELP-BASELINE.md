# CLI help baseline

Date: 17 October 2026

This baseline establishes the build, test, and CLI help contract before the
manifest format changes. It does not change manifest behavior.

## Verification commands

Run the complete baseline from a fresh checkout or disposable source tree:

```sh
cmake -S . -B /tmp/rovelle-baseline-build
cmake --build /tmp/rovelle-baseline-build
ctest --test-dir /tmp/rovelle-baseline-build --output-on-failure
```

The automated `cli_help_smoke` test checks these invocations:

```sh
/tmp/rovelle-baseline-build/rovelle --help
/tmp/rovelle-baseline-build/rovelle help
/tmp/rovelle-baseline-build/rovelle version --help
/tmp/rovelle-baseline-build/rovelle hash --help
/tmp/rovelle-baseline-build/rovelle blob put --help
/tmp/rovelle-baseline-build/rovelle blob get --help
/tmp/rovelle-baseline-build/rovelle manifest publish --help
```

Each invocation must exit with status 0 and print a case-insensitive `Usage:`
marker. The command inventory is read from the root help and compared with the
expected baseline, so adding or removing an advertised command requires an
intentional test update.

## Isolation and portability

The smoke test is POSIX `sh` and uses only utilities available on macOS and
Linux. It resolves the executable path before changing to a temporary
directory, writes all captured output below that directory, and removes the
directory on exit. It does not access the user's home, repository data, or
operational command paths.

## Results

- Clean CMake configure: passed.
- Clean build with strict warnings-as-errors: passed.
- Full CTest suite: passed (`1/1`, `cli_help_smoke`).
- Manual smoke-test invocation: passed.

## Warnings

The configure, build, and test commands emitted no warnings. No unrelated
issues were changed.
