# Design considerations

With Sen, you will be able to expand your options when designing your system. This page compiles
some notes, considerations and tips you can use when doing so.

## Bridging domains

![Screenshot](../assets/images/gateway_light.svg#only-light){: style="width:350px; float: right;"}
![Screenshot](../assets/images/gateway_dark.svg#only-dark){: style="width:350px; float: right;"}

It is possible to bridge Sen to other IPC systems and communication solutions, just as Sen can be
bridged to object systems. By doing so, existing Sen applications do not need to know if they are
connected with some other system. All they see are objects with methods and events. Note that these
applications do not even need to be re-compiled.

[Connecting existing systems](connecting_existing_systems.md) covers the shapes such a
bridge can take.

## Where the process boundary goes

A Sen object is published to a bus, and whoever wants it says so with a query. Neither side names
the other, and neither side states where the other runs. Putting two components in one process, in
two processes on one machine, or on two machines is a matter of configuration and of loading the
transport that carries the bus between them.

That has a practical consequence for design: you do not have to decide your deployment topology
early, and you are not stuck with the decision. The same packages, unchanged and un-recompiled, run
as a single process while you are developing and as several when you need isolation, independent
restart, or separate hardware. So the usual argument about how finely to divide a system does not
have to be settled up front here. Start with whatever is easiest to run and debug, and split when
something concrete pushes you to: a component that must survive a restart of the rest, a workload
that needs its own machine, a piece owned by a different team on a different release schedule.

What does need thought early is the boundary itself, because that is the part configuration cannot
undo. Two components that share a bus and a set of classes are coupled through those classes
whatever process they sit in, and changing a class that others already consume is the change that
propagates. [Compatibility conversions](../users_guide/compatibility_conversions.md) absorb some of
that, but they are a tolerance, not a substitute for a boundary you intend.

## Architectural guidelines

Software design requires combining multiple perspectives and evaluating their applicability in
different contexts. In this section, we define some general ideas to guide the design decisions.

**Aim for simplicity and clarity of intent**

Simplicity and clarity are applicable to any software development, but are especially relevant in
environments with real-time constraints. In this context, we weigh every design decision that adds
cognitive or implementation-related complexity in terms of its value. Enforcement of this guideline
is difficult but possible through attentive review and firm adherence to coding standards. Failing
to follow it results in technical debt that has to be refactored away later.

**No unnecessary abstraction layers**

The efficient usage of memory and compute resources is essential, and the "abstraction penalty"
should be avoided. Abstractions shall only exist when the motivation is distinctly justified. Even
then, techniques that reduce or eliminate abstraction layers such as generic programming (where the
compiler takes care of it) or explicit code generation (where the code generator deals with removing
indirections) mitigate the penalties.

**Solve problems only once and make solutions easily available**

Different solutions to the same problem are costly and prone to error. Their usage, scope, and
context usually define how thoroughly they get tested and validated.

**Automate as much as possible**

Repetitive, manually written code is tedious and prone to error. Therefore, creating tools to write
it is undoubtedly preferred. Automation is not limited to C/C++ code, but naturally includes build
and configuration files.

**Prefer declarative definitions**

Declarative definitions express facts and conscious intent. Intent can be exploited by tools to
generate the most appropriate code, and by humans to better recognize the purpose. These definitions
apply to code, models, and configuration files.

**Enable composition**

Reusable building blocks with a clearly defined interface, without sacrificing performance or
safety. More importantly, they let us scale the system, the codebase, and the development effort. In
this environment, composition must be possible at design time and at compile time.

**Aim for testability at every level**

This results in:

- Elements that are self-standing for instantiation, stimulation, and individual testing.
- Dependency injection without polluting the interface.
- Reliance on scripting tools for high-level integration and orchestration tests.
- Instrumented code that creates continuous performance and endurance tests, and regression
  detections.

Testing shall not be a second thought, but a primary need for both Sen and functional components.
Testing must apply existing, well-known patterns for exercising the code. To ensure that your code
is easy to test:

- Isolate the function from the context, make the context mockable, apply dependency injection, and
  systematically inject errors.
- Rely on unit tests to detect problems as quickly as possible.
- Declare pre- and post-conditions and automate them for validation at every possible level.
- Rely on integration tests to verify contracts between components.
- Run the tests in the target HW environment as the critical last step. If it does not work in the
  target, it does not work, but requiring the target HW for testing basic logic is not sufficient.

**Tooling follows method, not the other way around**

If a tool is not in line with the methodology of choice, it shall be adapted or replaced. This
guideline is crucial in any development and extends to the build system, toolchains, CI, and so
forth.

**Prioritize validation by design-time > compile-time > run-time**

Prevent bugs as early as possible. Design-time checkers are the first and most powerful weapon
against bugs. Design-time checkers prevent bugs before any code is written, and the tooling has
access to system-level and domain-specific considerations that the compiler lacks. That said, the
compiler is a powerful ally, and it has a better understanding of the final code than the tool that
generated the code. Therefore, both the generated and manually written code should use compile-time
assertions to prevent wrong assumptions (which is especially relevant when switching compilers,
architecture, or operating systems).

**Surface design decisions**

Design is a fundamental part of any serious product development from a qualification perspective,
and it makes understanding the system easier. To have a software design, we first need a language in
which we will express the concepts that our architecture is built with. This means having an
architectural framework that is backed by a SW infrastructure that ensures that we are not "building
a castle in the air." Our design approach must be solid, up to date with the code, transparent to
the tooling, traceable, easy to change, easy to validate, and resilient to human error.

Sen (starting with this document) shall enforce design decisions to be visible, documented, and
structured.
