# STL Language — Formal Grammar Reference

This document is the authoritative reference for the syntax of Sen's interface
definition language (STL). The friendly user-guide introduction lives in
[`stl.md`](stl.md); this file is for tooling authors, users writing non-trivial
definitions, and anyone who needs to resolve a syntax question precisely.

When this document disagrees with the parser (`libs/core/src/lang/stl_parser.cpp`),
the parser is correct — please open an issue so we can reconcile.

---

## 1. Tokens

### 1.1 Keywords (case-sensitive)

| Category              | Keywords                                                       |
| --------------------- |----------------------------------------------------------------|
| File structure        | `import`, `package`                                            |
| Type declarations     | `class`, `struct`, `enum`, `variant`,                          |
| Type modifiers        | `abstract`, `extends`                                          |
| Type constructors     | `sequence`, `array`, `optional`, `quantity`, `alias`           |
| Class body            | `var`, `fn`, `event`                                           |
| Boolean literals      | `true`, `false`                                                |
| Query language        | `SELECT`, `FROM`, `WHERE`, `BETWEEN`, `IN`, `NOT`, `AND`, `OR` |

The query-language keywords (uppercase) belong to Sen's object-query sub-language,
not to STL proper. They are included here because they are reserved in the
shared tokenizer and therefore cannot be used as STL identifiers.

### 1.2 Reserved identifiers

In addition to the keywords above, the parser refuses to let you use any C++
reserved word as an identifier (`auto`, `class`, `const`, `if`, `while`, …).
The full list lives in `StlParser::checkNotReserved` in
`libs/core/src/lang/stl_parser.cpp`.

### 1.3 Attribute names

Recognised inside attribute brackets `[...]`:

| Context    | Names                                                                      |
| ---------- |----------------------------------------------------------------------------|
| Property   | `static`, `static_no_config`, `writable`, `confirmed`, `bestEffort`, `tag` |
| Method     | `const`, `confirmed`, `bestEffort`, `local`                                |
| Event      | `confirmed`, `bestEffort`                                                  |

`tag` takes a value: `tag: my_tag_name`. It may appear multiple times in the
same attribute list. Attribute values are either a literal or an identifier.

### 1.4 Built-in types

| Type        | C++ mapping        |
| ----------- | ------------------ |
| `u8`        | `uint8_t`          |
| `u16`       | `uint16_t`         |
| `u32`       | `uint32_t`         |
| `u64`       | `uint64_t`         |
| `i16`       | `int16_t`          |
| `i32`       | `int32_t`          |
| `i64`       | `int64_t`          |
| `f32`       | `float`            |
| `f64`       | `double`           |
| `bool`      | `bool`             |
| `string`    | `std::string`      |
| `TimeStamp` | `sen::TimeStamp`   |
| `Duration`  | `sen::Duration`    |

`i8` does not exist — use `u8` if you need an 8-bit integer.

### 1.5 Operators and punctuation

| Symbol(s) | Role                                                                         |
| --------- |------------------------------------------------------------------------------|
| `{` `}`   | Type-body delimiters                                                         |
| `[` `]`   | Attribute list delimiters                                                    |
| `<` `>`   | Type-parameter delimiters (`sequence<T>`, `array<T, N>`, ...)                |
| `(` `)`   | Method / event parameter list delimiters                                     |
| `,`       | List separator (fields, enum variants, type args, parameters)                |
| `.`       | Qualified-name separator (`sen.kernel.BuildInfo`)                            |
| `:`       | Type annotation (`name : type`), struct/class inheritance, enum storage type |
| `;`       | Statement terminator                                                         |
| `->`      | Method return type                                                           |

### 1.6 Literals

* **String** — double-quoted (`"..."`) or single-quoted (`'...'`). No escape
  sequences are currently recognised; an unterminated literal is a lex error.
* **Integer** — one or more decimal digits, optionally preceded by `-`.
  Hexadecimal, octal, binary, exponent and suffix forms are not supported.
  Stored as `int64_t`.
* **Float** — decimal integer part, `.`, decimal fractional part
  (e.g. `1.5`, `-0.25`). The decimal point is required to distinguish from an
  integer. Stored as `double`.
* **Boolean** — `true` or `false`.

### 1.7 Identifiers

```
identifier ::= [A-Za-z_] [A-Za-z0-9_]*
```

Qualified names are a sequence of identifiers separated by `.`:

```
qualifiedName ::= identifier ( '.' identifier )*
```

Qualified names appear in `package` declarations, type references, struct
inheritance (`struct Child : parent.BaseType`) and class inheritance
(`class Child : extends parent.BaseType`).

### 1.8 Comments

* **Line comment** — `// ...` runs to end-of-line.
* **Block comments are not supported.**

Comments attached to a declaration (on the line before or on the same line
after `;`) are captured by the parser as the declaration's description and
made available through the meta-reflection API.

---

## 2. File structure

An STL file has three ordered sections:

```
stl ::=  importStatement*
         packageDeclaration
         topLevelDeclaration*

importStatement     ::= 'import' stringLiteral

packageDeclaration  ::= 'package' qualifiedName ';'

topLevelDeclaration ::= structDecl
                      | classDecl
                      | enumDecl
                      | variantDecl
                      | sequenceDecl
                      | arrayDecl
                      | optionalDecl
                      | aliasDecl
                      | quantityDecl
```

Exactly one `package` declaration is required and it must precede all
declarations. Multiple files may declare the same package.

Imports accept both `.stl` paths (Sen files) and `.xml` paths (HLA FOM
documents). Paths are resolved against the Sen include search path.

---

## 3. Declarations

### 3.1 Struct

```
structDecl   ::= 'struct' identifier structParent? ( structBody | ';' )
structParent ::= ':' qualifiedName
structBody   ::= '{' ( structField ( ',' structField )* ','? )? '}'
structField  ::= identifier ':' typeExpr
```

Struct inheritance uses `:` followed directly by the parent name. A trailing
comma after the last field is accepted. An empty struct uses the semicolon
form (`struct Empty;`) with no body braces.

### 3.2 Class

```
classDecl      ::= 'abstract'? 'class' identifier classParents? classBody
classParents   ::= ':' classParent ( ','? classParent )*
classParent    ::= 'extends' qualifiedName
classBody      ::= '{' classMember* '}'
classMember    ::= propertyDecl | methodDecl | eventDecl
```

Class inheritance starts with `:`, then one or more `extends` clauses (only
one `extends` is permitted). Separating commas between clauses are optional.

Examples:

```
class Foo { ... }
class Foo : extends Bar { ... }
```

### 3.3 Class members

```
propertyDecl   ::= 'var' identifier ':' typeExpr attributeList? ';'
methodDecl     ::= 'fn' identifier '(' paramList? ')' ( '->' typeExpr )? attributeList? ';'
eventDecl      ::= 'event' identifier '(' paramList? ')' attributeList? ';'
paramList      ::= param ( ',' param )*
param          ::= identifier ':' typeExpr
attributeList  ::= '[' attribute ( ',' attribute )* ']'
attribute      ::= identifier ( ':' ( literal | identifier ) )?
```

A method with no `-> type` returns `void`. Properties do not support default
values; initial values come from the component's YAML configuration (or,
less commonly, from the implementation itself at construction time).
Properties declared with `[static_no_config]` can only be set from the
implementation.

### 3.4 Enum

```
enumDecl     ::= 'enum' identifier ':' typeExpr enumBody
enumBody     ::= '{' enumVariant ( ',' enumVariant )* ','? '}'
enumVariant  ::= identifier
```

The grammar accepts any type expression as the enum's storage type, but in
practice it must resolve to a built-in integer (`u8`, `u16`, `u32`, `u64`,
`i16`, `i32`, `i64`) — the type-resolver rejects anything else. Variants are
named-only; explicit numeric values are not supported.

### 3.5 Variant

```
variantDecl    ::= 'variant' identifier '{' variantElement ( ',' variantElement )* ','? '}'
variantElement ::= qualifiedName
```

A `variant` holds exactly one of the listed types at runtime. Types must be
distinct.

### 3.6 Alias

```
aliasDecl ::= 'alias' identifier typeExpr ';'
```

The two names appear side-by-side, no `=` between them:

```
alias DeviceId   u64;
alias NameList   sequence<string>;
```

### 3.7 Quantity

```
quantityDecl ::= 'quantity' '<' typeExpr ',' identifier '>' identifier attributeList? ';'
```

The first type argument is the underlying numeric type, the second is the
unit **abbreviation** (short form). The resolver looks the second argument up
with `UnitRegistry::searchUnitByAbbreviation`, so `m`, `deg`, `kph`, `degC`
are valid but the long names (`meter`, `degree`, `km_per_hour`, `centigrade`)
are not. See [Appendix A](#appendix-a--registered-quantity-units) for the
full list.

```
quantity<f32, degC>     Temperature  [min: -273.15];
quantity<f64, m>        Distance     [min: 0.0];
quantity<f32, kph>      GroundSpeed;
quantity<f32, rad>      Angle;
quantity<f32, m_per_s>  Speed;
```

### 3.8 Top-level sequence / array / optional declarations

These give a container a type name so it can be referenced elsewhere:

```
sequenceDecl ::= 'sequence' '<' typeExpr ( ',' integerLiteral )? '>' identifier ';'
arrayDecl    ::= 'array'    '<' typeExpr ','   integerLiteral    '>' identifier ';'
optionalDecl ::= 'optional' '<' typeExpr '>'                         identifier ';'
```

---

## 4. Type expressions

```
typeExpr       ::= qualifiedName typeArguments?
typeArguments  ::= '<' typeArg ( ',' typeArg )* '>'
typeArg        ::= typeExpr | integerLiteral
```

Examples:

```
u32
string
sen.kernel.BuildInfo
sequence<u8>
sequence<string, 16>
array<f32, 4>
optional<sen.kernel.BuildInfo>
```

A bounded `sequence<T, N>` uses stack storage and a runtime length up to `N`;
`array<T, N>` has a fixed compile-time length.

---

## 5. Coding style conventions

These are conventions, not grammar — but the tooling indentation rules and
the shipped syntax-highlighter indentation follow them.

* 2-space indentation; no tabs.
* Allman brace style — opening `{` on its own line.
* Struct / enum members one per line, fields aligned with the longest field
  name, commas separating entries.
* Method / event signatures on a single line; break only if the parameter list
  is unusually long.
* Attribute lists stay on the same line as the member they apply to.
* Line comments (`//`) document members; place them on the same line as the
  member when short, above it when multi-line.

---

## 6. Known mistakes and easy confusions

* **Struct vs class inheritance** — both use `:` to introduce the parent list,
  but structs name the parent directly (`struct Child : Parent`) while classes
  use keyworded clauses (`class Child : extends Parent`).
* **Method return type uses `->`, not `:`** — `:` is for struct fields, method
  parameters, property type annotations and inheritance; returns use `->`.
* **`package` terminates with `;`** — easy to forget because import does not.
* **Properties have no default values** — initialize them from the
  implementation. Don't write `var x : u32 = 5 [static];` even though the
  tokenizer may not complain.
* **`alias` has no `=`** — the name and the aliased type sit side-by-side.
* **`quantity` uses generic-style brackets** — `quantity<base, unit> Name`,
  not `quantity Name : base in unit`.
* **`quantity` unit is the abbreviation, not the long name** — write
  `quantity<f32, m>`, not `quantity<f32, meter>`. Prefixed forms combine the
  prefix abbreviation with the base abbreviation (`km`, `ms`, `us`, `Mpa`).

---

## Appendix A — Registered quantity units

The source of truth is `libs/core/src/meta/unit_registry.cpp`. A
`quantity<T, unit>` declaration looks the second argument up by
**abbreviation** (the short form), so the tables below key on the
abbreviation and list the long name for reference only.

### SI base units (accept all SI prefixes)

Each of the following units is automatically registered with every SI prefix
from `femto` through `peta`, producing the prefixed abbreviation by
concatenation (e.g. `km`, `ms`, `us`, `cm`, `MPa`).

| Category           | Abbreviation  | Long name                      |
| ------------------ | ------------- | ------------------------------ |
| `length`           | `m`           | `meter`                        |
| `time`             | `s`           | `second`                       |
| `angle`            | `rad`         | `radian`                       |
| `temperature`      | `k`           | `kelvin`                       |
| `mass`             | `g`           | `gram`                         |
| `angularVelocity`  | `rad_per_s`   | `radians_per_second`           |
| `density`          | `g_per_cm3`   | `grams_per_centimeters_cube`   |
| `pressure`         | `pa`          | `pascals`                      |
| `area`             | `m_sq`        | `square_meter`                 |
| `force`            | `nw`          | `newton`                       |

**Prefix abbreviations:** `f` (femto), `p` (pico), `n` (nano), `u` (micro),
`m` (milli), `c` (centi), `d` (deci), `da` (deca), `h` (hecto), `k` (kilo),
`M` (mega), `G` (giga), `T` (tera), `P` (peta).

So, for example, `km` is kilometer, `ms` is millisecond, `us` is microsecond,
`cm` is centimeter, `MPa` is megapascal.

### Additional units (no SI prefixes)

| Category              | Abbreviation   | Long name                    |
|-----------------------|----------------|------------------------------|
| `frequency`           | `hz`           | `hertz`                      |
| `velocity`            | `m_per_s`      | `meters_per_second`          |
| `velocity`            | `dm_per_s`     | `decimeters_per_second`      |
| `velocity`            | `kph`          | `km_per_hour`                |
| `velocity`            | `mph`          | `miles_per_hour`             |
| `velocity`            | `kn`           | `knots`                      |
| `velocity`            | `ft_per_s`     | `feets_per_second`           |
| `velocity`            | `ft_per_min`   | `feets_per_minute`           |
| `acceleration`        | `m_per_s_sq`   | `meters_per_second_squared`  |
| `angularAcceleration` | `rad_per_s_sq` | `radians_per_second_squared` |
| `time`                | `min`          | `min` (minute)               |
| `time`                | `hour`         | `hour`                       |
| `time`                | `day`          | `day`                        |
| `time`                | `week`         | `week`                       |
| `time`                | `month`        | `month`                      |
| `time`                | `year`         | `year`                       |
| `length`              | `ft`           | `foot`                       |
| `length`              | `mi`           | `mile`                       |
| `length`              | `nmi`          | `nauticalMile`               |
| `angle`               | `deg`          | `degree`                     |
| `angle`               | `arcmin`       | `arcminute`                  |
| `angle`               | `arcsec`       | `arcsecond`                  |
| `temperature`         | `degC`         | `centigrade`                 |
| `temperature`         | `degF`         | `fahrenheit`                 |
| `angularVelocity`     | `deg_per_s`    | `degrees_per_second`         |
| `angularVelocity`     | `rpm`          | `revolutions_per_min`        |
| `mass`                | `lb`           | `pound`                      |
| `density`             | `kg_per_m3`    | `kilograms_per_meters_cube`  |
| `torque`              | `Nm`           | `newton_meter`               |

> Note: the Celsius scale is registered under the long name `centigrade`
> (abbreviation `degC`); `celsius` is not a valid identifier.
