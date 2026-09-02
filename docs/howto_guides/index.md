# How-to guides

Each page here answers one question, and assumes you already have Sen installed and a package that
builds. If you do not, start with [Install](../getting_started/install.md) and
[Create your first package](../getting_started/first_package.md).

## Writing the code

[Working with objects](objects.md) is the one to read first: registering objects, subscribing to
other people's, calling methods and reacting to events. [Generated code](generated_code.md)
explains what Sen writes for you from an STL file and how to work with it.
[Writing components](components.md) goes further, into components that do more than instantiate
objects. [Using groups](using_groups.md) controls what runs before what.

## Time

[Controlling the clock](controlling_clock.md) covers real time, faster than real time, and stepping
a system one cycle at a time. [Precision sleeper](precision_sleeper_config.md) is for when a
component needs to hit its period accurately. [Dead reckoning](dead_reckoning.md) is about sending
less data by predicting motion between updates.

## Configuration and logging

[Python configuration](python_config.md) is for when YAML stops being enough and you want to build
a configuration programmatically. [Configuring the environment](configure_environment.md) covers
the variables Sen reads, and the per-component thread settings: priority, CPU affinity and stack
size. [Logging](logging.md) explains the log levels and how to change them at
runtime.

## Sharing interfaces between projects

[Exportable interfaces](exportable_interfaces.md) is how you publish an interface other projects
can implement, and [Consuming interfaces](consuming_interfaces.md) is the other side of that.
[Generating interface packages](generate_interface_packages.md) covers the build side, and
[Creating Conan packages](creating_conan_packages.md) covers distribution.

## Building and testing

[Building from source](building_from_source.md) is for working on Sen itself rather than on a
project that uses it. [Unit tests](unit_tests.md) shows how to drive a kernel from a test so your
objects can be tested without a running system.

## When something does not work

[Troubleshooting](troubleshooting.md) collects the failures people actually hit and what they mean.

## Fitting Sen to a system you already have

[Connecting existing systems](connecting_existing_systems.md) answers the question most
people arrive with: can Sen talk to the system I already have? It lays out the shapes a
bridge can take and what each one costs.

## Background reading

[Design considerations](considerations.md) is not a how-to at all. It is a longer piece on the
trade-offs behind Sen's design. Read it once you have a system of your own to compare it
against.
