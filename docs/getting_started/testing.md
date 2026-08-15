# Running the Tests

Tests are off by default. Enable them at `conan install` time, build, then drive them through
the `run_tests` family of targets — those targets build every test prerequisite first
(including the `@sen/client` npm install), which a bare `ctest` invocation does not.

## Enable and build

```shell
conan install . --profile:all=sen_gcc -o "sen/*:with_tests=True" --build=missing
conan build   . --profile:all=sen_gcc
pip install junitparser   # run_tests merges the JUnit reports with it
```

## Run

```shell
source build/gcc/Release/generators/conanbuild.sh
cmake --build --preset conan-gcc-release --target run_tests
```

`run_tests` executes the stable tier in parallel (`ctest -LE flaky`), then the flaky-labelled
tier with `--repeat until-pass:5`, and merges both JUnit outputs into `ctestReport.xml`.
Narrower lanes exist too: `run_unit_tests`, `run_integration_tests`, `run_smoke_tests`.

Plain `ctest --preset conan-gcc-release -R <pattern>` is fine for iterating on one suite —
just remember it only *runs*; build the tree (or the relevant lane target) first so test
prerequisites exist.

## What to expect outside CI

A handful of suites are environment-sensitive by design. Seeing them fail on a laptop or in a
bare container does not mean the tree is broken:

| Suite | Behaviour outside CI |
| --- | --- |
| `RestE2EFixture.*` | Flaky-labelled (retried up to 5×). Binds a fixed local port; leftover server processes or parallel runs make it fail. |
| `CliPackageInit*` | End-to-end scaffolding of `sen package init`; sensitive to the surrounding environment and known to fail in bare containers. |
| kernel `transport` / `runtime_compatibility` / `type_clash` / `object_sync` / stress suites | Disabled pending SEN-1725. The `object_sync` container flavours additionally need a Docker daemon plus `pip install docker testcontainers`. |
| `jsonrpc_ts_client_integration` (reconnect scenario) | Known-intermittent: races a server bounce against re-resolution. |

## The Debug lane

`SEN_DEBUG_ASSERT` and the component-level invariant checks compile **only** in the `Debug`
configuration — `Release` and `RelWithDebInfo` define neither `DEBUG` nor `NDEBUG`, so those
checks are absent from the default build. When touching lifecycle, subscription, or
state-machine code, run the affected component's suite in Debug as well:

```shell
conan install . --profile:all=sen_gcc -o "sen/*:with_tests=True" -s build_type=Debug --build=missing
source build/gcc/Debug/generators/conanbuild.sh
cmake --preset conan-gcc-debug
cmake --build --preset conan-gcc-debug --target jsonrpc jsonrpc_test
(cd build/gcc/Debug/bin && ./jsonrpc_test)
```

Run component test binaries from the build's `bin/` directory — several load their component's
shared library at runtime and resolve it relative to there.

## Writing tests

For writing unit tests against Sen's `TestKernel` / `TestComponent`, see
[Unit Testing](../howto_guides/unit_tests.md). The component test directories
(`components/*/test/`) are the templates for suite structure and CMake registration.
