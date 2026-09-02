# The configuration file

`sen run my_config.yaml` starts a kernel from a YAML file. That file is how you say which
components exist, which objects they publish, how fast they run and how the kernel itself behaves.
For most users it is the part of Sen they touch most often, and the only part they touch without
writing C++.

This page is the reference for it. [Command line tools](command_line.md) documents the `sen`
commands themselves.

## Sections

A configuration file has these top-level keys. All of them are optional, and a file with none of
them starts a kernel that does nothing.

| Key | What it holds |
|---|---|
| `include` | Other YAML files to merge into this one |
| `load` | Components that already exist as libraries, to be loaded |
| `build` | Components for the kernel to assemble out of your packages |
| `kernel` | How the kernel itself runs |

Start with the distinction between `load` and `build`. `load` brings in a
component someone wrote in C++: the shell, `influx`, `jsonrpc`, or one of yours. `build` asks the
kernel to construct a component for you out of packages and a list of objects, with no component
code of your own. Most projects use both: `load` for the shell, `build` for their own objects.

```yaml title="both sections together"
load:
  - name: shell
    group: 2
    open: [ local.counters ]

build:
  - name: counterComponent
    group: 3
    freqHz: 2
    imports: [ my_counter ]
    objects:
      - name: myCounter
        class: my_counter.CounterImpl
        bus: local.counters
        step: 5
```

## `load`: components that already exist

Each entry needs a `name`, which is the component's registered name. **A name repeated in `load` is
a hard error**, not a last-one-wins.

Beyond `name`, an entry may carry any key from [`ComponentConfig`](#componentconfig) and any number
of keys the component itself defines. `open` above is one of the latter: the shell reads it, the
kernel does not know what it means.

## `build`: components the kernel assembles

Each entry needs a `name`, and again **a repeated name is a hard error**. The keys the kernel treats
specially are:

| Key | Meaning |
|---|---|
| `freqHz` | How often the component runs its cycle |
| `imports` | Packages to load, so the kernel can find your classes |
| `objects` | The objects to instantiate |

### `freqHz`

The cycle rate, converted internally to a period. Neither of these is obvious from the file:

- **Omitting it is not an error.** The kernel logs `component frequency not set. Defaulting to 30
  Hz` and continues at 30 Hz. If you have never seen that warning it is because your configurations
  set the key. A missing `freqHz` therefore fails quietly in the log rather than at startup.
- **`freqHz` has to be positive.** Zero or a negative number is refused at startup with `invalid
  frequency value`. There is no floor above zero and no ceiling, so `freqHz: 0.5` is valid and gives
  a two-second cycle.

### `imports`

A list of package names. The kernel needs this to resolve the `class` values below; without the
import the class simply is not found. Note that importing a package is not the same as the loader
finding its library. That is `LD_LIBRARY_PATH`, or `PATH` on Windows, and is covered in
[Create your first package](../getting_started/first_package.md).

### `objects`

A list of objects to instantiate. Three keys are structural and the rest are initial property
values:

| Key | Required | Meaning |
|---|---|---|
| `name` | yes | Instance name, unique within its component and bus |
| `class` | yes | Qualified implementation class, `<package>.<Class>` |
| `bus` | no | Where to publish it; omit it and the object is not published on a bus |

A repeated object `name` within a component is a hard error, as it is for components themselves. And
`bus` must be exactly two dot-separated parts, a session and a bus. So `my.tutorial` is valid,
`my.tutorial.extra` and a bare `tutorial` both fail at startup with `invalid bus address`.

Every other key sets a property's initial value. Static properties **must** be given one here,
because they cannot be assigned later.

```yaml
objects:
  - name: firstGrade
    class: school.ClassroomImpl
    bus: school.primary
    studentsBus: school.primary   # a property of ClassroomImpl
    defaultSize: 5                # likewise
```

### What the component actually receives

The kernel forwards configuration keys on to the component as its own parameters, and the exclusion
list is short: **everything except `group`, `objects` and `imports` is forwarded.**

That is wider than it looks. `name` and `freqHz` are forwarded
as well as consumed, and so are the `ComponentConfig` keys. So a component may read `freqHz` itself
if it wants to, and a parameter of your own named `priority` will collide with the thread priority.
Pick another name.

## `ComponentConfig`

These keys may appear on any `load` or `build` entry. They configure the thread the component runs
on and the queues around it.

```rust
--8<-- "libs/kernel/stl/sen/kernel/basic_types.stl:component_config"
```

| Key | Notes |
|---|---|
| `priority` | `lowest`, `nominalMin`, `nominalMax` or `highest` |
| `stackSize` | Thread stack in bytes; `0` means the platform default |
| `group` | Startup ordering, see below |
| `cpuAffinity` | A CPU mask, so `0x5` is cores 0 and 2 |
| `inQueue`, `outQueue` | See [Queues](#queues) |
| `sleepPolicy` | See [Sleep policy](#sleep-policy) |

**`group` is the one that changes behavior most.** Components start in ascending group order, and a
group is fully up before the next begins. That is why the shell is conventionally in group 2 and
your own components in group 3: the shell is ready and has its buses open before objects start
appearing on them. Groups are not priorities, and they do not affect scheduling while the system
runs. They do decide shutdown order: the kernel stops and unloads groups from the highest number
down, so a component in group 2 outlives one in group 3 and the shell is the last thing to go.

### Queues

```rust
--8<-- "libs/kernel/stl/sen/kernel/basic_types.stl:queue_config"
```

**Queues are unbounded by default** (`maxSize: 0`). Setting a bound means deciding what to discard
when it is reached, which is what `evictionPolicy` is for: `dropOldest` keeps the freshest data and
`dropNewest` keeps the earliest.

```yaml
build:
  - name: myComponent
    group: 3
    freqHz: 100
    inQueue:
      maxSize: 1000
      evictionPolicy: dropOldest
```

### Sleep policy

```rust
--8<-- "libs/kernel/stl/sen/kernel/basic_types.stl:sleep_policy"
```

A variant, so you pick one shape or the other. `SystemSleep` hands waiting to the operating system.
`PrecisionSleep` spins down through two coarser sleeps before the final wait, which costs CPU and
buys timing accuracy; a zero in either field means its default, 7 ms and 1 ms respectively.
Components built by the kernel default to `PrecisionSleep`.

## The `kernel` section

```rust
--8<-- "libs/kernel/stl/sen/kernel/basic_types.stl:kernel_params"
```

Everything here has a default, so the section can be omitted entirely.

| Key | Notes |
|---|---|
| `runMode` | See below |
| `appName` | Names the application; optional |
| `bus` | Where kernel objects are published. Defaults to `local.kernel` |
| `clockBus` | Where a virtual clock is published. **Defaults to whatever `bus` is** |
| `clockName` | Defaults to `clock` |
| `clockMaster` | Under virtual time, publish a master clock to `clockBus` |
| `logConfig` | Pattern, level, sinks, loggers and whether a backtrace is printed on failure |
| `crashReportDir` | Defaults to the system temporary directory |
| `crashReportDisabled` | Suppresses reports entirely |
| `lockMemoryPages` | Keeps the process resident, so it cannot be paged out |
| `sleepPolicy` | As above, for the kernel's own component |

### Run modes

```rust
--8<-- "libs/kernel/stl/sen/kernel/basic_types.stl:run_mode"
```

`realTime` is the default shape: components run against the system clock. Under `virtualTime` the
kernel advances in discrete steps you drive yourself, which is what makes a run reproducible within
a process; `virtualTimeRunning` advances those steps continuously instead of waiting to be told.
`startAndStop` brings everything up and takes it straight back down, which is a cheap smoke test
that a configuration is valid.

## Environment variables

Any value may pull from the environment with `@env(NAME)`. **A missing variable is a runtime
error**, so use the two-argument form `@env(NAME,DEFAULT)` when you want a fallback.

Spaces around the comma are fine. The default itself has to be letters, digits or underscores, so
the dot in `@env(BUS,local.kernel)` stops it matching. Text that does not match is left alone. You
get no error and no fallback, just the literal `@env(BUS,local.kernel)` as your value. The pattern
also does not work across two lines.

```yaml
load:
  - name: shell
    group: 2
    open: @env(MY_BUS)                       # throws if MY_BUS is unset
  - name: @env(MY_COMP,defaultComponent)     # falls back if MY_COMP is unset
    group: 3
```

The pattern escapes with backslashes written immediately before the `@`, and what happens depends on
whether there is an odd or an even number of them:

- **Even**: rendered as half as many backslashes, and the variable **is** substituted.
- **Odd**: rendered as half of what remains after removing one, and the substitution does **not**
  happen; the `@env(...)` text is left as it is.

```yaml title="with MY_COMP=FooComponent"
someEscape1: \@env(MY_COMP,defaultComponent)      # -> @env(MY_COMP,defaultComponent)
someEscape2: \\@env(MY_COMP,defaultComponent)     # -> \FooComponent
someEscape3: \\\@env(MY_COMP,defaultComponent)    # -> \@env(MY_COMP,defaultComponent)
someEscape4: \\\\@env(MY_COMP,defaultComponent)   # -> \\FooComponent
```

## Combining files

`include` merges other files into this one. Inclusion is recursive, and paths are relative to the
top-level configuration file rather than to the file doing the including.

```yaml title="my_setup.yaml"
include:
  - shell.yaml
  - ether.yaml

build:
  - name: myComponent
    group: 3
    freqHz: 10
    imports: [ my_package ]
    objects:
      - name: myObject
        class: my_package.MyClassImpl
        bus: my.tutorial
        prop1: someValue
```

Lists are concatenated, so two files each contributing a `load` entry give you both. Where the same
setting is defined twice, **the file doing the including wins.** That is what makes overriding
compact. Name only what you want to change:

```yaml title="the same setup, with one property overridden"
include:
  - my_setup.yaml

build:
  - name: myComponent
    objects:
      - name: myObject
        prop1: someOtherValue
```

Nothing else needs repeating. `group`, `freqHz`, `imports`, `class` and `bus` all survive from the
included file.

## Getting your editor to check the file

Sen can generate a JSON schema from your data model, and an editor pointed at that schema will flag
unknown keys and wrong types as you type, and complete your own class and property names. This is
the part of configuring Sen where the tooling already does the work, so it is worth wiring up.

**If you built your package with `add_sen_package`, the schema already exists.** Generation is on by
default and lands in `${PROJECT_SOURCE_DIR}/schemas`, which `SCHEMA_PATH` controls and `NO_SCHEMA`
turns off (see [CMake](cmake.md)). Otherwise, generate one from your STL:

```sh
# a schema for a package
sen generate json package stl stl/my_counter/counter.stl \
    -i stl -c my_counter.CounterImpl -o my_counter.json
```

Do not skip `-c`. It lists the implementation classes you wrote, which is what
lets the schema complete and check the `class:` values in your `objects` list. Without it the
schema's class list comes out empty. Components use `sen generate json component stl` with `-n` for
the component name instead.

Then combine every schema your configuration might mention into one file:

```sh
sen generate json schema my_counter.json other_package.json -o schema.json
```

From CMake, `sen_combine_schemas(OUTPUT <file> SCHEMAS <files...>)` does the same thing as a build
step, which is how this repository assembles the schema its own examples use.

Finally, point the configuration at it. Every example configuration that ships with Sen carries this
as its first line:

```yaml
# $schema: ../base/schema.json

load:
  - name: shell
    group: 2
```

**How that line is picked up depends on your editor.** JetBrains IDEs read the `# $schema:` comment
directly, which is why the examples are written that way. The YAML extension for VS Code looks for
its own form, `# yaml-language-server: $schema=./schema.json`, or a `yaml.schemas` mapping in your
settings. Both consume the same generated file.

!!! note "Regenerate it when the model changes"
    The schema is generated from your STL, so it goes stale exactly like any other generated
    artifact. Under CMake that is handled for you. By hand, it is a step to remember. A schema
    describing last week's model will confidently flag a property you added yesterday.

## See also

- **[Command line tools](command_line.md)**: the `sen` commands, including every `--help`
- **[CMake](cmake.md)**: `add_sen_package`, `SCHEMA_PATH` and the rest of the build interface
- **[Create your first package](../getting_started/first_package.md)**: a configuration written
  from scratch, in context
- **[Main concepts](main_concepts.md)**: what buses, groups and properties are, if the vocabulary
  here is new
