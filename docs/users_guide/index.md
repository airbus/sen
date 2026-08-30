![Screenshot](../assets/images/eyes_light.svg#only-light){: style="width:150px; float: right;"}
![Screenshot](../assets/images/eyes_dark.svg#only-dark){: style="width:150px; float: right;"}

# Overview

- In Sen, you write *packages*. Packages contain your functionality in the form of classes that can
  be instantiated to create objects. Sen objects are regular C++ objects with "super-powers" infused
  by the Sen code generator.

- By writing config files, you tell Sen which packages to load and which objects to create. The
  grouping of objects that run in a thread and serve some specific purpose is called *component*.

- Components can be instantiated wherever you like (same process, same computer, other computers)
  and Sen makes this transparent to your code. You achieve this by publishing objects to *buses*.
  Sen *sessions* are namespaces for *buses*.

- Time can be virtualized. Sen can run your code in real time or stepped mode (slower or faster than
  real time, you choose).

- You can define your types and classes using a little language called STL, and it can also be done
  using standard HLA FOMs.

- Sen comes with a bunch of tools that let you interact with your systems, test them, script them,
  and create your own apps.

That's basically it. Now we take a step back and get an understanding of the bigger picture and how
everything fits together.

To understand how Sen sees the world, let's define a few lightweight concepts:

- **Classes** define the interface of objects.

- **Objects** encapsulate state (properties) and behavior (your code).

- **Packages** are libraries of classes.

- **Components** run your code by importing packages and instantiating objects.

- **Systems** are a collection of components that are organized for a common purpose.

## Where to go from here

**Start with the ideas.** [Mental model](mental_model.md) is the shortest route to thinking about
Sen the way Sen works. [Main concepts](main_concepts.md) goes over objects, components, buses and
the quality-of-service choices in more detail. [Execution model](execution_model.md) explains what
happens in a single cycle, and [Workflow](workflow.md) describes how a project usually grows.

**Then the language you write interfaces in.** [STL](stl.md) is the tour, and
[STL grammar](stl_grammar.md) is the exact syntax when you need to look something up. If your
interfaces come from somewhere else, [HLA FOMs](hla.md) covers the simulation standards, and
[Compatibility and conversions](compatibility_conversions.md) explains what happens when two sides
disagree about a type. [Sen Query Language](sql.md) is how you filter objects.

**Then running and building things.** [The configuration file](configuration.md) is the reference
for the YAML that `sen run` takes, and [Command line](command_line.md) documents the commands
themselves. [CMake](cmake.md) documents the build functions your own package will call.

**The libraries**, if you are working against the C++ API directly:
[core](core_library.md) for the base types, [kernel](kernel_library.md) for the runtime,
[util](util_library.md) for the helpers, [db](db_library.md) for recorded data and
[gen](gen_library.md) for the code generator. The database also has
[Python bindings](db_python_bindings.md) for reading recordings.

**When the network does not behave**, the [FAQ](faq.md) covers why Sen uses multicast, how to force
TCP, how to discover remote kernels without multicast at all, and the user limits WSL2 imposes.
