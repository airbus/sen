# Compatibility conversions

Sen automatically assists users in making diverging `STL` specifications compatible by converting
the underlying data from what the senders representation is to what the receiver expects. This way,
communication is still possible even when the senders and receivers type specifications do not match
exactly, for example, because a legacy service has not been upgraded to the new version of an `STL`.

## Basic Types

In general, these are the conversion rules for numeric types:

- **Truncation**: when the receiver type is smaller, the result will be truncated to fit into the
  value range of the receiver type. *Lossy for integer and floating-point types*
- **Widening**: when the receiver type is larger, the result will be widened. *Lossy for
  floating-point types*.
- **Signedness**: when the receiver type has a different signedness, the result will be truncated.
  *Lossy for integer types*.
- **Rounding**: when the decimal values of the senders value cannot be represented in the receiver
  type, they will be rounded. *Lossy for integer and floating-point types*.
- **Stringification**: All Sen types can be transformed to a string. Native types are stringified
  with the usual operators (e.g. transformation of a number to string) and custom types are
  transformed to JSON format.

These conversions are not guaranteed to be lossless! Sen tries to reduce conversion loss where
possible but does lossy conversions where necessary. For example, when sending a `u32` to a receiver
that expects `u16`. Here, Sen will truncate sent values to fit into the value range of the receiver
type `u16`.

| Source \\ Target              | bool               | integral           | floating point     | string         | duration                | timestamp                          |
| ----------------------------- | ------------------ | ------------------ | ------------------ | -------------- | ----------------------- | ---------------------------------- |
| **bool (false)**              | `false`            | `0`                | `0.0`              | `"false"`      | :fontawesome-solid-ban: | :fontawesome-solid-ban:            |
| **bool (true)**               | `true`             | `1`                | `1.0`              | `“true”`       | :fontawesome-solid-ban: | :fontawesome-solid-ban:            |
| **integral (zero)**           | `false`            | `0`                | `0.0`              | `"0.0"`        | `0 ns`                  | epoch                              |
| **integral (non-zero)**       | `true`             | numeric conversion | numeric conversion | `"`<value>`"`  | ns                      | ns since epoch                     |
| **floating point (zero)**     | `false`            | `0`                | 0.0                | `"0.0"`        | `0 ns`                  | epoch                              |
| **floating point (non-zero)** | `true`             | rounding           | numeric conversion | `"`<value>`"`  | ns                      | ns since epoch                     |
| **string**                    | `“true”` -> `true` | parsing            | parsing            | nothing        | ns (if numeric string)  | ns since epoch (if numeric string) |
| **duration**                  | non-zero -> `true` | ns                 | ns                 | ns             | nothing                 | ns since epoch                     |
| **timestamp**                 | non-zero -> `true` | ns since epoch     | ns since epoch     | ns since epoch | ns since epoch          | nothing                            |

## Custom Types

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
  Field updates from fields not present in the receiver class are ignored. All these conversions
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

- **`Sequence <-> Sequence`**: If receiver sequence is smaller, it is truncated. The type of the
  elements stored in the sequences need to be compatible. If the sequence is of fixed-size (array),
  elements are converted into the receiver array and remaining elements are discarded.
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
