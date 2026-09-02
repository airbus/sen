# Compatibility conversions

Sen automatically assists users in making diverging `STL` specifications compatible by converting
the underlying data from what the senders representation is to what the receiver expects. This way,
communication is still possible even when the senders and receivers type specifications do not match
exactly, for example, because a legacy service has not been upgraded to the new version of an `STL`.

## Basic types

In general, these are the conversion rules for numeric types:

- **Clamping**: when the value does not fit the receiver type, it is clamped to that type's nearest
  bound. It is *not* wrapped or bit-truncated. Sending `1000` to a `u8` receiver yields `255`, not
  `232`; sending `-1` yields `0`. *Lossy for integer and floating-point types*.
- **Widening**: when the receiver type is larger, the result is widened. Integer to larger integer
  and `f32` to `f64` are exact. Converting an integer *into* a floating-point type is exact only
  while the value stays within the range that type represents exactly, so very large integers lose
  precision. *Lossy only in that last case*.
- **Signedness**: when the receiver has a different signedness, the value is clamped to the
  receiver's range by the same rule: a negative value sent to an unsigned receiver arrives as `0`.
  *Lossy for integer types*.
- **Rounding**: when the decimal values of the senders value cannot be represented in the receiver
  type, they will be rounded. *Lossy for integer and floating-point types*.
- **Stringification**: All Sen types can be transformed to a string. Native types are stringified
  with the usual operators (e.g. transformation of a number to string) and custom types are
  transformed to JSON format.

These conversions are not guaranteed to be lossless. Sen reduces conversion loss where it can and
does lossy conversions where it must: sending a `u32` to a receiver that expects `u16` delivers any
value above `65535` as `65535`.

How much of that you accept is a setting. The kernel's `compatibility` key takes one of three
values:

| `compatibility` | Behavior |
|---|---|
| `relaxed` | Conversions happen, and a conversion that can lose data is warned about, naming both types. The default. |
| `strict` | Conversions happen, but one that can lose data is refused. |
| `disabled` | Only an exact type match is accepted. |

`relaxed` is the default so that a system that works today keeps working, and starts telling you
where it is losing data. A rig whose numbers have to be trusted should run `strict`.

| Source \\ Target              | bool               | integral           | floating point     | string         | duration                | timestamp                          |
| ----------------------------- | ------------------ | ------------------ | ------------------ | -------------- | ----------------------- | ---------------------------------- |
| **bool (false)**              | `false`            | `0`                | `0.0`              | `"false"`      | :fontawesome-solid-ban: | :fontawesome-solid-ban:            |
| **bool (true)**               | `true`             | `1`                | `1.0`              | `"true"`       | :fontawesome-solid-ban: | :fontawesome-solid-ban:            |
| **integral (zero)**           | `false`            | `0`                | `0.0`              | `"0"`          | `0 ns`                  | epoch                              |
| **integral (non-zero)**       | `true`             | numeric conversion | numeric conversion | `"<value>"`    | ns                      | ns since epoch                     |
| **floating point (zero)**     | `false`            | `0`                | 0.0                | `"0.000000"`   | `0 ns`                  | epoch                              |
| **floating point (non-zero)** | `true`             | rounding           | numeric conversion | `"<value>"`    | ns                      | ns since epoch                     |
| **string**                    | `"true"` -> `true` | parsing            | parsing            | nothing        | ns (if numeric string)  | ns since epoch (if numeric string) |
| **duration**                  | non-zero -> `true` | ns                 | ns                 | ns             | nothing                 | ns since epoch                     |
| **timestamp**                 | non-zero -> `true` | ns since epoch     | ns since epoch     | ns since epoch | ns since epoch          | nothing                            |

Numbers become strings through `std::to_string`, so floating-point values carry six decimal places:
`1.5` arrives as `"1.500000"`.

## Custom types

### Quantities

Quantities can be converted to/from in the following ways:

- **`Quantity <-> Numeric Types`**: Numeric fitting between the underlying type of the quantity and
  the basic type
- **`Quantity <-> String`**: The quantity, including its unit, is stringified
- **`Quantity <-> Quantity`**: If the sender and receiver quantities have different units, a unit
  transformation is performed between them
- **`Quantity <-> Enum`**: Numeric fitting between the underlying type of the quantity and the
  integer value of the enum
- **`Quantity <-> Optional`**: Conversion between the underlying type of the quantity and the type
  of the optional

The transformation only applies within a unit category. Quantities whose units measure different
things, or where one side carries a unit and the other does not, are reported as incompatible
instead of being converted, so a length never arrives as a duration.

### Enums

Enums can be converted to/from in the following ways:

- **`Enum <-> Numeric Types`**: Numeric fitting between the integer value of the enum and the plain
  numeric type. An error is triggered if the integer is not in the set of keys available
- **`Enum <-> String`**: The enumerator name is stringified/parsed. An error is triggered if the
  name does not match
- **`Enum <-> Quantity`**: Numeric fitting between the underlying type of the quantity and the
  integer value of the enum. An error is triggered if the integer is not in the set of keys
  available
- **`Enum <-> Enum`**: Enumerators of the sender and receiver enums are mapped using their names.
  Error if the enumerator name is not found in the receiver enum
- **`Enum <-> Optional`**: Conversion between the integer of the enum and the underlying type of the
  optional

### Structures

Structs can be converted to/from in the following ways:

- **`Struct <-> Struct`**: Fields are mapped using their names, provided the types are compatible.
  Field updates from fields not present in the receiver struct are ignored. All these conversions
  extend through all the hierarchy of the structs (e.g. a property in the sender parent struct can
  be mapped to a property in the receiver child struct)
- **`Struct <-> String`**: Structs are stringified to JSON form
- **`Struct <-> Optional`**: Conversion between the struct and the underlying type of the optional
  if the latter is a struct or a string

### Variants

Variants can be converted to/from in the following ways:

- **`Variant <-> Variant`**: Each sender variant type is transformed into its analogous receiver
  type (matched by variant index), provided the types are compatible. An error is triggered if the
  sender sets its value to an index not present in the receiver variant
- **`Variant <-> String`**: Variants are stringified to JSON form
- **`Variant <-> Optional`**: Conversion between the variant and the underlying type of the optional
  if the latter is a struct or a string

### Optionals

Optionals can be converted to/from in the following ways:

- **`Optional <-> String`**: An empty optional is represented by the "null" string.
- **`Optional <-> All Types`**: Conversion rules applied to the underlying type of the optional

### Sequences

Sequences can be converted to/from in the following ways:

- **`Sequence <-> Sequence`**: If the receiver sequence is smaller, it is truncated: the receiver
  keeps the first elements that fit and the rest are dropped, without an error at the time. The type
  of the elements stored in the sequences need to be compatible. If the sequence is of fixed-size
  (array), elements are converted into the receiver array and remaining elements are discarded the
  same way.

  Note that this differs from the numeric rule above: a number that does not fit is *clamped* to the
  receiver's bound, while a sequence that does not fit is *cut short*. A size mismatch is reported
  as a minor compatibility issue when the two type sets are matched, so it is visible before any
  data flows.
- **`Sequence <-> String`**: Sequences are stringified to JSON form
- **`Sequence <-> Optional`**: Conversion between the sequence and the underlying type of the
  optional, if the latter is a sequence or a string

### Classes

**`Class <-> Class`**:

Properties are adapted to the ones with matching names if types are compatible. Properties only
present in the sender class are ignored by the receiver class.

Methods are adapted to methods with matching names under the following conditions

- Arguments are matched using their names and their types are adapted.
- The receiver method will only be called if it has equal or fewer arguments, discarding excess
  arguments. Otherwise, Sen will trigger an error.
- Return types must be compatible.

Events are adapted to events with matching names under these conditions:

- Arguments are matched using their names, if types are compatible.
- The receiver event will only be emitted if it has equal or fewer arguments, discarding excess
  arguments.

If the class inherits from any parents, these conversions apply to the whole hierarchy. Conversion
between class members does not care about the level of the hierarchy where the member is located
(e.g. a property in a sender parent class can be mapped to a property in a receiver child class)

**`Class <-> String`**:

Classes are stringified to JSON form.

## Changing a class others already use

Everything above matches by name, and that decides what is safe to change once another team builds
against your class.

**Adding is safe.** A new property, method or event is ignored by anyone who does not know it.

**Renaming is a break, and a quiet one.** Names are the only thing conversions match on, so a
renamed property stops arriving and the reader keeps whatever it last had. Removing a property does
the same to anyone still reading it.

**Narrowing a type keeps working and loses data.** A `u32` above 65535 reaches a `u16` as 65535.
Under `relaxed` that is warned about, and under `strict` it is refused.

**Arguments go opposite ways for methods and events.** A method is called only when it takes the
same number of arguments as the caller supplies or fewer, with the extras dropped, so adding an
argument to a method breaks callers built against the old one: they supply too few, and that is an
error. An event is delivered only when the subscriber takes the same number or fewer, so adding an
argument to an event is safe, while removing one silently stops delivery to subscribers that still
expect it.
