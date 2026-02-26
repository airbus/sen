# Main Concepts

## Objects

In Sen, the communication is modeled around the idea of objects interacting with each other. In a
nutshell,

- Objects can contain data and code. The data is in the form of properties, and the code is in the
  form of methods.

- Methods are functions that can be called by other components. Methods can have arguments and may
  optionally return a value.

- Objects can emit events. Other components can react to those events. Events can have arguments.

- Objects are instances of classes. Like C++ and Java, Sen supports inheritance (including multiple
  inheritance).

![Screenshot](../assets/images/object_light.svg#only-light){: style="width:900px"}
![Screenshot](../assets/images/object_dark.svg#only-dark){: style="width:900px"}

You can discover objects published by other components, examine their properties, call their methods
and react to their events. You can also publish your own objects, so that other components can "see"
you, call you, and react to your changes.

Objects are identified by name. This means that object names will be unique. You are responsible for
naming your objects. Once named, objects also provide a (32-bit) numeric identifier that is more
"computer-friendly".

Methods can be optionally marked as `constant`. This means that they do not change the internal
state of the object.

Properties can be:

- **Static**: You have to set a value when constructing the object. Once set, this property will not
  change. Think of the word "static" simply as not being "dynamic". The equivalent in C++ or Java
  would be a constant member variable that must always have a value. Note that this value can be
  different for every instance.

- **Dynamic**: You *may* set a value when constructing the object, but the important trait here is
  that this is a property that may change at some point. Other components may react to those
  changes.

- **Read Only**: *You*, the one implementing the object, decide when to change the property value.
  Other components cannot directly change it. The only way they might end up triggering a change is
  if they call some of your methods, and you end up deciding to do the change.

- **Read Write**: Other components might call the setter method and change its value (without your
  permission or awareness).

By default, properties are dynamic and read-only.

Methods, properties and events can have quality-of-service attribute that can be:

- **Confirmed**. The call, response or notification is guaranteed to arrive. The order of emission
  and reception is maintained. For in-process interactions this is implemented as (potentially
  queued) function calls. TCP is used for inter-process communication.

- **Best Effort**. The call, response or notification will be sent as fast as possible. There are no
  guarantees regarding reception or ordering. Old data does not overwrite newer data. This is
  typically used when you are interested in the latest data, low latency and are Ok with sporadic
  missing packets. For in-process communication, this is implemented using (potentially queued)
  function calls. UDP unicast is used for inter-process method calls. UDP multicast is used for
  inter-process events.

![Screenshot](../assets/images/tcp_udp_light.svg#only-light){: style="width:250px; float: right;"}
![Screenshot](../assets/images/tcp_udp_dark.svg#only-dark){: style="width:250px; float: right;"}

You can clearly see that the guarantees that Sen provides are in line with those of TCP and UDP.
That said, keep in mind that in controlled environments (a single computer, a local area network
with bounded traffic, or basically anything that is not the internet) you are very likely going to
be fine with a best-effort approach, as packet loss would represent a non-nominal operation and is
probably a sign of some infrastructural or load calculation problem. That said, you can always
choose.

Well, you can always choose as long as it makes sense. Sen will not let you use a best-effort
transport if it detects that the information that has to be transported is unbounded, and you might
receive incomplete, incorrect or inconsistent data.

Methods are typically expected to execute when called, but this may create contention if there's a
significant amount of callers, as they would queue up. In Sen, you can mark methods so that they can
provide deferred answers. This allows them to ingest multiple calls without having to provide an
answer straight away. This allows for techniques like batching of responses, delegating those calls
to other elements (i.e. load balancing) or the parallelization of work.

**Object Ownership**.

In Sen, objects are strongly owned by the component that creates them. If the owning component goes
away (is halted, shut-down, crashes or gets disconnected), all its objects will be automatically
removed. Similarly, objects strongly own their state. This means that each object is responsible for
updating its properties (even if the property is writeable, it is the owning object who's in charge
of triggering the update).

**Asynchronous behavior**.

Sen is a fully asynchronous system. This means that when calling a method you should not expect it
to execute immediately after the call is made. It will not block. Sen will trigger the execution at
a future time and will call you back if you provide a callback function. If the method returns a
value, the callback will receive it, and you will be able to use it. If the method threw an
exception, you will be able to examine it and re-throw it if needed.

We have talked about objects, but so far we have not touched the topic of *types*. What types may I
use as an argument for a method? Or for my property?

## Types

The Sen type system is rich:

- Signed and unsigned integers (`u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`).
- Floating point values of 32 and 64 bits (`f32` and `f64`).
- Basics: Boolean (`bool`) and String (`string`).
- Time: Time stamps (`TimeStamp`) and time durations (`Duration`).
- Enumerations (with an integral storage `T`, `enum : T {...}`).
- Structures (with 0:n fields of any value type, `struct {...}`). Single inheritance is supported.
- Variants (holding 1:n distinct types , `variant { T1, ... Tn}`)
- Sequences (of any value type, bounded, unbounded, and fixed `sequence<T>`, `sequence<T, n>`,
  `array<T, n>`).
- Quantities (of any numeric type, with a given unit and an optional min and max value).
- Optionals (of any type).
- Classes and interfaces.

The variant type represents a type-safe union. An instance of variant at any given time either holds
a value of one of its alternative types, or in the case of error - no value. As with unions, if a
variant holds a value of some object type T, the object representation of T is allocated directly
within the object representation of the variant itself. Variant is not allowed to allocate
additional (dynamic) memory. A variant is *not* permitted to hold the same type more than once.

Sen will auto-generate the code to natively support these types in your language. The only type that
you have to "worry" about is the "class", as you will need to inherit from a generated base class in
order to implement your logic.

In Sen, you can define your types using a very small language called Sen Type Language (STL, more on
this later). For those interested in simulation, you can also use HLA FOM.

## Components

![Screenshot](../assets/images/component_light.svg#only-light){: style="width:600px; float: right;"}
![Screenshot](../assets/images/component_dark.svg#only-dark){: style="width:600px; float: right;"}

There are three things we can see in a component:

1. An *interface*, used to interact with other components.
2. The *functional logic* (the code) that is in charge of the actions. This logic often holds state.
3. An *execution context*, that allows the functional code to run. In Sen, this is a thread.

Components are self-standing. This means that we can always instantiate them, and they will run.
When components need to talk to each other, they will declare it and Sen will provide them
visibility.

For a system to execute, we need to provide a context where to run and way for components to talk to
each-other. In Sen, the inter-connection mechanism for components is called a kernel.

![Screenshot](../assets/images/one_process_light.svg#only-light){: style="width:700px"}
![Screenshot](../assets/images/one_process_dark.svg#only-dark){: style="width:700px"}

Components are also relocatable. This means that they can be moved to other execution contexts
(processes or threads) without having to change a single line of code.

![Screenshot](../assets/images/two_processes_light.svg#only-light){: style="width:1200px"}
![Screenshot](../assets/images/two_processes_dark.svg#only-dark){: style="width:1200px"}

When components are separated by process boundaries, Sen will take care of transparently handling
the inter-process-communication (IPC) while respecting their quality-of-service requirements.

### Moving Components

![Screenshot](../assets/images/container_light.svg#only-light){: style="width:450px; float: right;"}
![Screenshot](../assets/images/container_dark.svg#only-dark){: style="width:450px; float: right;"}

In Sen, you are free to run your components in the same or different thread, process or computer.
This gives you a great degree of flexibility to adapt. Just keep in mind that:

- When components are _far away_, more resources are required to make them run and interact. There
  are also fewer chances for interference.

- When components are _close to each other_, the interaction is cheaper, but the chances for
  interference are higher.

The underlying reason is resource sharing.

For example, let's say that you decide to put all your components in a single process. The effort to
move the data between them will be low, but if one component crashes or corrupts the memory it could
bring all your system down. Let's now say you go the other way and decide to have each component in
an independent process. In this case, a crash of a component will not bring the others down, but we
will be requesting the operating system to allocate more resources, create and schedule more
processes, copy more data between them and do more synchronization work.

You have to decide how and where to group your components based on the trade-offs you are willing to
accept and the needs of your project/environment. Sometimes, what matters is the convenience of
having everything in one process. Sometimes, what matters will be the performance gain of grouping
components that exchange huge amounts of data. Maybe, during development you will want to keep some
components running, while starting and stopping the one you are currently working on. You are in
control and can set the grouping that best fits your needs.

With Sen, these groupings are nothing more than entries in configuration files, and the good thing
is that you can have as many as you want and use them when and where you need.

There is another axis that we need to consider: moving components to processes in other computers.

This is also possible and still fully transparent to your components. In this case you need to
consider the following:

- You are gaining in isolation and robustness. If a machine halts or the OS becomes overloaded, only
  the components in that machine will be affected.

- You are gaining in computing power. You have more resources and can do more. Make sure you are
  using those resources effectively and locating components accordingly.

- You need a network between those computers. You will be imposing more on the infrastructure.
  Having a network card is a non-issue these days, but do not underestimate the effort of properly
  setting-up, configuring and maintaining a clean network layout. Remember also that switches and
  routers might be in the loop and those also need to be configured and maintained (i.e. IGMP
  Snooping, V-LANs, etc.).

- You will need a way to deploy your binaries, start/stop your processes and monitor their
  execution. Maybe you will also need to debug them when they crash or do performance profiling in a
  particular configuration.

- Testing will become more complicated. You now have more moving parts and the network is, by
  nature, a shared resource. This means that if you don't have an isolated network, running your
  tests in parallel will cause them interfere each-other and fail.

Now, the issues listed above are not unique to Sen, but common to any distributed system.
Thankfully, there are mature solutions that help us overcome many of these challenges during
development, testing and production: containers and orchestrators.

Containers allow you to create self-contained, deployable units of software that can be executed in
isolation and with a great degree of options, tools and features. They allow for things like hosting
versioned containers in repositories, layering, etc. If you are not familiar with containers, you
can think of them as extremely lightweight virtual machines that have no runtime performance
overhead. Tools like Docker Compose allow you to mimic distributed systems by isolating the
execution of multiple containers that communicate to each other over virtual networks and for which
you can allocate certain computer resources.

Container Orchestration Platforms, like Kubernetes, go beyond and allow the deployment, execution
management and monitoring of containers at scale over heterogeneous computing clusters.

### Grouping components

Components might depend on each other and some require to be initialized and started before others.
Groups are basically a mechanism to define that order and ensure consistency. This is not a new
concept (and in fact is widely used in similar systems, specially OS microkernels). In this case it
is fully defined by the environment, and it works as follows:

- Components always have an associated group.
- During start-up, we increase the group until we reach the highest.
- To increase to a given group, all components associated with it must have been correctly
  initialized and their execution started.
- During shut-down, we decrease the group until we reach the minimum.
- To reduce to a given group, all components associated with it must have been correctly stopped and
  their execution halted.

In the (somewhat unlikely) case that you are writing a component that would need to perform some
action during the initialization, loading or unloading, you are free to implement (override) the
corresponding `init()`, `load()` and `unload()` functions.

Say that we have components A, B and C. Components A and B are in group 1. Component C is in group
2\. The progression of the initialization logic would be as follows:

1. The kernel starts at group 0, and it advances to group 1.
2. In group 1, the kernel finds the *A* and *B* components, so it: (1) loads them, (2) initializes
   them, and gets them running. We have reached level 1.
3. In group 2, the kernel finds *C*, so it (1) loads it, (2) initializes it and (3) runs it.
4. The kernel does not find andy other group, so it reaches the "running" state.

At this point, if we command the kernel to shut-down, the kernel does the same process, but in
reverse.

1. In group 2, the kernel finds *C*, so it stops it and then unloads it.
2. In group 1, the kernel finds the *B* and *A* components, so it stops them, and then unloads them.
3. The kernel does not find andy other group, so it reaches the "stopped" state, and finishes the
   execution.

You see that during start-up the kernel proceeds by groups, and within each group it goes by layers:
loads all of them, initializes all of them and runs all of them. During shut-down, it does the
reverse, but it stops all components, and then unloads them.

## Buses

So far, we have said that components provide functionality within a system, run in an execution
context and can interact with each other using a well-defined interface over a communication
channel. Let's now dive a bit deeper and see how the communication takes place.

- Components are created and connected to each other over the kernel.
- There's one kernel per process (normally).
- There can be many components, even across processes and computers.
- In the kernel, components can find (and create) *sessions*. Sessions act like namespaces that
  allow the segregation of information per system. Sessions are identified by name.
- Within a session, components will find (and can create) *buses*. Buses also act as namespaces to
  segregate the communication, but with a finer degree of detail.
- Inside a bus, components can find (and publish) *objects*. Being a distributed object-oriented
  system, objects are the basic element in Sen.

![Screenshot](../assets/images/zoom_light.svg#only-light){: style="width:1400px"}
![Screenshot](../assets/images/zoom_dark.svg#only-dark){: style="width:1400px"}

This creates a system where objects can be found (and published) using a `<session>.<name>`
approach.

For example, a building monitoring system may use a session named "monitoring", and then have a bus
for each building. The buses could be named like `"monitoring.headquarters"`,
`"monitoring.offices"`, `"monitoring.depot"`, etc. If there would be another system, it could use a
different session name and define its own buses.

You are free to name your sessions and buses as you like, and follow your own criteria for
categorizing your object groupings. This is something that you need to evaluate based on the nature
of the problem that you are trying to solve, and the convenience that you might get from this
feature. The key take-away here is that you are able to segregate the communication between your
systems and within your system by using sessions and buses.

## Packages

Normally, a component would:

1. Detect objects published by other components (and interact with them), or
2. Publish your own objects, or
3. Do both

Sen needs to know the types of the objects to be able to help you with this. Here is where packages
help.

Packages streamline the handling of types in all situations. Think of a package as a catalog of
types that can be used by Sen when working with components. Some of those types are interface
definitions (classes), but packages can also contain *your* implementation of some classes. This
means that packages can hold general types *and* your functional logic.

For example, let's say that you have an interface definition with the classes w, x, y and z. You
don't know much about w and x, but want to implement y and z. You write your implementation for
those two classes and create a package out of that.

Now you have a package containing the binaries of your implementation. It also contains the
definition of all the types you work with and meta information that encapsulates all these facts.

You might be asking: What can we do with a package? We can use it as a factory for objects. Users of
your package are now able to instantiate objects of class y and z. They won't know the details of
your implementation, but they know the interfaces and can create objects of those types.

We need to explain the difference between a package and a good-old library. You might be asking: "if
the same can be achieved with a header file and a static or shared library, what value does this
mechanism provide?". The answer is that, in fact, *packages are libraries*. Specifically, packages
are shared libraries that can be natively loaded by a Sen kernel. The particularity of packages is
that they are created with a uniform interface that allows Sen to inspect all the meta information
it needs to understand what is being provided.

This means that Sen is able to instantiate objects (depending on the packages that you may want to
import) and use configuration parameters. This creates a huge opportunity for getting rid of
boilerplate code.

You see, there is a common pattern that most Sen applications share. Normally we:

1. Instantiate the components and get them to run.
2. Load some sort of configuration information / parametrization data.
3. Load a set of packages where we will find classes that can be instantiated.
4. Instantiate a set of classes (create our own objects).
5. Establish a connection between the component and to some set of buses.
6. Publish our objects to those buses.
7. Enter an execution loop where we will:
   1. Drain the inputs from the outside (discovered objects, received method calls, received events
      or received property changes).
   2. Perform some computation based on those inputs, and produce some outputs (published objects,
      outgoing method calls, emitted events or property changes of the owned objects).
   3. Flush those outputs so that other components can react to them.

![Screenshot](../assets/images/wheel_light.svg#only-light){: style="width:250px; float: right;"}
![Screenshot](../assets/images/wheel_dark.svg#only-dark){: style="width:250px; float: right;"}

If we don't do something about it, every Sen user will likely do the previous steps in sightly
different ways, using different approaches, getting the configuration data from different sources in
different formats and therefore ending up re-doing the same work many (*many*) times across an
undefined set of projects and with a wide range of bugs.

We can do something about this. We can identify what are the common needs here and provide a
reference implementation to allow users to automatically instantiate components that do all of this
in a structured way.

In most cases, there are only a few things that vary between applications:

- The packages that we need to import to be able to instantiate our objects.
- The objects that we want to instantiate and their initialization values.
- The buses where we want to publish our objects and discover the ones published by other
  components.
- The criteria we use to discover other objects.

Sen allows you to define all of this in a configuration file and let you focus on the business logic
to have an easier life getting some things done.

![Screenshot](../assets/images/parametrization.svg#only-light){: style="width:900px;"}
![Screenshot](../assets/images/parametrization_dark.svg#only-dark){: style="width:900px;"}

Keep in mind that the configuration file can contain the information for instantiating multiple
components in your process.

Being able to build and instantiate custom components based on a set of packages and a configuration
file is key for enabling a powerful degree of composition.

Packages become common assets. With the multidimensional modularity of Sen:

- We can define a rich interface for our objects by using the type system.
- We can decide how to group our implementation into reusable, deployable, modules by using
  packages.
- We can group objects to perform a well-defined function within a system by using components.
- We can define and parametrize components simply using configuration files.
- We can instantiate multiple components in a process by telling Sen which ones to load and run
  using a configuration file.
- We can change where those components are executed, and do this in a way that is transparent to our
  code, even if they are communicating over a network.
- We can do all of the above in a type-safe manner and without having to deal with low-level
  messaging constructs. Your focus will be in the logic you implement and the information you use,
  and not in how and where does it come from.

**Things to remember**

- Components are the units of execution in Sen.
- Packages are libraries of classes that can be loaded by the kernel.
- Packages contain your implementation for some classes.
- The Sen kernel is able to *build* custom components by importing packages and instantiating
  objects. This is done via configuration files. This is also the easiest and most flexible way of
  creating components.
- You can also implement your own components, with code.
