# Glossary

Sen reuses words you already own. Some it narrows, some it redefines, and a few mean more
than one thing inside Sen itself. This page gives each one a single meaning and points at
the page that explains it properly.

Acronyms are not listed here. Every acronym in the documentation carries its expansion as a
tooltip. Hover over HLA or QoS anywhere on the site, including in this sentence, and you
never have to leave the page you are reading to resolve one.

## Words Sen redefines

These are ordinary engineering words with a specific Sen meaning. Where the everyday meaning
is close enough to mislead, the entry says so.

bus { #bus }

:   A named channel inside a [session](#session) where objects are published and found. A bus
    is addressed as `<session>.<bus>`, for example `school.primary`, and both halves are
    required. Buses decide what reaches your machine; [interests](#interest) decide what you
    are told about once it has. See [Main concepts](main_concepts.md).

component { #component }

:   The unit of execution: an interface, the logic behind it, and a thread to run it in.
    Components are relocatable. You can move one to another thread, process or machine
    without changing its code.

    *Not the same as a shipped component.* The word also names the twelve things Sen ships
    that you load rather than write, such as `shell` and `ether`. Those are components in
    exactly this sense; they are simply ones you did not have to build. See
    [Components](../components/index.md).

interest { #interest }

:   A standing query that says what a component wants to be told about, written in the
    [Sen Query Language](sql.md). An interest filters what you receive; it does not change
    what is on the network. See [The Sen Query Language](sql.md).

library { #library }

:   One of the C++ libraries Sen is built from: `sen::core`, `sen::db`, `sen::kernel`,
    `sen::util` and `sen::gen`. Not to be confused with a [package](#package), which is
    what *you* write and what the kernel loads at run time. See
    [The Sen libraries](index.md).

object { #object }

:   An instance of a class you declared in [STL](#stl), owned by the component that created
    it and published on a [bus](#bus). Objects carry [properties](#property), emit
    [events](#event) and expose [methods](#method). They are network-level objects, not C++
    objects: the C++ class the generator writes for you is how you work with one, not what
    one is.

package { #package }

:   A unit of interface and implementation that the kernel loads at run time. It is what
    `sen package init` creates. A package holds `.stl` files describing types and the C++
    that implements them.

    *Three neighbors to keep apart.* A **Conan package** is how Sen and its dependencies are
    distributed to a build. An **npm package** is a JavaScript dependency of the browser
    tooling. A Sen package is neither. See
    [Creating your first package](../getting_started/first_package.md).

property { #property }

:   A named value an [object](#object) carries, declared in [STL](#stl). Two axes decide how
    it behaves. Whether it can change at all: *static* is set once at construction, *dynamic*
    may change later. And who may change it: *read-only* means only the implementation
    does, *read-write* means other components may call the setter. Properties are dynamic and
    read-only unless you say otherwise. See [Main concepts](main_concepts.md).

event { #event }

:   A notification an [object](#object) emits, which components holding an
    [interest](#interest) in it are told about. An event is one-way and carries no reply.
    See [Main concepts](main_concepts.md).

method { #method }

:   An operation another component can call on an [object](#object). Calls never block the
    caller: the answer arrives later, and the implementation may defer it deliberately rather
    than answer in the same cycle. See [Main concepts](main_concepts.md).

session { #session }

:   The outermost namespace. There is nothing to create, join or leave: a session exists
    because someone used the name. The name feeds the multicast addressing, and participants
    ignore remote kernels whose session differs, so two systems on one network with different
    session names do not see each other. See [Main concepts](main_concepts.md).

## Words that collide with something you already know

STL { #stl }

:   **Sen Type Language**, the language you declare your interfaces in. Sen's `.stl` files
    are STL files in this sense and no other.

    *Other things are called STL.* In C++, it usually means the
    [Standard Template Library](https://en.wikipedia.org/wiki/Standard_Template_Library). In
    3D printing and CAD, `.stl` is a
    [stereolithography mesh format](https://en.wikipedia.org/wiki/STL_(file_format)). Neither
    has anything to do with Sen, and both are common enough that a search for "STL file" will
    find them first. See [The Sen Type Language](stl.md).

SQL { #sql }

:   **Sen Query Language**, the language [interests](#interest) are written in. It borrows
    `SELECT ... FROM ... WHERE` from [database SQL](https://en.wikipedia.org/wiki/SQL) because
    the shape is familiar, but it queries live objects on a bus rather than rows in a table,
    and it is not a database language. See [The Sen Query Language](sql.md).

kernel { #kernel }

:   The interconnection mechanism a Sen system runs in. It loads your components, runs their
    cycles and carries objects between them, and there is one per process.

    *This is not an operating-system kernel.* It is an ordinary library linked into your own
    process, with no privileged mode and nothing installed on the machine. See
    [Main concepts](main_concepts.md) and [Execution model](execution_model.md).

remote kernel { #remote-kernel }

:   The kernel in another process that this one has found, one per process. If you arrive
    from another distributed system, this is what you would call a *peer*. This documentation
    uses "remote kernel", and keeps *peer* only for the shape of the network: Sen's transport
    is peer-to-peer, with no central server or broker. See [Ether](../components/ether.md).

subscription { #subscription }

:   The live list of objects an [interest](#interest) currently matches, held as a
    `Subscription<T>`. It has to stay alive to keep working: if it goes out of scope the list is
    destroyed and the callbacks stop.

    *It is not a topic subscription.* Nothing is subscribed to a channel or a queue. The interest
    is the query, and the subscription is what that query matches right now. See
    [Working with objects](../howto_guides/objects.md).

container { #container }

:   In the type system, one of the type constructors that wrap a value type: `sequence<T>`,
    `sequence<T, n>`, `array<T, n>`, `optional<T>` and `quantity<T, unit>`.

    Elsewhere in this documentation "container" means a Docker or
    [OCI](https://opencontainers.org/) container. Those passages are about deployment and say
    so. See [The Sen Type Language](stl.md).

## Words with more than one meaning inside Sen

execution time { #execution-time }

:   Two unrelated things share this word.

    - The value `RunApi::getTime()` returns, which is the time a component is running at. It
      advances by whole periods and Sen does not interpret what it means. See
      [The time a component sees](execution_model.md#the-time-a-component-sees).
    - The **CPU time an update consumed**, which is what the `<component> execution time overrun`
      metric measures and what gets reported when a cycle runs long.

export { #export }

:   Three unrelated things share this word, and only context separates them.

    - `SEN_EXPORT_CLASS` **registers a class** with the kernel so it can be instantiated by
      name from configuration.
    - `sen generate cpp exports` writes a **symbol-visibility header**, the ordinary C++
      meaning of exporting from a shared library.
    - **Exporting interfaces** means installing your `.stl` files so another project can
      build against them.

    See [Generate exportable interfaces](../howto_guides/exportable_interfaces.md), which
    disambiguates the first and third at the point they are used.

interface { #interface }

:   Usually the set of properties, events and methods a class declares in [STL](#stl): what
    other components can see and do. It also appears in its ordinary networking sense, a
    network interface such as `eth0`, throughout [Ether](../components/ether.md) and the
    configuration pages. The two are never in the same sentence, but they are in the same
    documentation.

## Standards and other systems Sen mentions

These are not Sen's terms. They are named across the documentation, and this table says
what each one is, how Sen relates to it, and where the authority lives, so you do not
have to leave to find out.

| Term | What it is | Where Sen stands |
|---|---|---|
| [HLA](https://en.wikipedia.org/wiki/High_Level_Architecture) | The IEEE 1516 simulation interoperability architecture | Sen reads FOM XML at build time and emits types from it. **Sen is not an RTI and does not join federations.** See [Using HLA FOMs](hla.md) |
| FOM / SOM / OMT | The object models of an HLA federation, and the template they are written against | The FOM is an input to the code generator. OMT's own extension point for custom transportation types is not supported |
| [RTI](https://en.wikipedia.org/wiki/Run-time_infrastructure_(simulation)) | The broker an HLA federation runs through | No equivalent. Sen is broker-less; kernels find each other over `ether` |
| [SISO](https://www.siso.org/) | The body that publishes the standard simulation models | Its models are what Sen's FOM importer is aimed at |
| RPR, NETN | Shared FOMs: RPR (SISO standard), NETN (NATO product) | Either can serve as your interface definition. See [Using HLA FOMs](hla.md) |
| [DIS](https://en.wikipedia.org/wiki/Distributed_Interactive_Simulation) | A message-based simulation protocol with fixed PDUs | Reachable through a gateway you write. A DIS gateway is not shipped |
| [CIGI](https://en.wikipedia.org/wiki/Common_Image_Generator_Interface) | The common image generator interface protocol | A CIGI gateway exists as a worked example and is **not released**. See [Connecting existing systems](../howto_guides/connecting_existing_systems.md) |
| [DDS](https://en.wikipedia.org/wiki/Data_Distribution_Service) | A publish-subscribe middleware standard | A system you may be arriving from. See [The mental model](mental_model.md) |
| [ROS](https://www.ros.org/) | The Robot Operating System | As above |
| [SOME/IP](https://some-ip.com/) | Automotive service-oriented middleware over IP | As above |
| [MCP](https://modelcontextprotocol.io/) | The protocol a large language model uses to reach a tool | What the MCP gateway speaks. See [MCP gateway](../components/mcp_gateway.md) |
| [PTP](https://en.wikipedia.org/wiki/Precision_Time_Protocol) | Clock synchronization across a network | Named where clock accuracy matters |
| [IGMP](https://en.wikipedia.org/wiki/Internet_Group_Management_Protocol) | How hosts and switches manage multicast group membership | What has to work for `ether` discovery to work. See [Ether](../components/ether.md) |

## What this page deliberately does not contain

It does not list the shipped components, the STL types or the built-in units. All three are
generated from the source, so a copy here would be one more thing to keep in step and the
first to fall out of it. [Components](../components/index.md) and
[The Sen Type Language](stl.md) hold the real lists.
