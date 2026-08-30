# Using HLA FOMs

Sen is able to use HLA FOMs to model types and classes.

## How Sen uses a FOM

Sen uses HLA FOM XML as an **interface definition format**. The code generator reads your FOM
modules at build time and emits Sen types and classes from them, so a shared model can serve as your
ICD instead of an STL. For example, you could use the RPR FOM and Link 16 FOM (SISO standards), NETN
FOM (NATO product), etc.

**Sen is not an RTI and does not natively join HLA federations.** As with any other protocol, that
is done by writing an adapter. The same applies to DIS. Sen participants talk to other Sen
participants over Sen's own transport, and anything outside that is reached through a
bridge.[^hla-adapters] [The mental model](mental_model.md#hla-or-dis) maps the vocabulary across.

[^hla-adapters]: Adapters of this kind do exist, but they are currently internal to Airbus and are
    not part of this distribution. They may be open-sourced in the future; nothing is committed.

Sen builds a C++ representation of what a FOM expresses. It does not reproduce how that FOM is
encoded. Attributes describing the wire format, such as `size`, `endian` and the width behind a
representation, are read where they help pick a type and dropped otherwise, because Sen does not
implement that wire format. Where the standard's type system is redundant Sen collapses it: booleans
are declared as enumerations, in more than one way, and all of them become `bool`.

These are decisions, and they are separate from the list below of things that are not supported yet.
An adapter bridging to a real federation reads the FOM itself for encoding detail, because that is a
transport concern and this layer is an API.

Sen generates no encoder or decoder, and does not need to. Encoding is a wire concern and belongs to
whatever is on the wire: an adapter or gateway does it against the FOM, using the encoding helpers
its RTI provides.

Currently, Sen expects the element layout of the IEEE 1516.2-2010 DIF. Only files with an `.xml`
extension are processed.

These OMT elements are read: `objectClass`, `attribute` (name, `updateType`, `sharing`,
`transportation`), `interactionClass` (name, `transportation`), `parameter`, `basicData`,
`simpleData`, `enumeratedData`, `arrayData`, `fixedRecordData`, `variantRecordData`, units and
semantics.

These are **not supported yet**: `ownership`, `order`, `updateCondition` and `dimensions`. A FOM
declaring `DivestAcquire` on an attribute imports cleanly and that declaration is currently dropped,
so plan on Sen's own ownership model for now; see [Main concepts](main_concepts.md). A FOM declaring
`TimeStamp` order imports the same way; [Time management](#time-management) covers what that means.
These are areas we intend to grow into rather than deliberate omissions.

`<modelIdentification>` gives the model's name, which is how a mappings file names a document, and
its `reference` entries of type Dependency, which are where the module dependency graph comes from.
A dependency naming a document that is not in the set is an error at generation time, and one on
the MIM is ignored. Version, POC, `useLimitation` and `useHistory` do not reach the generated model.

The **MIM** is skipped on purpose. Its two roots are recognized so class hierarchies resolve, and
the rest of it describes managing a federation, which needs an RTI.

Sen can read several module sets at once, each one a directory of FOM files whose name becomes the
Sen package. A name is looked for in the module that mentions it and then through that module's
dependencies, so a class declared in more than one module becomes a single class rather than one
per module. `BaseEntity` is declared in seven of the example modules and comes out as one class in
the package of the directory it was first read from, `rpr.BaseEntity`. The empty classes NETN uses
to reach RPR's hierarchy resolve to it the same way, which is how `netn.NETNAircraft` comes to
extend `rpr.Aircraft`, and 69 of the NETN classes inherit from an RPR one like this.

`transportation` **is** honored, and maps like this:

| FOM `transportation` | Attribute → property | Interaction → method | Interaction → event |
|---|---|---|---|
| `HLAreliable` | reliable, as `[confirmed]` | reliable, as `[confirmed]` | reliable, as `[confirmed]` |
| `HLAbestEffort` | best-effort multicast | best-effort unicast, as `[bestEffort]` | best-effort multicast |

A FOM's `transportation` says whether delivery has to be reliable, and nothing more. Sen's modes say
that too, but they also say whether a message goes to every subscriber or to one, so mapping
`HLAbestEffort` means choosing something the FOM never stated. The table shows what Sen chooses.

`[confirmed]` and `[bestEffort]` are the modes you write in STL. Best-effort multicast is what
a property or event gets when you write neither, which is why those two cells name no attribute.
[Quality of service](mental_model.md#quality-of-service-confirmed-vs-best-effort) describes all
three.

A `transportation` value Sen does not recognize is a hard error at generation time. That means the
OMT's own extension point, where a FOM declares its own transportation types, is not supported.

One more thing before you import a shared FOM: when two participants disagree about a type, Sen
currently adapts rather than refuses, and some of those adaptations lose data quietly: numbers
clamp, sequences truncate. [Run-time compatibility](compatibility_conversions.md) covers what
happens today; read it if your FOM is shared across organizations. How much of this should be
adjustable is still open, so treat the current behavior as the present shape rather than a settled
contract.

Example of a diagram generated by Sen:

![Class diagram generated from the RPR, NETN and Link 16 FOMs](../snippets/fom.svg){: style="width:1200px;"}

You can download the image to get a better look.

Sen can also generate markdown documentation (see the following section).

Put your HLA FOMs XMLs in folders (for example: rpr, netn).

In CMake you can do as follows to generate C++ code:

```cmake
sen_generate_cpp(
  TARGET
    fom_lib
  HLA_FOM_DIRS
    rpr
    netn
  GEN_HDR_FILES
    fom_headers
  HLA_MAPPINGS_FILE
    mapping.xml
)
```

The `HLA_MAPPINGS_FILE` option is for dealing with interactions. More on that later.

You can also generate UML diagrams:

```cmake
sen_generate_uml(
  TARGET
    fom_lib_uml
  OUT
    ${CMAKE_CURRENT_BINARY_DIR}/fom.plantuml
  HLA_FOM_DIRS
    rpr
    netn
  CLASSES_ONLY
)
```

## Time management

Sen's stepped mode is not a system without HLA time management. It is one point inside it, the most
constrained one: under `virtualTime` every kernel is in effect both time-regulating and
time-constrained, with one lookahead and one timestep shared by all of them, and a barrier at every
step.

The services you would reach for all serve the same purpose. Per-federate lookahead, LBTS, the
distinction between a time advance request and a next message request, the four combinations of
regulating and constrained: every one of them exists so that participants can sit at different
logical times at once. The barrier forbids that, so they collapse into advancing everyone by
`delta`. Nothing is left to declare or negotiate, and everything arrives in receive order.

Lookahead is the exception, in that it survives but nobody declares it.
[`processNoFlush(delta)`](execution_model.md#real-time-execution-vs-stepped-execution) cycles the
components without publishing what they produced, and `flushOutputs()` publishes it, so a value
written in step T cannot be read before T+1. Lookahead is `delta`, and the double buffer is what
enforces it. A federate that understates its lookahead corrupts the federation; a Sen component has
no way to understate it, because `setNext` cannot make a value visible in the same step.

Two limits are worth knowing. Within one process stepping is deterministic, and across processes it
is not yet. And a component that cannot be stepped keeps following the real clock, `ether` among
them, because the network does not step.

Simulation time itself belongs to your model. Carry it as a property and Sen will move it around
without interpreting it. Every object also has a `lastCommitTime`, which follows virtual time when
the kernel is stepped.

In your FOM, `order` is read and dropped, while `transportation` is honored and maps as the table
above shows. In the modules shipped with the examples that costs nothing: 15 declarations ask for
`TimeStamp` order, all of them in `RPR-Minefield_v2.0.xml`, and all 15 also declare `HLAreliable`,
so they arrive reliably and in order anyway. That is true of these modules and is not a general
rule, because the two elements are independent: a FOM can ask for `HLAbestEffort` with `TimeStamp`,
wanting ordering without reliability, and Sen has no way to express that. Sen also reads none of the
`<tags>` section, which is where RPR keeps the DIS timestamp.

## Data type mappings

- HLA Enumerations are mapped to Sen enumerations. The declared `representation` picks the
  underlying integer type, and a representation Sen does not know is an error at generation time.
  Each enumerator keeps the value the FOM gives it, because in HLA the value is normative and the
  name is documentation. Two enumerators whose names collide after conversion to camel case get a
  numeric suffix. A `<value>` holding a list or a range is not supported: only the leading number is
  read, and the rest is ignored without a message. `HLAother` alternatives are not handled.
- HLA Records are mapped to Sen structs.
- HLA Variant Records are mapped to Sen variants.
- HLA Arrays of `HLAASCIIchar` or `HLAunicodeChar` with a `Dynamic` cardinality are mapped to Sen
  strings.[^bounded-text] With any other cardinality they follow the array rules below.
- HLA Arrays with a fixed cardinality are mapped to Sen bounded sequences with that maximum. Sen
  does not generate a fixed-size array here, because on an interface accepting fewer elements is
  more useful than forcing the exact count.
- HLA Arrays with a range are mapped to Sen bounded sequences, taking the upper bound as the
  maximum, as long as that bound is below 1048576.
- HLA Arrays with a `Dynamic` cardinality, and arrays whose range reaches that limit, are mapped to
  Sen unbounded sequences. RPR uses `[1..2147483647]` in several places, and those are unbounded.
- HLA Simple Types with units are mapped to Sen quantities.
- Type names in HLA are transformed to UpperCamelCase.
- Enumerator names in HLA are transformed to lowerCamelCase.

A FOM may use the same type for more than one alternative, telling them apart with a discriminant
enumeration. RPR does this in `SpatialVariantStruct`, where one structure serves both world and
body axis coordinates.

In C++ a variant becomes a `std::variant` holding the alternatives in the FOM's order, so an
alternative is picked by index, and where a type repeats that is the only way, since
`std::get<Type>` cannot say which of the two you meant. Sen generates a constant for each
alternative, named after its enumerator in the FOM, so you can write the index by name. The
discriminant enumeration itself carries the FOM's own values and is not the index.

The [Sen Query Language](sql.md) does not work this way. A query names the alternative by its type,
as in `spatial.SpatialFPStruct.worldLocation.x`, and a type name is the only thing a query path can
say. So where a FOM uses one type for two alternatives, a query has no way to name one of them.
`SpatialFPStruct` is both the world-axis and the body-axis alternative of `SpatialVariantStruct`,
and a query mentioning it cannot express which one you mean.

[^octetpair]: The OMT treats an octet pair as two opaque bytes with no arithmetic meaning. Mapping
    it to `u16` gives it one, so the rules in
    [Run-time compatibility](compatibility_conversions.md#basic-types), which widen and clamp, will
    apply arithmetic semantics to something the standard says has none. Treat it as storage rather
    than as a number.

[^bounded-text]: In C++ a Sen `string` is rendered as `std::string`, while a bounded sequence
    becomes `sen::StaticVector<T, size>`, because the standard library has no bounded-capacity
    container of that kind. See [Sequences and arrays](stl.md#sequences-and-arrays). Support for
    fixed and bounded `std::string`-like containers is planned, and will make this more convenient
    for C++ users.

HLA data representations are mapped as follows:

| HLA Representation                 | Sen   |
| ---------------------------------- | ----- |
| `HLAfloat32BE`,`HLAfloat32LE`      | `f32` |
| `HLAfloat64BE`, `HLAfloat64LE`     | `f64` |
| `HLAinteger16BE`, `HLAinteger16LE` | `i16` |
| `HLAinteger32BE`, `HLAinteger32LE` | `i32` |
| `HLAinteger64BE`, `HLAinteger64LE` | `i64` |
| `HLAoctet`                         | `u8`  |
| `HLAoctetPairBE`, `HLAoctetPairLE` | `u16`[^octetpair] |

A FOM may also declare its own representations, as RPR does with
`RPRunsignedInteger32BE` and friends. You do not need to register these with Sen: it reads every
`basicData` entry in your FOM and picks the Sen type from the `encoding` text, which must begin with
one of the following.

| `encoding` starts with    | Sen   |
| ------------------------- | ----- |
| `8-bit unsigned integer`  | `u8`  |
| `16-bit unsigned integer` | `u16` |
| `32-bit unsigned integer` | `u32` |
| `64-bit unsigned integer` | `u64` |
| `16-bit signed integer`   | `i16` |
| `32-bit signed integer`   | `i32` |
| `64-bit signed integer`   | `i64` |

Anything else stops the generation with an "unknown encoding" error naming the representation. Note
that there is no 8-bit signed entry, because Sen has no `i8`.

The types the standard defines are mapped by name. Sen does not read their declarations, which is
why it can skip the MIM:

| HLA Type                                      | Sen      |
| --------------------------------------------- | -------- |
| `HLAASCIIchar`, `HLAbyte`                     | `u8`     |
| `HLAunicodeChar`                              | `u16`    |
| `HLAcount`, `HLAseconds`,`HLAmsec`,`HLAindex` | `i32`    |
| `HLAinteger64Time`                            | `i64`    |
| `HLAfloat64Time`                              | `f64`    |
| `HLAboolean`                                  | `bool`   |
| `HLAASCIIstring`, `HLAunicodeString`          | `string` |

Some FOMs define their own version of a type the standard already has. These are the predefined
mappings for those:

| FOM | Type         | Sen    |
| --- | ------------ | ------ |
| RPR | `RPRboolean` | `bool` |

HLA does not define a standard for naming units, so Sen matches the unit text in your FOM against
the list below. The match is exact, spaces and brackets included, and a unit that is not on the list
simply produces a plain number instead of a quantity. The list is what published models have been
seen to use, not a standard anybody publishes.

| HLA Unit                              | Sen Unit     |
| ------------------------------------- | ------------ |
| "meter per second squared (m/(s^2))"  | `m_per_s_sq` |
| "degree (deg)"                        | `deg`        |
| "radian (rad)"                        | `rad`        |
| "radian per second (rad/s)"           | `rad_per_s`  |
| "meter (m)"                           | `m`          |
| "hertz (Hz)", "interrogations/second" | `hz`         |
| "kilogram (kg)"                       | `kg`         |
| "revolutions per minute (RPM)", "RPM" | `rpm`        |
| "degree Celsius (C)"                  | `degC`       |
| "microsecond"                         | `us`         |
| "millisecond (ms)"                    | `ms`         |
| "second (s)"                          | `s`          |
| "meter per second (m/s)"              | `m_per_s`    |
| "micron"                              | `um`         |
| "decimeter per second (dm/s)"         | `dm_per_s`   |

## Class mapping

The HLA class hierarchy inherits from `hla.ObjectRoot`, the class that `HLAobjectRoot` is mapped to.
In generated C++ it appears as `hla::ObjectRootBase`, deriving from `sen::NativeObject` and so from
`Object`.

That base class carries one property, `rtiId`. It exists because HLA object instances carry an
instance ID assigned by an RTI, so a bridge to an HLA federation has somewhere to put it. **Sen
never populates it**: nothing in Sen talks to an RTI, so the property stays empty unless your own
code writes to it. It is completely independent of the identification Sen provides in the `Object`
class.

HLA class attributes are mapped to Sen class properties, with the attribute name converted to
lowerCamelCase. Five names are already taken by the `Object` class, so an attribute that would
collide with one of them gets `NonSen` appended: `name`, `id`, `localName`, `lastCommitTime` and
`propertyUntyped` become `nameNonSen`, `idNonSen` and so on. If a generated property has a name you
did not expect, this is why.

Property mode is mapped as follows:

| `updateType`  | `sharing`                         | Sen         |
| ------------- | --------------------------------- | ----------- |
| `Static`      | `PublishSubscribe` or `Publish`   | `staticRW`  |
| `Static`      | `Subscribe` or `Neither`          | `staticRO`  |
| anything else | anything else                     | `dynamicRO` |

So an attribute that changes over time arrives read-only by default: other objects can watch it, but
only its owner sets it. That is usually what a FOM means, since an attribute is normally published
by whoever owns the entity.

When you do want a dynamic property writable from outside, say so in
[the mappings file](#the-mappings-file) instead of changing the FOM.

## The mappings file

The FOM describes a data model; it does not describe how you want to use it. The mappings file is
where that goes. You pass it to the code generator with `HLA_MAPPINGS_FILE`, and it decorates the
imported model without touching the FOM itself. The standard's XML stays exactly as published, and
your project's decisions live alongside it.

It can make a property writable, and turn an interaction into a method or an event.

```xml
<senMapping>
    <class name="BaseEntity.PhysicalEntity.Munition">
        <property name="launcherFlashPresent" writable="true"/>
        <event hlaInteraction="MunitionDetonation" pack="true"/>
    </class>
</senMapping>
```

The `class` name is the class's **path in the FOM hierarchy**, not its Sen name:
`BaseEntity.PhysicalEntity.Munition`, not `rpr.Munition`. Property names, by contrast, are the
**Sen** names: lowerCamelCase, as generated.

### Properties

`writable="true"` gives the property a public setter. The FOM decides everything else about it.

### Interactions

An interaction is sent once and delivered to every federate subscribed to its class, so it carries
several distinct communication patterns:

- True broadcasts.
- Requests made to one or multiple "back-ends" (where the receiver may or may not be defined, and a
  request ID is usually provided).
- Responses to requests (where the request id is sent back to all potential senders for them to
  manually correlate).
- Messages between participants.

In HLA, interactions are mapped as a hierarchy which is parallel to the class hierarchy.

In Sen:

- Classes for objects that have properties, methods and can emit events.
- Method calls, made to some specific instance, with arguments, and which might return a value.
- Events, which interested listeners can react to.

To map HLA interactions to the Sen world you need to provide an XML file defining how you want to
map those interactions to Sen methods or events.

**Methods that do not return.**

If you want to map an interaction as a method that doesn't return anything, you need to define the
class that will hold the method and the interaction from where the method name and the arguments
will be obtained. For example:

```xml
<class name="WeaponServer">
    <method hlaInteraction="CreateMunition"/>
    <method hlaInteraction="DeleteMunition"/>
</class>
```

If you don't want your method to have all the parameters that are defined in the HLA interaction,
you can ignore them as follows:

```xml
<class name="WeatherServer">
    <method hlaInteraction="METOC_Interaction.Request.RequestWeatherCondition">
        <ignore parameter="EventId"/>
        <ignore parameter="ServiceId"/>
    </method>
</class>
```

**Methods that return.**

You can use the definition of an interaction to define the return value of a method. For example:

```xml
<class name="WeatherServer">
    <method hlaInteraction="METOC_Interaction.Request.RequestLandSurfaceCondition">
        <ignore parameter="EventId"/>
        <ignore parameter="ServiceId"/>

        <return hlaInteraction="METOC_Interaction.Response.WeatherCondition.LandSurfaceCondition">
            <ignore parameter="EventId"/>
            <ignore parameter="Status"/>
        </return>
    </method>
</class>
```

In this case, the return type will be a Sen structure holding all the parameters of the interaction.

**Returning a plain type instead of a structure.**

If the answer is a single value and not a set of parameters, name a FOM datatype directly with
`dataType` and leave `hlaInteraction` off:

```xml
<class name="TerrainServer">
    <method hlaInteraction="RequestGroundHeight">
        <return dataType="HeightFloat32"/>
    </method>
</class>
```

The two are alternatives: `hlaInteraction` builds a structure from a response interaction,
`dataType` uses a datatype the FOM already defines.

**Methods that return errors.**

If you don't want to return error codes, you can throw a `std::exception`, and Sen will propagate it
to the caller.

**Local-only methods.**

Set the `local` attribute to `true` on the method node, for example:

```xml
<class name="MyClass">
    <method hlaInteraction="MyInteraction" local="true">
        <return hlaInteraction="MyResultInteraction"/>
    </method>
</class>
```

**Events.**

Events can be injected into classes in a similar way:

```xml
<class name="BaseEntity.PhysicalEntity">
    <event hlaInteraction="Collision">
        <ignore parameter="EventIdentifier"/>
        <ignore parameter="IssuingObjectIdentifier"/>
    </event>

    <event hlaInteraction="CollisionElastic">
        <ignore parameter="EventIdentifier"/>
        <ignore parameter="IssuingObjectIdentifier"/>
    </event>

    <event hlaInteraction="WeaponFire">
        <ignore parameter="EventIdentifier"/>
    </event>
</class>
```

**Packing Arguments.**

By default, each parameter in an interaction is mapped to an argument. If you want to pack them into
a single structure, set the "pack" attribute to "true" (this works for methods and events). For
example:

```xml
<class name="BaseEntity.PhysicalEntity.Munition">
    <event hlaInteraction="MunitionDetonation" pack="true">
        <ignore parameter="EventIdentifier"/>
        <ignore parameter="MunitionObjectIdentifier"/>
    </event>
</class>
```

**Optionals.**

There's no formal support for truly optional values in HLA. However, the Sen code generator treats
an attribute or an interaction parameter as optional when its documentation (semantics) starts with
one of the following:

- "Optional."
- "Optional ("
- "Optional:"
- "Optional,"

## Customizing the generated code

The same `codegen_settings.json` that customizes STL-declared classes also works on classes imported
from a FOM. `checkedProperties` asks your implementation for approval
before an external write is applied, and `deferredMethods`, which lets an implementation answer when
it chooses. [Customizing the generated code](stl.md#customizing-the-generated-code) explains both.

The one thing that differs for FOM classes is how you name the class, and it is not the name the
mappings file uses:

```json
{
  "classes": {
    "rpr.Munition": {
      "checkedProperties": ["launcherFlashPresent"]
    }
  }
}
```

The key is the **Sen qualified name**: the package, then the class. The package is the FOM
directory name in lower case, so `HLA_FOM_DIRS rpr netn` gives packages `rpr` and `netn`, and the
class is its generated name without the hierarchy path. The
[generated FOM reference](../snippets/fom.md) lists every name in this form, which is the quickest
way to find the one you want.

So the same class is written two ways, depending on which file you are in:

| File | Name to use | Example |
|---|---|---|
| The mappings XML | Path in the FOM hierarchy | `BaseEntity.PhysicalEntity.Munition` |
| `codegen_settings.json` | Sen qualified name | `rpr.Munition` |

### Why these are not in the mappings file

The split follows one rule: **the mappings file decides the interface, the settings file decides the
implementation.** Whether an interaction becomes a method or an event, what its arguments are, and
whether a property has a public setter all change what other components see, so they belong to the
model. Whether your implementation validates a write before accepting it, or answers a call later
instead of immediately, changes nothing about the interface, and another component cannot tell, so
it belongs to your project rather than to the shared model.

That matters most when the FOM is shared. Two organizations agreeing on the same model can make
opposite implementation choices without either of them having to change anything the other reads.

!!! note "If you have an older mappings file"

    `checked` on a property and `deferred` on a `<return>` used to be written in the mappings file,
    and still work so that existing projects keep building. They print a warning asking you to move
    them here, and that is the direction to go: they are implementation choices, and they should not
    sit in the file that defines the interface.
