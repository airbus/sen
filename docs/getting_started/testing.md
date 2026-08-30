# Running the tests

Tests are off by default. Enable them at `conan install` time, build, then drive them through
the `run_tests` family of targets. Those targets build every test prerequisite first
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

`run_tests` executes the stable tier in parallel (`ctest -LE flaky`), then the flaky-labeled
tier with `--repeat until-pass:5`, and merges both JUnit outputs into `ctestReport.xml`.
Narrower lanes exist too: `run_unit_tests`, `run_integration_tests`, `run_smoke_tests`.

Plain `ctest --preset conan-gcc-release -R <pattern>` is fine for iterating on one suite,
but remember it only *runs*; build the tree (or the relevant lane target) first so test
prerequisites exist.

## What to expect outside CI

A handful of suites are environment-sensitive by design. Seeing them fail on a laptop or in a
bare container does not mean the tree is broken:

| Suite | Behavior outside CI |
| --- | --- |
| `RestE2EFixture.*` | Flaky-labeled (retried up to 5×). Binds a fixed local port; leftover server processes or parallel runs make it fail. |
| `CliPackageInit*` | End-to-end scaffolding of `sen package init`; sensitive to the surrounding environment and known to fail in bare containers. |
| kernel `transport` / `runtime_compatibility` / `crash_report` / `type_clash` | Not built. They are commented out in `libs/kernel/test/CMakeLists.txt` until the pipeline can host them. When they come back they drive several `sen run` processes through `runner.py`, so they need no container, but they do need the `ether` and `py` components in the build. |
| kernel `object_sync` / `interest_filtering` | Linux only, and registered only when the build opts in: `object_sync` needs `-DSEN_INTEGRATION_TEST_IMAGE=<image>` and `SEN_BUILD_ETHER`, `interest_filtering` needs `SEN_BUILD_ETHER`. Not registered otherwise, so a plain local run stays green rather than failing. `object_sync` additionally needs a Docker daemon plus `pip install docker testcontainers`. The stress suites are not registered at all. |
| `jsonrpc_ts_client_integration` (reconnect scenario) | Known-intermittent: races a server bounce against re-resolution. |

## The Debug lane

`SEN_DEBUG_ASSERT` and the component-level invariant checks compile **only** in the `Debug`
configuration, which is the only one that defines `DEBUG`. `Release` defines `NDEBUG` and
`RelWithDebInfo` defines neither, so in both of them those checks are absent from the build. When
touching lifecycle, subscription, or state-machine code, run the affected component's suite in Debug
as well:

```shell
conan install . --profile:all=sen_gcc -o "sen/*:with_tests=True" -s build_type=Debug --build=missing
source build/gcc/Debug/generators/conanbuild.sh
cmake --preset conan-gcc-debug
cmake --build --preset conan-gcc-debug --target jsonrpc jsonrpc_test
(cd build/gcc/Debug/bin && ./jsonrpc_test)
```

Run component test binaries from the build's `bin/` directory. Several load their component's
shared library at runtime and resolve it relative to there.

## Writing tests

For writing unit tests against Sen's `TestKernel` / `TestComponent`, see
[Unit testing](../howto_guides/unit_tests.md). The component test directories
(`components/*/test/`) are the templates for suite structure and CMake registration.
