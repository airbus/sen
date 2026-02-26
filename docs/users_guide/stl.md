# Defining Interfaces with the Sen Type Language

Sen has its own little language for you to define your interfaces in a convenient way.

This is an example:

```rust title="person.stl"
package school;

class Person
{
  var firstName     : string [static];
  var surName       : string [static]; 
  var brainActivity : f32;  

  // method that takes a string and returns another
  // @param question the thing that you want to ask
  fn ask(question : string) -> string;

  // an event that requires confirmed transport
  // @param words what the person wants to say
  event saidSomething(words : string) [confirmed];
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
one package declaration per STL files. Multiple STL files might declare the same package. For
example:

```title="package declaration"
import "stl/sen/kernel/basic_types.stl"

package sen.kernel;

// .. types
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
| `string`    | ASCII string.             | `std::string` |
| `TimeStamp` | A point in time.          | `TimeStamp`   |
| `Duration`  | A time duration.          | `Duration`    |

## Sequences

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

## Variants

The variant type represents a type-safe union. An instance of variant at any given time either holds
a value of one of its alternative types, or in the case of error - no value. As with unions, if a
variant holds a value of some object type T, the object representation of T is allocated directly
within the object representation of the variant itself. Variant is not allowed to allocate
additional (dynamic) memory. A variant is *not* permitted to hold the same type more than once.

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

## Quantities

You can use strongly-defined quantity types, with units and optional ranges. For example,

```
quantity<f64, deg> Lat [min: -90.0,  max: 90.0];
quantity<f64, deg> Lon [min: -180.0, max: 180.0];

quantity<f32, m_per_s> Speed;
quantity<f32, rad> Angle;
quantity<f32, m> Meters;
```

## Optional values

You can define types that might optionally hold a value (of any given type). For example,

```
optional<f64> MaybeFloat64;
optional<Error> MaybeError;
```

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

If a class is marked as "abstract" it means that we demand an implementation for it. The Sen kernel
will refuse to instantiate an abstract class without an implementation.

There are 3 kinds of members that a class can have: properties, methods and events.

Methods are defined as follows:

```rust
fn <name>([<arg_name> : <arg_type>]...) [ -> [return_type]] [<attribute>...]; 
```

You can decorate methods with the following attributes:

- *const*: Means that the method call does not change the state of the object.
- *confirmed*: Means that the transport is reliable (the default for methods).
- *bestEffort*: Means that the transport is done using best-effort mechanisms.
- *local*: The method can only be called within its component context.

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

For example:

Events are defined as follows:

```rust
event <name>([<arg_name> : <arg_type>]...) [<attribute>...];
```

You can decorate events with the following attributes:

- *confirmed*: Means that the transport is reliable (events are best-effort by default).
- *bestEffort*: Means that the transport is done using best-effort mechanisms.

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

Properties are defined as follows:

```rust
var <name> : <type> [<attribute>...];
```

You can decorate properties with the following attributes:

- *confirmed*: Means that the transport is reliable (events are best-effort by default).
- *bestEffort*: Means that the transport is done using best-effort mechanisms.
- *static*: Means that the property does not change during the life-time of the object.
- *static_no_config*: Means that the property does not change during the life-time of the object,
  and it cannot be set via configuration parameters (only with code).
- *writable*: Means that the property can be set externally (has a public setter).
- *tag: your_tag*: This can be done multiple times. You can inspect tags at runtime.

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

NOTE: Classes cannot be used for properties, arguments, function returns, struct fields, etc. This
is because they are not considered to be "value" types. Value types are those that can only contain
data, and therefore can be safely transferred between components and over the network in a
self-contained manner. If you need to include a reference to some other object, the best is to use
some kind of identifier (the name, the ID, or some other differentiator). The translation between
this identifier is done by the client using a `SELECT` statement, that way Sen can resolve it to an
object pointer (also allowing for the evaluation multiple data elements if disambiguation is
needed).

### Customizing the generated code

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

NOTE: In the past, we used STL to annotate deferred methods and checked properties, but this led to
a situation where implementation-related customizations were triggering interface updates, even when
there was no effect to the client.

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

## Documentation

This guide explains how to properly document the types supported by the language. You can add
comments in two main ways:

- **Before comments** → placed right before the declaration.
- **Inline comments** → placed at the end of the same line as the declaration.

### General Rules

- Any object that uses **brackets** (e.g., `enumerations`, `structures`, `variants`, `classes`)\
  → supports **before comments** for the declaration itself.\
  → supports **before and inline comments** for elements inside the brackets.

- `sequences`, `optional` and `quantities` → support **before and inline comments**.

```rust
// A structure to represent a point in 2D space
struct Point
{
  // The X coordinate
  x: i32; // horizontal axis
  // The Y coordinate
  y: i32; // vertical axis
}
```

```rust
// A temperature in Celsius
quantity Temperature: f32; // float type
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

**Class Example:**

```rust
// Example class demonstrating documentation
class Example
{
  // Stores the first name
  var firstName : string [static]; // A string property

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
