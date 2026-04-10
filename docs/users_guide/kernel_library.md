# sen::kernel

The Sen Kernel library is Sen's runtime environment. It is responsible for instantiating
components, coordinating their execution, and managing the communication between them.

## Responsibilities

| Area | What the kernel does                                                                          |
|------|-----------------------------------------------------------------------------------------------|
| **Bootloader** | Parses the YAML configuration file and creates the kernel instance                            |
| **Component lifecycle** | Loads, initialises, runs, stops, and unloads components in group order                        |
| **Execution scheduling** | Drives the drain -> update -> commit cycle at the configured `freqHz`                         |
| **Transport abstraction** | Routes method calls, property updates, and events between components via pluggable transports |
| **Session and bus management** | Maintains the session/bus/object namespace and handles broker-less discovery                  |
| **Object ownership** | Removes all objects owned by a component when that component disconnects or crashes           |

## Execution cycle

Each component runs the following cycle at its configured frequency:

1. **Drain** - pull in all pending inputs (discovered objects, incoming calls, received events,
   property changes from remote objects).
2. **Update** - run the component's `update()` function, which is where your logic lives.
3. **Commit** - flush all outputs (property changes on owned objects, outgoing method calls,
   emitted events) so that other components can react to them.

See the [Execution Model](execution_model.md) page for a deeper treatment.

## Using the kernel from code

The most common way to instantiate a kernel is through a YAML configuration file and the `sen run`
command. For embedding a kernel programmatically, include `sen/kernel/kernel.h` and use the
bootloader API - refer to the Doxygen reference for details.

## See Also

- [Execution Model](execution_model.md)
- [Command Line reference](command_line.md) - YAML configuration schema
- [Main Concepts - Components](main_concepts.md#components)
- [API Reference](../doxygen_gen/html/index.html) - full Doxygen documentation (available after building the docs)
