# Sen examples

How this directory is laid out:

- `packages/` holds the example code: one Sen package per directory, most with an STL
  interface and a C++ implementation. `hla_fom` is the exception, generated from HLA FOM XML
  rather than from STL. Two packages, `my_counter` and `lifecycle`, also carry a runnable
  `config.yaml`.
- `config/` holds the numbered walkthroughs: the YAML configurations that run the examples,
  and a readme per directory. The readmes are sources for the
  [documentation site](https://airbus.github.io/sen/latest/) and reference the code in
  `packages/`; read them there rather than on GitHub.
- `apps/` holds larger example applications.

## Building the examples

The examples build with the rest of the tree when the `with_examples` option is on. It goes on
both commands: `conan build` resolves the graph again, so an option left off it falls back to
its default and rewrites the build folder `conan install` just configured, and the tree comes
out with no examples in it.

```shell
conan install . --profile:all=sen_gcc_x86 --build=missing -o "sen/*:with_examples=True"
conan build . --profile:all=sen_gcc_x86 -o "sen/*:with_examples=True"
```

Both profiles target Linux and detect the architecture, so the same pair of commands works on
x86 and arm. Add `-o "sen/*:with_tests=True"` to both lines as well if you want the tests in
the same build folder -- the options combine, and building twice is not needed.

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

This boots the tutorial counter with an interactive Sen shell attached, and the `counters` bus
is already open. At the prompt:

```text
local.counters.myCounter.print   # prints the value it has reached
local.counters.myCounter.hello   # greets you with the current value
```

It is ticking, so two `print` calls return different values. `config/0_counter/` walks through
the same example in full.

Every package under `packages/` follows the same pattern through its own `config.yaml`; the
path segments (`build/gcc/Release`) follow the profile you built with.
