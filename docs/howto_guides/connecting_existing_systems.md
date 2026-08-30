# Connecting Sen to systems you already have

The Sen core does not ship with connectors to other systems. It gives you a set of
ways to build the bridge instead, and they differ from each other in kind, not just in
detail. Which one fits is usually decided by facts you already know about the system
you are connecting.

## What decides the shape

You can normally answer all of these before writing anything, and the first one
alone picks the section you want below.

**Where does the bridge run?** Inside a Sen process, in a process of its own, or
inside the other system.

**Does the other side have introspection?** If both sides can describe their own
types at runtime, a bridge can be written once and work for types it has never seen.
Otherwise it has to be written against a specific data model, and the same is true
when the mapping has to be fast.

**Who drives the clock?** If the other system has its own main loop, one of the two
loops has to give way, and that decision shapes everything else.

**What language must the bridge be written in?** A component is C++. Another language
reaches Sen either through a binding that runs inside a kernel, as Sen's Python
support does, or over a protocol from outside it.

## A component that translates

The most direct approach is a component with objects on a bus, published and
configured like any other, whose implementation talks to the outside. That might be an SDK, a
serial link, or a wire protocol. Everything already on the bus sees ordinary Sen objects and has no
idea a bridge exists.

The approaches differ sharply, and which one you take follows from the
introspection question above.

**Driven by introspection.** Sen's type system is fully introspectable at runtime. If
the other side is too, the bridge can map between the two type systems generically,
without naming a single one of your types.

The `influx` component ships and works this way. You give it query selections such as
`SELECT * FROM school.primary`, and it records whatever matches into InfluxDB. There
is no code in it for your types, so adding a class to your model is enough to see it
in Grafana.

Qt shows how far this can go, because it has a meta-object system of its own. A bridge
written once against both could surface Sen objects as Qt objects, so a QML interface
would treat them as native and would need no code per type.

A bridge like this keeps working as your model grows: new types cross it without anyone
touching the bridge. But the mapping has to happen at runtime, so the approach only
applies when the far side really does describe itself.

**Written against a fixed data model.** When the far side has an agreed model, or the
throughput matters, the mapping is written explicitly. Airbus's DIS gateway works
this way, mapping Sen objects derived from HLA FOMs onto DIS PDUs in both directions.

Gateways of this kind run inside Airbus for DIS, CIGI, TacView and HLA, and for internal
simulation and testing frameworks, Qt, and other proprietary ecosystems and products.
None of them is released or available to download.

How much of that writing is mechanical depends on the far side. Where both ends are
described by a model, the translator can largely be generated. An HLA FOM already
tells the generator the shape of the Sen classes it will produce, and the API on the
RTI side has a known shape too, so the code that sits between them follows from the
same source you started with.

A message-based protocol gives you less to work from. DIS and CIGI define fixed message
layouts instead of a model to read, so that mapping is written once by hand and then
maintained.

Written this way the bridge is fast and exact, and the awkward corners of a standard have
somewhere obvious to live. You maintain it by hand, though: a change on either side is a
change to the bridge.

## An application of its own

Sometimes the bridge should not be in the same process at all. The MCP gateway is the
shipped example. Its reasons make a useful checklist:

- **Isolation.** It acts on requests from an LLM. Keeping it out of the kernel's
  process is a security boundary, not a matter of taste.
- **Technology.** A TypeScript client already existed. Forcing that into a C++
  component would have meant rewriting something that worked.
- **Traffic.** Its exchanges are not high-frequency, so the cost of crossing a
  process boundary does not matter.

Its shape combines two things from elsewhere on this page. It opens a WebSocket to
the `jsonrpc` component of each kernel it is pointed at, discovers whatever those
kernels publish, and re-publishes that introspection over MCP without adding anything
of its own. The generic mapping described above does not require being in-process, so
it works across a protocol that carries type information. And a bridge outside the
kernel is not tied to one of them, which is how the gateway presents several kernels
through a single interface.

A boundary like that contains failures as well as attackers, and it leaves you free to
pick the language and ecosystem. It also puts a process boundary in the data path, which
is why the third point above matters. Check your traffic before assuming this shape fits.

## A Sen kernel inside another system

When the other system has a runtime of its own, whether a main loop, an event loop or
a scheduler, the bridge can live inside it: instantiate a Sen kernel in that process
and translate between the two worlds in place. `KernelBlockMode::doNotBlock` exists
precisely so that starting a kernel does not take over the calling thread.

This is the option with the real design work in it, because you now have two loops
that both want to run. They can be reconciled in either of these ways:

**Let the host drive.** In a component you write yourself, `run()` is yours, and the
runtime API exposes `drainInputs()`, `update()` and `commit()` as three separate
calls, not only as a loop. Your host's iteration can call them in order, and Sen
advances in step with it. The two systems then move together, and nothing needs guarding because
nothing runs concurrently.

The coupling is tight, though. Sen advances exactly as often as the host does, so a slow
host slows everything on the bus.

**Let both run, and guard the handover.** Sen keeps its own cycle and the translation
happens at a defined point in it. `preDrain()` and `preCommit()` are the hooks. The
first runs before Sen reads its inputs, so that is where incoming data can be handed
in. The second runs before the commit, so that is where outgoing data can be picked up
once your objects have updated. Both are enabled by returning `true` from
`needsPreDrainOrPreCommit()`. Override them alone and they are never called, with no
error. You choose how to protect the shared data: a mutex is one option, a
queue is another, and which is right depends on the host's threading model and on how
much latency you can accept.

Now each side runs at its own rate, and the concurrency problem that comes with that is
yours to get right rather than Sen's.

A CIGI gateway (the common image generator interface protocol) is one working example
of this shape. Anything with its own loop that you need to reach into fits the same
pattern.

## What you get once a gateway exists

Whichever shape you chose above, the result is a component, and that is what makes the
rest of this possible.

A gateway is instantiated from configuration like anything else, and everything already
on the bus keeps seeing ordinary Sen objects. Adding one to a running system costs its
users no code at all.

It also costs no network traffic unless you ask for it. Because a gateway is a
component, where it runs is a deployment choice. Several of them can sit in the same
kernel as the model they serve. A simulation built on an HLA data model could load a
CIGI gateway, a DIS gateway, the MCP gateway and `influx` beside it in one process, and
Sen would add nothing on the wire, since components in the same kernel exchange objects
through queued function calls and `ether` only enters when you load it. Put each gateway
in its own process instead when you want the isolation, or mix the two. The code does
not change either way, only the configuration does.

A gateway need not be a single choke point either. Each instance declares its own
interest, so several of them can take a slice of the object space and translate in
parallel, partitioned by bus, by type, or by any condition the query language can
express. The objects being translated do not know, and nothing in the gateway has to
be written with that in mind.

The composition goes further than that. Two gateways written by different people, for
different protocols, both speak Sen objects on a bus. So if you load an HLA gateway and
a DIS gateway into one kernel and run nothing else at all, no model and no objects of
your own, what you have is a bidirectional DIS to HLA translator that neither author
set out to build. Every gateway that exists pairs with every gateway that already
existed, and nobody has to agree on anything beyond the object model they both already
speak.

There is a matching point about what you do not pay for. A system built natively on Sen
carries none of these protocols in its own model. You attach a gateway when you have
something to talk to, and until then the protocol's data model, its semantics and its
overhead are simply absent. Building directly on one of these standards works the other
way around, because its object model is your object model from the first line, whether
or not you ever federate.

## How much the kernel does for you

Whichever shape you picked, how much you write depends on how you start the kernel.
Sen is a library as much as it is an executable, and how you start it decides how
much it does for you.

**`sen run`, with a configuration file.** For a system being built around Sen this is
usually the right answer. You declare which packages to load and which objects to
instantiate, and the kernel does the rest. You do not write any of the following. It
loads your package libraries and registers their types. It checks your configuration
against those types. It instantiates the objects, connects them to their buses and
publishes them. It runs the cycle at the frequency you configured. On shutdown it
unwinds all of that in the right order.

**A kernel you instantiate yourself.** The same kernel, embedded in your own program
through `sen/kernel/kernel.h`. You can still hand it a configuration and get
everything above; what you gain is control over when it starts, and the option of
`KernelBlockMode::doNotBlock` so it does not take over your thread.

**A component you implement yourself.** A `Component` subclass, where the lifecycle
hooks (`preload`, `load`, `init`, `run`, `unload`) are yours. Now nothing on that list
is done for you. Loading libraries, registering types, creating objects, connecting
buses, publishing them, **and driving the execution loop** all become your job. `run()` is handed a
`RunApi` and it is up to you what happens inside it, including whether you call
`execLoop()` or advance drain, update and commit yourself.

That last option is what makes [a Sen kernel inside another
system](#a-sen-kernel-inside-another-system) possible, at a price. You take on that
whole list in order to control the one part of it you need. If all you need is
translation, the first option is far less work, because a package with objects in it
is all it takes.

## Writing your own translator

You do not have to wait for someone else to write the translator you need. The
meta-model is not private. `sen::core` ships the STL parser and the FOM parser in its
`lang` module, and `sen::gen` ships the generators built on top of them, including the
ones for C++, Python, TypeScript, JSON, PlantUML and MkDocs. So you can read a
resolved type set and emit whatever your project needs from it, and some users already
do. Generating a translator shaped for your own framework, idiom or paradigm is a
matter of writing the emitter, since the model it reads is the same one Sen uses.

The plan is to make that a first-class part of the `sen generate` application, so
adding a translator does not mean building a tool around the libraries first.

## Using Sen from another language

A different question from the ones above. Here the goal is not
to bridge two systems but to work with Sen objects from somewhere other than C++.

One thing this is not is Sen's pluggable transport. That interface is how the kernel
reaches a network, and replacing it changes what carries Sen traffic between kernels. It
is not a way to make a participant out of a program written in another language, because
the wire format and the discovery it would have to speak are internal. Use one of the
three shapes below instead.

They run in increasing distance from the kernel:

| | How it works | Consequences |
|---|---|---|
| **Native** | Hand-written bindings for one language and one domain | Exactly what you want; you maintain it, and it does not follow your model automatically |
| **Introspection-based, in-process** | Runs inside a kernel and uses Sen's runtime type information, so it needs no per-type code. Sen's Python support works this way | Wide coverage for little code; runs inside a component, so it is bound by that component's cycle |
| **External, over a protocol** | Any language with a client for one of Sen's protocol components (`jsonrpc`, `rest`, or another) | No C++ and no Sen build. It can still be generic over your model, since the protocol carries type information, and it can hold connections to several kernels at once. It is not a participant on the bus |

On the last row: `jsonrpc` is *a* protocol Sen speaks, not *the* way out. Nothing in
the design privileges it. A protocol component is just a component, so if gRPC or
XML-RPC suits your ecosystem better, adding one is a reasonable thing to do.

## If this looks like a lot of options

It is, and that is deliberate. The systems people connect to Sen are genuinely
different from each other, and a single prescribed route would fit some of them badly.
An in-process translator, a separate application and a kernel embedded in someone
else's runtime are not three ways of doing the same thing. They exist because all
three cases turn up.

None of it has to be worked out alone. If you are weighing two of these shapes against
each other, or you cannot tell which of the four questions applies to your case, open
an [issue](https://github.com/airbus/sen/issues) and ask. The people who built these
mechanisms would rather talk through a choice beforehand than watch someone commit to
the wrong shape and discover it later.

## See also

- [Design considerations](considerations.md), on where the process boundary goes and
  what is cheap to change later
- [Writing a component](components.md), including the case where you already have an
  external event loop
- [The execution model](../users_guide/execution_model.md), for the cycle and how it is
  advanced
- [Using HLA FOMs](../users_guide/hla.md), for what FOM support means and does not mean
