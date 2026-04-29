# The Sen Type Language (STL)

STL is Sen's small declarative language for describing the shape of
components: their configurable data, their events, and the operations they
expose.

> Looking for the exact syntax rules? See the [formal grammar reference](stl_grammar.md)
> for tokens, EBNF production rules, and the full list of registered quantity units.

This is an example:

```rust title="temperature_sensor.stl"
package sensors;

// Periodically reports temperature from a configured hardware source.
class TemperatureSensor
{
  var modelName  : string [static];   // configured once per instance
  var sampleRate : f32    [writable]; // sampling rate in Hz

  // Read the current temperature, in degrees Celsius.
  fn read() -> f32 [const];

  // Emitted whenever a new sample is available.
  // @param value the freshly sampled temperature
  event sampleReady(value: f32) [confirmed];
}
```

## File structure

The structure of STL files is as follows:

1. Imports
2. Package name
3. Types

Imports are a way to include other STL files, so you can use those definitions. For example:

```title="import statement"
import "stl/sen/kernel/basic_types.stl"

// ... rest of the file
```

You can import as many STL files as you need.

STL files specify a *package*, which is a namespace where the types will be held. There can only be
one package declaration per STL file. Multiple STL files might declare the same package. For
example:

```title="package declaration"
import "stl/sen/kernel/basic_types.stl"

package sen.kernel;

// .. types
```

To use a type defined in another package, import the STL file that declares it
and reference the type by its qualified name (`<package>.<typename>`):

```rust title="using a type from another package"
import "stl/sen/kernel/basic_types.stl"

package my_app;

struct ComponentHealth
{
  name  : string,
  build : sen.kernel.BuildInfo // qualified name
}
```

## Basic types

Sen defines the following basic types:

| Name        | Description               | C++           |
| ----------- | ------------------------- | ------------- |
| `u8`        | 8-bit unsigned integral.  | `uint8_t`     |
| `u16`       | 16-bit unsigned integral. | `uint16_t`    |
| `i16`       | 16-bit integral.          | `int16_t`     |
| `u32`       | 32-bit unsigned integral. | `uint32_t`    |
| `i32`       | 32-bit integral.          | `int32_t`     |
| `u64`       | 64-bit unsigned integral. | `uint64_t`    |
| `i64`       | 64-bit integral.          | `int64_t`     |
| `f32`       | 32-bit floating point.    | `float`       |
| `f64`       | 64-bit floating point.    | `double`      |
| `bool`      | Boolean.                  | `bool`        |
| `string`    | Character string.         | `std::string` |
| `TimeStamp` | A point in time.          | `TimeStamp`   |
| `Duration`  | A time duration.          | `Duration`    |

## Literals

Literals appear in attribute values (`[min: -90.0]`, `[tag: my_tag]`) and
inside enum storage declarations. STL's literal syntax is deliberately
minimal:

- **Strings** are double-quoted (`"hello"`) or single-quoted (`'hello'`).
  Escape sequences (`\n`, `\t`, ...) are **not** interpreted; the characters
  between the quotes are taken literally.
- **Integers** are decimal only, optionally prefixed with `-`. Hexadecimal
  (`0x...`), octal, binary, exponent forms, and type suffixes are not
  supported.
- **Floats** require a decimal point. Write `1.0`, not `1`; the decimal
  point is what distinguishes a float from an integer.
- **Booleans** are `true` and `false`.

## Value types vs classes

Most of STL is about **value types**: types whose instances can be copied
and transferred wholesale between components, across the network, or into
storage. The following are value types:

- Basic types (`u32`, `bool`, `string`, `TimeStamp`, `Duration`, ...)
- Containers (`sequence`, `array`, `optional`, `quantity`)
- Custom types declared with `enum`, `struct`, `variant`, `alias`

**Classes are not value types.** A class represents a live element with
identity and behavior; it cannot appear as a property type, a struct field,
a variant alternative, a method parameter, or a return value. When you need
to reference another object from a value-typed context, store an identifier
(a name, an ID, or some other differentiator) and resolve it at query time
with a `SELECT` statement.

## Sequences and arrays

Sequences are lists of elements. They can be bounded or unbounded and store any value type.

| Name                | Description               | C++                          |
| ------------------- | ------------------------- | ---------------------------- |
| `sequence<T>`       | Unbounded sequence of T.  | `std::vector<T>`             |
| `sequence<T, size>` | Bounded sequence of T.    | `sen::StaticVector<T, size>` |
| `array<T, size>`    | Fixed size sequence of T. | `std::array<T, size>`        |

For example, you can define an unbounded sequence as follows:

```title="unbounded sequence of f32"
sequence<f32> MyUnboundedSequence;
```

Unbounded sequences have variable size and unlimited capacity. This means that they can grow and
shrink, and their contents are limited by the amount of available memory. They use the heap.

```title="bounded sequence of f32, with a maximum of 100 elements"
sequence<f32, 100> MyBoundedSequence;
```

Bounded sequences have fixed capacity and variable size. This means that you can add and remove
elements, but only until you reach the maximum capacity. They use the stack.

Note that bounded sequences have similarities with arrays (fixed capacity, stack usage), but behave
like vectors (or lists), in the sense that they have a size which increases or decreases when you
add or remove elements.

Arrays, by contrast, have fixed length and do not grow or shrink:

```title="fixed-size array of three f32"
array<f32, 3> Vec3;
```

## Optional values

You can define types that might optionally hold a value (of any given type). For example,

```
optional<f64> MaybeFloat64;
optional<Error> MaybeError;
```

## Quantities

You can use strongly-defined quantity types, with units and optional ranges. For example,

```
quantity<f64, deg> Lat [min: -90.0,  max: 90.0];
quantity<f64, deg> Lon [min: -180.0, max: 180.0];

quantity<f32, m_per_s> Speed;
quantity<f32, rad> Angle;
quantity<f32, m> Meters;
```

The second type argument is the **unit abbreviation** (`m`, `rad`, `deg`, `kph`,
`degC`, ...), not the long form. SI base units are also registered with every
metric prefix, so `km`, `ms`, `us`, `cm`, `MPa` all resolve out of the box.

See the
[full list of registered units](stl_grammar.md#appendix-a--registered-quantity-units)
for what is currently available.

Quantities accept an attribute list containing `min:` and/or `max:` bounds,
as shown above.

## Aliases

Aliases give an existing type a new name. They are useful for making intent clear,
or for hiding implementation details behind a domain-specific name. The alias
is structural - `DeviceId` below is interchangeable with `u64` wherever it is
used.

```
alias <name> <type>;
```

Note that there is **no `=`** between the alias name and the aliased type; the
two identifiers sit side-by-side.

For example,

```rust
alias DeviceId       u64;
alias NameList       sequence<string>;
alias CoordinatePair array<f64, 2>;
alias HostBuildInfo  sen.kernel.BuildInfo; // aliasing a struct from another package
```

Aliases shine once you've declared your own structs, variants, and enums:
any of them can be given a second name for clarity at the call site.

## Enumerations

In STL, enumeration types are defined like this:

```
enum <type_name>: <storage>
{
  <enumeration>,
  ...
}
```

For example,

```c++ title="Example of an enumeration type"
// Category of a problem during component execution
enum ErrorCategory: u8
{
  runtimeError,       // same semantics of std::exception
  logicError,         // faulty logic such as violating logical preconditions or invariants
  expectationsNotMet, // expectations on inputs or internal state were not met
  ioError,            // problem while performing an I/O operation
  other               // any other reason
}
```

The storage type must be an integral (`u8`,`u16`,`i16`,`u32`,`i32`, `u64` or `i64`).

## Structures

You can group values with structs. They are defined like this:

```
struct <type_name>
{
  <field_name> : <field_type>,
  ...
}
```

Fields can be of any type and names must be unique.

For example:

```rust
// Build-related information
struct BuildInfo
{
  maintainer: string,    // principal maintainer of the software
  version   : string,    // version string (format-agnostic)
  compiler  : string,    // vendor-specific compiler string
  debugMode : bool,      // compiled in debug mode or not
  buildTime : string,    // when did this build took place
  wordSize  : WordSize,  // architecture
  gitRef    : string,    // git ref spec
  gitHash   : string,    // git hash
  gitStatus : GitStatus  // git status
}
```

In C++, structs are (unsurprisingly) rendered as `struct`.

If your struct does not have any field, you can omit the `{` `}`. For example,

```rust
struct SaveCursorPosition;
struct RestoreCursorPosition;
```

Furthermore, structs may have a parent struct to avoid code duplication. For example,

```rust
struct ParentStruct
{
  familyName : string,
  hairColor : string,
  badHabits : sequence<BadHabit>
}

struct ChildStruct : ParentStruct
{
  name : string,
  age : u8,
}
```

*Note:* structs that specify a parent always declare a `is-a` relationship
to their parent. That is, as structs do not have any invariants all data members from
the parent will be available to every user of the derived class. Furthermore, a struct
with a parent is a class that requires run-time polymorphism and should, therefore, also
be treated as such in code. We currently strongly discourage the polymorphic usage of
structs as parent structs do have a virtual constructor.

## Variants

The variant type represents a type-safe union. A variant instance always
holds a value of exactly one of its alternative types. As with unions, the
object representation of the held type is allocated directly within the
variant's own object representation, with no additional (dynamic) memory
involved. A variant is **not** permitted to list the same type more than
once.

They are defined as follows:

```
variant <name>
{
  <type>,
  ...
}
```

Types must be unique within the variant.

For example,

```rust
variant CustomTypeData
{
  EnumTypeSpec,
  QuantityTypeSpec,
  SequenceTypeSpec,
  StructTypeSpec,
  VariantTypeSpec,
  AliasTypeSpec,
  ClassTypeSpec
}
```

Note that variants can be used as "enumerations with state". For example:

```rust
struct HideCursor;
struct ShowCursor;
struct SaveCursorPosition;
struct RestoreCursorPosition;
struct MoveCursorLeft { cells: u32 }
struct MoveCursorRight { cells: u32 }
struct MoveCursorUp { cells: u32 }
struct MoveCursorDown { cells: u32 }
struct Print { text : string }

struct CPrint
{
  flags : u32,
  color : u32,
  text  : string
}

variant TerminalCommand
{
  HideCursor,
  ShowCursor,
  SaveCursorPosition,
  RestoreCursorPosition,
  MoveCursorLeft,
  MoveCursorRight,
  MoveCursorUp,
  MoveCursorDown,
  Print,
  CPrint
}
```

In C++, variants are rendered as `std::variant<...>`.

## Classes

You can define classes as follows:

```
[abstract] class <name> [: extends <parent_class>]
{
   <members>...
}
```

For example:

```rust
class MyClass
{
}

class MySubClass : extends MyClass
{
}
```

A class may `extends` at most one parent class.

If a class is marked as `abstract`, the Sen kernel will refuse to instantiate
it without a dedicated C++ implementation. Non-abstract classes can be
instantiated directly from configuration.

There are 3 kinds of members that a class can have: properties, methods and events.

**Methods** are defined as follows:

```rust
fn <name>([<arg_name> : <arg_type>]...) [ -> [return_type]] [<attribute>...];
```

For example,

```rust
class Example
{
  // a method with no arguments (that doesn't return)
  fn methodWithoutArgs() [const, bestEffort];

  // a method that returns a string
  fn methodThatReturns() -> string [const];

  // adds two integrals
  fn add(leftHandSide: i32, rightHandSide: i32) -> i32;
}
```

**Events** are defined as follows:

```rust
event <name>([<arg_name> : <arg_type>]...) [<attribute>...];
```

For example,

```rust
class Example
{
  // an event without arguments
  event somethingHappened();

  // an event with arguments
  event somethingElseHappened(what: string, count: u32);
}
```

**Properties** are defined as follows:

```rust
var <name> : <type> [<attribute>...];
```

For example,

```rust
class Person
{
  var firstName     : string [static];
  var surName       : string [static];
  var kind          : string [static_no_config];
  var stressLevel   : f32    [writable];
  var temperature   : f32    [writable];
  var brainActivity : f32;
}
```

The attribute vocabulary for properties, methods, and events is listed in the
[Attributes](#attributes) section. Remember that property types, method
parameters, and return types must be [value types](#value-types-vs-classes).

## Attributes

Attributes decorate a declaration with metadata. They appear in square brackets
at the end of the declaration, before the `;`, and are separated by commas.

Most attributes are **flags**: their name on its own turns the flag on. A few
take a value, written as `name: value` (e.g. `min: 0.0`, `tag: my_tag`).

The table below lists every attribute and which declarations it applies to.

| Attribute            |  Property  |  Method  |  Event   | Quantity  | Meaning                                                                                   |
|----------------------|:----------:|:--------:|:--------:|:---------:|-------------------------------------------------------------------------------------------|
| `static`             |     ✓      |          |          |           | The property never changes during the lifetime of the object.                             |
| `static_no_config`   |     ✓      |          |          |           | Static and cannot be set from YAML configuration; only from the implementation.           |
| `writable`           |     ✓      |          |          |           | The property has a public setter (it can be set externally).                              |
| `confirmed`          |     ✓      |    ✓     |    ✓     |           | Transport is reliable. Default for methods; opt-in for properties and events.             |
| `bestEffort`         |     ✓      |    ✓     |    ✓     |           | Transport uses best-effort mechanisms. Default for events; opt-in for methods/properties. |
| `const`              |            |    ✓     |          |           | The method does not change the state of the object.                                       |
| `local`              |            |    ✓     |          |           | The method can only be called within its component context.                               |
| `tag: <name>`        |     ✓      |          |          |           | User-defined tag, inspectable at runtime. May appear multiple times.                      |
| `min: <literal>`     |            |          |          |     ✓     | Lower bound for the quantity's value.                                                     |
| `max: <literal>`     |            |          |          |     ✓     | Upper bound for the quantity's value.                                                     |

Example combining several:

```rust
var stressLevel : f32 [writable, tag: bio, tag: humanitarian];
fn probe() -> string [const, local];
event beaconFired() [confirmed];
```

Properties do **not** support a default-value syntax (`var x : u32 = 5 [static];` is
not valid). Initial values come from the component's YAML configuration, or from
the implementation at construction time.

### Tagging properties

The `tag: <name>` attribute is the mechanism for user-defined labelling of
properties. Tags are inspectable at runtime through Sen's meta-reflection API
and let you group properties across unrelated classes for filtering,
selection, or UI purposes, without those classes having to share a common
ancestor. A property may carry multiple tags:

```rust
class Patient
{
  var heartRate     : f32       [writable, tag: vital, tag: continuous];
  var bloodPressure : f32       [writable, tag: vital];
  var lastVisit     : TimeStamp [tag: audit];
}

class Incubator
{
  var internalTemp  : f32 [writable, tag: vital, tag: continuous];
}
```

A query can then ask for "every `vital`-tagged property across the system"
and get back `heartRate`, `bloodPressure`, and `internalTemp` in a single
result, regardless of which class they belong to.

## Customizing the generated code

If you are generating C++ code, there are some knobs you can use to customize the output:

- *Checked properties*: If you have a `writable` property, you can tell Sen to first ask for your
  approval when someone attempts to do a "set" to it.
- *Deferred methods*: If you mark a method as deferred, the generated code will allow you to
  postpone the execution of calls by providing a `std::future` that can be set by you whenever you
  decide.

To generate the code in this way, you use a JSON file that may look as follows:

```json
{
  "classes": {
    "my_package.MyClass": {
      "deferredMethods": [
        "doingSomethingDeferred",
        "doingSomethingDeferredWithoutReturning"
      ],
      "checkedProperties": [
        "prop7"
      ]
    }
  }
}
```

## Documenting STL files

You can add comments to STL files in two main ways:

- **Before comments** → placed right before the declaration.
- **Inline comments** → placed at the end of the same line as the declaration.

Only line comments (`//`) are supported; there is no block-comment syntax.

### General rules

- Any object that uses **brackets** (e.g., `enumerations`, `structures`, `variants`, `classes`)\
  → supports **before comments** for the declaration itself.\
  → supports **before and inline comments** for elements inside the brackets.

- `sequences`, `optional` and `quantities` → support **before and inline comments**.

```rust
// A structure to represent a point in 2D space
struct Point
{
  // The X coordinate
  x: i32,

  y: i32 // The Y coordinate
}
```

```rust
// An angle in radians
quantity<f32, rad> Angle;

quantity<f32, rad> Angle; // An angle in radians
```

### Classes

In addition to the general documentation rules, classes have additional special documentation for
methods and events.

As mentioned in the general rules, they both can have a comment block before their declaration.
Within this block, and after the description, the parameters can be documented:

```rust
// @param <name> [comment]→ describes a parameter.
```

You can add multiple lines of comments, but only one @param per parameter.

**Class example:**

```rust
// Example class demonstrating documentation
class Example
{
  // Stores the first name
  var firstName : string [static];

  // Stores the surname
  var surName   : string [static];

  // An event triggered when something happens
  // @param what A description of what happened
  // @param count Number of times it occurred
  event somethingElseHappened(what: string, count: u32); // Example event

  // This is just an example method
  // @param example1 This parameter is a string
  // You can add more description here of the parameter, but do not repeat @param example1
  // @param example2: This parameter is also a string
  fn exampleMethod(example1: string, example2: string) -> string;
}
```

## Syntax highlighting

Please have a look at the `resources/syntax_highlighting/stl` folder in the Sen repo.

## Importing HLA-FOM-defined types

Sometimes it is convenient to reuse HLA-defined types in your STL. You can include HLA FOM XMLs from
STL files:

```rust
package se;

import "fom/rpr/RPR-Physical_v2.0.xml"

// An Entity model.
struct EntityModel
{
  name: string,               // Name of this model.
  type: rpr.EntityTypeStruct  // Type of entity represented by this model.
}
```

You can see that the `rpr.EntityTypeStruct` is available. The same happens with all the other types
in the XML.

Keep in mind that Sen (the STL parser) expects the HLA XML files to follow a certain file layout.

```
- root
  - module_1
    - file_1.xml
    - file_2.xml
  - module_2
    - file_3.xml
    - file_4.xml
  ...
  - module_n
    - file_5.xml
    - file_6.xml

  - mapping_1.xml
  - mapping_2.xml
  - ...
  - mapping_n.xml
```

This is needed because the XML files may depend on each other, and Sen needs to be able to find
them.

Regarding mappings, have a look at the documentation about specifying objects with HLA.

## Quick reference

One-line form of every declaration kind, for quick recall:

```rust
package example;
import "stl/sen/kernel/basic_types.stl"

// Aggregates
enum Status: u8    { active, idle, error }
struct Point       { x: f32, y: f32 }
struct Empty;
variant Shape      { Point, Line, Circle }

// Type constructors
sequence<u8>         ByteStream;
sequence<u8, 1024>   BoundedBuffer;
array<f32, 3>        Vec3;
optional<string>     MaybeName;
quantity<f32, m>     Distance   [min: 0.0];
alias DeviceId       u64;

// behavior
abstract class MyService : extends Base
{
  var name  : string [static];
  var count : u32    [writable, tag: counter];
  fn process(input: string) -> bool [const];
  event pinged();
}
```

## Examples

```rust title="teacher.stl"
import "stl/school/person.stl"

package school;

struct ImpartingClass
{
  since     : TimeStamp,
  className : string
}

struct WaitingForStudents
{
  since : TimeStamp
}

variant TeacherStatus
{
  WaitingForStudents,
  ImpartingClass
}

class Teacher: extends Person
{
  var status      : TeacherStatus [confirmed];
  var stressLevel : f32;

  fn assignTasks();

  event stressLevelPeaked(level : f32);
}
```

```rust title="student.stl"
import "stl/school/person.stl"

package school;

struct Sleeping
{
  since          : TimeStamp,
  snortingVolume : f32
}

struct DoingSomething
{
  since      : TimeStamp,
  taskName   : string,
  difficulty : f32,
  progress   : f32
}

struct DoingNothing
{
  since : TimeStamp
}

variant StudentStatus
{
  DoingNothing,
  DoingSomething,
  Sleeping
}

class Student: extends Person
{
  var status     : StudentStatus [confirmed];
  var focusLevel : f32;

  fn startDoingTask(taskName: string, difficulty: f32) -> bool;

  event madeSomeNoise(noise: string, volume: f32);
  event gotDistracted(reason: string);
}
```

```rust title="class_room.stl"
package school;

class ClassRoom
{
  var createTeacher : bool   [static];
  var defaultSize   : u32    [static];
  var studentsBus   : string [static];
  var teacherName   : string [confirmed];

  fn addStudents(count: u32);
  fn removeStudents(count: u32);
}
```
