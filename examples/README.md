# Sen Examples

How this directory is laid out:

- `packages/` holds the example code: one Sen package per directory, each with its STL
  interface, C++ implementation and a runnable `config.yaml`.
- `config/` holds the documentation pages for the example walkthroughs. They are sources for
  the [documentation site](https://airbus.github.io/sen/latest/) and reference the code in
  `packages/`; read them there rather than on GitHub.
- `apps/` holds larger example applications.

## Building the examples

The examples build with the rest of the tree when the `with_examples` option is on. Options are
read at `conan install`, the step that generates the build files:

```shell
conan install . --profile=sen_gcc_x86 --build=missing -o "sen/*:with_examples=True"
conan build . --profile=sen_gcc_x86
```

Use `sen_gcc_arm` instead on arm hardware; the README explains the profiles.

Each example package lands as a shared library in `build/gcc/Release/bin/` next to the `sen`
application.

## Running an example

From the repository root, with the tree built as above:

```shell
source build/gcc/Release/generators/conanrun.sh          # Third-party library paths
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$(pwd)/build/gcc/Release/bin"
cd examples/packages/my_counter
../../../build/gcc/Release/bin/sen run config.yaml
```

This boots the tutorial counter with an interactive Sen shell attached; the `counters` bus is
already open, so you can inspect the ticking `myCounter` object directly. Every package under
`packages/` follows the same pattern through its own `config.yaml`; the path segments
(`build/gcc/Release`) follow the profile you built with.
