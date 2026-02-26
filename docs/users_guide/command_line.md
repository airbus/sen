# Command Line Tools

Sen comes with a command line application (called `sen`) to help you perform some actions following
the approach taken by `git`.

The best way to familiarize yourself with all the commands and the available options is to do
`--help`, starting with `sen --help`.

```
--8<-- "snippets/sen.sh"
```

You can also get help about the specific sub-commands.

For example:

```console title="sen command line examples"
# open a sen explorer window
sen explorer

# open a sen shell on the current terminal
sen shell

# connect to a remote sen application on localhost:8094
sen rshell localhost:8094

# starts a replay of an archive
sen replay my_archive

# runs a sen kernel from a configuration file
sen run my_config.yaml
```

## Run

```title="sen run"
--8<-- "snippets/sen_run.sh"
```

The configuration file uses YAML, and it has the following structure:

- A "load" section, where we define the components to be loaded. You need to specify at least the
  name and the group. You can set all the parameters defined in the `ComponentConfig` structure. The
  rest of parameters are forwarded to the component.
- A "build" section, where we define components to be built by the Sen kernel. You need to specify
  at least the name and the group. You can set all the parameters defined in the `ComponentConfig`
  structure. It also takes the following special entries:
  - "imports": a list of packages to import.
  - "objects": a list where you need to define the objects that will be instantiated. You can define
    the initial values that the object properties will have. There are three special entries:
    - "name": (required) The name of the instance. Must be unique within the component and bus (if
      any).
    - "class": (required) The name of the object's class.
    - "bus": (optional) The name of the bus where the object shall be published.
- An optional "kernel" section, that takes the contents of the `KernelParams` structure.

```yaml title="example configuration file"
load:
  - name: shell
    group: 2
    open: [ school.primary, school.secondary ]

build:
  - name: example
    group: 3
    freqHz: 30
    imports: [ school ]
    objects:
      - name: firstGrade
        class: school.ClassRoom
        studentsBus: school.primary
        bus: school.primary
        defaultSize: 5
        createTeacher: true
      - name: secondGrade
        class: school.ClassRoom
        studentsBus: school.secondary
        bus: school.secondary
        defaultSize: 10
        createTeacher: true

```

The `ComponentConfig` structure is defined as follows:

```rust
// Basic runtime configuration for a component
struct ComponentConfig
{
  priority   : Priority,     // thread priority
  stackSize  : u32,          // thread stack size in bytes, 0 means default
  group      : u32,          // the group where to run the component
  inQueue    : QueueConfig,  // queuing of inbound information
  outQueue   : QueueConfig   // queuing of outbound information
}
```

Regarding queues, they can be configured using the `QueueConfig` structure:

```rust
// Component queue eviction policy
enum QueueEvictionPolicy: u32
{
  dropOldest, // when full, drop the least recent element
  dropNewest, // when full, drop the most recent element
}

struct QueueConfig
{
  evictionPolicy : QueueEvictionPolicy, // what to do when the queue is full
  maxSize        : u64                  // 0 means unbounded
}
```

By default, queues are unbounded.

```yaml title="another example configuration file"
load:
  # first, load the shell
  - name: shell
    group: 2

  # then, load our component
  - name: my_component_with_config
    group: 3
    someParam: a value
    someOtherParam: 5 s

```

### Environment variables

The YAML sen parser supports using environment variable values directly by using
`@env(VARIABLE_NAME)` pattern. The pattern can be escaped using the backslash `\` character. If an
even number backlash characters are found before the `@env()` pattern, they are rendered as half of
them and the variable is also rendered. If the number is odd, they are rendered as half of them
minus one and the env command value is not rendered.

The `@env(VARIABLE_NAME)` function also supports default values for when the variable is not found.
It can be specified using `@env(VARIABLE_NAME, DEFAULT_VALUE)`.

If the `@env(VARIABLE_NAME)` is used and the variable does not exist, a runtime error is thrown.

Consider the variables MY_COMP=FooComponent and MY_BUS=BarBus

```yaml title="another example configuration file"
load:
  # first, load the shell
  - name: shell
    group: 2
    open: @env(MY_BUS) # Will throw if MY_BUS does not exist
  # then, load our component
  - name: @env(MY_COMP, defaultComponent) # rendered as - name: FooComponent
    group: 3
    someParam: a value
    someOtherParam: 5 s
    someEscape1: \env(MY_COMP, defaultComponent)        # rendered as env(MY_COMP, defaultComponent)
    someEscape2: \\env(MY_COMP, defaultComponent)       # rendered as \FooComponent
    someEscape3: \\\env(MY_COMP, defaultComponent)      # rendered as \env(MY_COMP, defaultComponent)
    someEscape4: \\\\env(MY_COMP, defaultComponent)     # rendered as \\FooComponent
    someEscape5: \\\\\env(MY_COMP, defaultComponent)    # rendered as \\env(MY_COMP, defaultComponent)
    someEscape6: \\\\\\env(MY_COMP, defaultComponent)   # rendered as \\\FooComponent
    someEscape7: \\\\\\\env(MY_COMP, defaultComponent)  # rendered as \\\env(MY_COMP, defaultComponent)
    someEscape8: \\\\\\\\env(MY_COMP, defaultComponent) # rendered as \\\\FooComponent
```

### Combining yaml files

Since configurations can get very complex and repetitive, Sen provides an "include" mechanism for
yaml files.

```yaml title="including yaml files"
include:
  - shell.yaml
  - ether.yaml

# the rest of the configuration file, as usual
build:
  - name: myComponent
    group: 3
    freqHz: 10
    imports: [ my_package ]
    objects:
      - name: myObject
        class: my_package.MyClassImpl
        prop1: someValue
        bus: my.tutorial

```

Where:

```yaml title="shell.yaml"
load:
  - name: shell
    group: 2
    open: [ my.tutorial ]
```

```yaml title="ether.yaml"
load:
  - name: ether
    group: 3
```

Resulting in a yaml that Sen perceives as:

```yaml title="result"
load:
  - name: shell
    group: 2
    open: [ my.tutorial ]
  - name: ether
    group: 3

build:
  - name: myComponent
    group: 3
    freqHz: 10
    imports: [ my_package ]
    objects:
      - name: myObject
        class: my_package.MyClassImpl
        prop1: someValue
        bus: my.tutorial
```

The `include` block contain a list of files (or a single file like `indclude: shell.yaml`), relative
to the yaml file doing the inclusion. Inclusions are recursive (you can include files that include
other files).

When including a file, it gets merged (or combined) with the contents of the current file. With this
mechanism you can effectively do "unions" of configuration parameters. If parameters are repeated
(the file you are including defines a configuration value that you also define in your file), the
one in your file prevails.

For example, let's define another yaml file as follows:

```yaml title="my_component.yaml"
build:
  - name: myComponent
    group: 3
    freqHz: 10
    imports: [ my_package ]
    objects:
      - name: myObject
        class: my_package.MyClassImpl
        prop1: someValue
        bus: my.tutorial
```

And now let's combine all three as such:

```yaml title="combination"
include:
  - shell.yaml
  - ether.yaml
  - my_component.yaml
```

If we want to overwrite the value of `prop1` of `myObject` in `myComponent`, we can do so like this:

```yaml title="combination"
include:
  - shell.yaml
  - ether.yaml
  - my_component.yaml

build:
  - name: myComponent
    objects:
      - name: myObject
        prop1: someOtherValue
```

Note that you just need to define the things you want to overwrite. The resulting configuration as
perceived by Sen would be like this:

```yaml title="result"
load:
  - name: shell
    group: 2
    open: [ my.tutorial ]
  - name: ether
    group: 3

build:
  - name: myComponent
    group: 3
    freqHz: 10
    imports: [ my_package ]
    objects:
      - name: myObject
        class: my_package.MyClassImpl
        prop1: someOtherValue
        bus: my.tutorial
```

## Code generator

```title="sen gen"
--8<-- "snippets/sen_gen.sh"
```

### C++

```title="sen gen cpp"
--8<-- "snippets/sen_gen_cpp.sh"
```

#### C++ from STL

```title="sen gen cpp stl"
--8<-- "snippets/sen_gen_cpp_stl.sh"
```

#### C++ from HLA FOMs

```title="sen gen cpp fom"
--8<-- "snippets/sen_gen_cpp_fom.sh"
```

### UML

Works the same way as C++, but with sightly different options.

#### UML from STL

```title="sen gen uml stl"
--8<-- "snippets/sen_gen_uml_stl.sh"
```

#### UML from HLA FOMs

```title="sen gen uml fom"
--8<-- "snippets/sen_gen_uml_fom.sh"
```

### MKDocs markdown

Works the same way as C++, but with sightly different options.

#### MKDocs from STL

```title="sen gen uml stl"
--8<-- "snippets/sen_gen_mkdocs_stl.sh"
```

#### MKDocs from HLA FOMs

```title="sen gen uml fom"
--8<-- "snippets/sen_gen_mkdocs_fom.sh"
```

### JSON schemas

Generates json schemas from a Sen data model.

#### JSON schemas for Sen components from STL

```
--8<-- "snippets/sen_gen_json_component_stl.sh"
```

#### JSON schemas for Sen components from HLA FOMs

```
--8<-- "snippets/sen_gen_json_component_fom.sh"
```

#### JSON schemas for Sen packages from STL

```
--8<-- "snippets/sen_gen_json_package_stl.sh"
```

#### JSON schemas for Sen packages from HLA FOMs

```
--8<-- "snippets/sen_gen_json_package_fom.sh"
```

#### Combine multiple JSON schemas to create a kernel configuration schema

```
--8<-- "snippets/sen_gen_json_schema.sh"
```

## Archiving Utility

Helps you interact with archives.

```title="sen archive"
--8<-- "snippets/sen_archive.sh"
```

### Basic information

```title="sen archive info"
--8<-- "snippets/sen_archive_info.sh"
```

### Indexed objects

```title="sen archive indexed"
--8<-- "snippets/sen_archive_indexed.sh"
```

## Packaging Utility

```title="sen package"
--8<-- "snippets/sen_package.sh"
```

### Package skeleton creation

```title="sen package init"
--8<-- "snippets/sen_package_init.sh"
```

### Component skeleton creation

```title="sen package init-component"
--8<-- "snippets/sen_package_init-component.sh"
```

## File to array utility

```title="sen fileToArray"
--8<-- "snippets/sen_fileToArray.sh"
```
