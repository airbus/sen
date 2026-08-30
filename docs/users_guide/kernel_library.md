# sen::kernel

The Sen kernel library is Sen's runtime environment. It is responsible for instantiating
components, coordinating their execution, and managing the communication between them.

## Responsibilities

| Area | What the kernel does                                                                          |
|------|-----------------------------------------------------------------------------------------------|
| **Bootloader** | Parses the YAML configuration file into the `KernelConfig` the kernel is constructed from     |
| **Component lifecycle** | Loads, initialises, runs, stops, and unloads components in group order                        |
| **Execution scheduling** | Drives the drain-update-commit cycle at the configured `freqHz`                               |
| **Transport abstraction** | Routes method calls, property updates, and events between components via pluggable transports |
| **Session and bus management** | Maintains the session/bus/object namespace and handles broker-less discovery                  |
| **Object ownership** | Removes all objects owned by a component when that component disconnects or crashes           |

## Execution cycle

Each component the kernel builds from the `build:` section of a configuration runs the following
cycle at its configured frequency:

1. **Drain**: pull in all pending inputs (discovered objects, incoming calls, received events,
   property changes from remote objects).
2. **Update**: Sen calls `update()` on the objects the component registered, which is where your
   logic lives.
3. **Commit**: flush all outputs (property changes on owned objects, outgoing method calls,
   emitted events) so that other components can react to them.

A component you write yourself in C++ owns its loop instead, making these three calls itself or
handing them to `execLoop`. See [Writing a component](../howto_guides/components.md). The
[Execution model](execution_model.md) page treats the cycle in more depth.

## Using the kernel from code

The most common way to instantiate a kernel is through a YAML configuration file and the `sen run`
command. To embed a kernel in your own program, include `sen/kernel/bootloader.h`, parse the
configuration with `Bootloader::fromYamlFile()` or `Bootloader::fromYamlString()`, and construct a
`Kernel` from the resulting `KernelConfig`. The Doxygen reference has the details.

## Replacing the transport

The kernel reaches the network through an interface, so a deployment can carry Sen traffic over
something other than `ether`. `sen/kernel/transport.h` declares both halves. Implement `Transport`
to send, and the kernel hands you a `TransportListener` in `start()` for what arrives.

This changes what carries traffic between kernels. It does not let a program in another language
join a bus, since the wire format and discovery belong to whichever transport is installed. See
[Connecting existing systems](../howto_guides/connecting_existing_systems.md).

## See Also

- [Execution model](execution_model.md)
- [The configuration file](configuration.md): every key the kernel reads from YAML
- [Main concepts: components](main_concepts.md#components)
- [API Reference](../doxygen_gen/html/index.html): full Doxygen documentation (available after
  building the docs)
