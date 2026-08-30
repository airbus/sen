![Screenshot](../assets/images/shell_light.svg#only-light){: style="width:150px; float: right;"}
![Screenshot](../assets/images/shell_dark.svg#only-dark){: style="width:150px; float: right;"}

# The Sen shell

## Getting it started

The Sen shell allows you to interact with all the objects that are published to the buses in the
kernel.

It makes use of the extensive introspection capabilities that are built into Sen. That's why you
get auto-complete, error-checking, argument parsing and even the ability to read the documentation.

You can run a stand-alone shell by typing:

```shell
sen shell
```

And you can load the shell component into any kernel. You do it like this:

```yaml
load:
  - name: shell
    group: 2     # you decide when to start the shell
```

The configuration options and the commands the shell offers are defined in the component's STL:

```rust title="Shell configuration"
--8<-- "snippets/shell.stl:config"
```

Once you start the shell, you can use tab to auto-complete and to see the available commands. If you
type `help` you will see more info about all the options you have.

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/docs-assets/shell_help.gif){: style="width:900px"}

To shut down, type Ctrl+D on an empty line, or use the `shutdown` command. Both stop the whole
kernel. If you loaded the shell next to other components, those stop too.

## Command reference

What you type is either one of the built-in commands below, or the address of something on a
[bus](../users_guide/glossary.md#bus) followed by the member you want and its arguments.

```rust title="The commands, from the shell's own interface"
--8<-- "snippets/shell.stl:commands"
```

Anything else is an address of the form `session.bus.object.member`:

| To do this | Type |
|---|---|
| Read a property | `local.counters.myCounter.getValue` |
| Write a `[writable]` property | `my.tutorial.timer.setNextProgram "20 s"` |
| Call a method, one argument | `my.tutorial.fibManager.computeFibonacci 8` |
| Call a method, several arguments | `my.tutorial.calc1.add 10, 5` |
| Call a method taking strings | `local.replay.replayer.open "my_replay", "school_recording"` |
| Call a method that returns nothing | `my.tutorial.client1.useCalculator` |
| Dump an object's current state | `my.tutorial.elon.print` |
| Inspect a type or instance | `info local.counters.myCounter` |

Arguments are separated by commas. Strings, durations and enumeration values are quoted; numbers
are not. Properties are read with the generated <code>get&lt;<var>Prop</var>&gt;()</code> accessor
and written with <code>setNext&lt;<var>Prop</var>&gt;()</code>, exactly as they are from C++, and
only `[writable]` properties expose a setter.

A getter prints the property name alongside its value, a method prints the returned value on its
own, strings come back quoted, and a setter that worked prints nothing whatsoever:

```text
sen:host/config> local.example.myObject.getProp5
- prop5: 453

sen:host/config> local.example.myObject.addNumbers 10, 5
15

sen:host/config> local.example.myObject.echo hello
"hello"

sen:host/config> local.example.myObject.setNextProp5 42

sen:host/config>
```

So the empty line after `setNextProp5` is success, not a swallowed error. Read the property back if
you want to see the effect, bearing in mind that the write lands on the next cycle rather than
this one, and that if the object assigns that property itself during `update()` then your value is
whatever the object last wrote, not the one you typed.

Calls do not block. Sen is asynchronous, so the shell returns immediately and prints the result when
it arrives, which may be a cycle or two later.

## Opening and closing buses

The `ls` command is especially useful because it allows you to see all the objects that are visible
by the shell.

The shell does not see any objects by default. The only thing you can see are the available
sessions. You need to open sessions and their buses so that the shell can start discovering objects.

The `open` command can be used to connect to a bus or to open a session and discover the available
buses. This command has auto-complete capabilities to help you. Let's give it a try.

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/docs-assets/shell_open_ls_close.gif){: style="width:900px"}

You can see that the shell lets you know when objects are discovered. The `ls` command shows them.
Unsurprisingly, the `close` command does the inverse.

Sometimes you will find yourself having to open some buses every time you load the shell. This is
because you are interested in working with some objects and the shell is not opening any bus by
default. To change this, you can set the `open` parameter in your configuration file and the shell
will automatically open them at the start:

```yaml
load:
  - name: shell
    group: 2
    open: [ local.kernel ]  # automatically open the 'local.kernel' bus
```

## Calling methods on objects

The shell allows you to invoke methods on objects, no matter if they are local or remote. Name the
object, then the method, then the arguments:

```text
my.tutorial.calc1.add 10, 5
```

The animation below shows the same thing against an object's `addNumbers` method.

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/docs-assets/instantiate_class.gif){: style="width:1200px"}

Keep in mind that due to Sen's asynchronous nature, the shell won't block and the call results will
eventually arrive.

The shell is meant to be an interactive interface. For scripting, see [what ships with
Sen](index.md#what-ships-with-sen); several of those components expose the same objects to code.

## Introspecting the system

You can use the shell to get information about the system. For example, you can use the kernel
object to find out the available types, units and the build information of the kernel and all
components.

And there's also a method named `info`, that you can use to inspect all the details of the available
types (the built-in types, and also any type received over the network during execution).

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/docs-assets/shell_introspection.gif){: style="width:1200px"}

## Creating queries

When you `open` a bus, you are declaring an interest in all the objects that are published in it.
You can inspect the list of active interests by using the `src` command. For example, if we call
`open local.kernel` and then call `src` we will see the following:

```text
  Name               Query
  local.kernel.all   SELECT * FROM local.kernel
```

This means that we have an active query named "local.kernel.all" which defines an interest in all
objects of that bus.

You can create your own queries, and therefore use the shell to monitor certain objects. For
example, let's create a query that only notifies us about the entities in the se.env bus, with a
force ID property that is "neutral" or "friendly". We call that query "friendly_fire":

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/docs-assets/shell_query.gif){: style="width:1200px;"}

You can now see that the shell automatically opened the se.env bus and created a query named
se.env.friendly_fire.

As soon as the query is active, Sen automatically evaluates the objects on the bus. The shell will:

* Notify you of discovered objects that meet the query conditions. Once an object has been
  discovered, you can interact with it as usual.
* Notify you when an object no longer meets the query conditions (or is destroyed), meaning it has
  been removed from your view.

You can always run the `ls` command to see the objects currently matching your active queries.

Note: Currently, you cannot see which object on a bus belongs to a specific query.

To close/delete a query, use the `close` command and pass the name of the query.

If you want the shell to automatically open some predefined queries, do as follows:

```yaml title="Configuring the shell to auto open queries"
load:
  - name: shell
    query:
      - name: lat_in_range
        selection: "SELECT rpr.PhysicalEntity FROM se.env WHERE spatial.SpatialFPStruct.worldLocation.x BETWEEN 0.0 AND 500.0"
      - name: friendly_fire
        selection: "SELECT rpr.PhysicalEntity FROM se.env WHERE forceIdentifier IN (\"friendly\", \"neutral\")"
```

## Connecting to a remote shell

Sometimes, during development, you will run your Sen application in a remote computer. Maybe you
don't have ssh access, or the process was started by a script, launcher or orchestrator. How would
you connect to the Sen process and access its shell? Enter the remote shell.

You can connect to a remote shell by doing `sen rshell`. For example:

```console title="connecting to a remote shell"
sen rshell 192.168.1.44:8094
```

This process is actually very light and doesn't run any Sen kernel at all. It just *talks* to a
shell running in a remote Sen kernel over a TCP channel.

Remote shell access is off unless you set `serverEnabled`, and the connection is unauthenticated, so
it is intended for trusted networks.

It simply sends your key presses to the remote shell and receives low-level terminal drawing
commands from it.

This means that you have all the power of the shell in your local terminal, even if your local
terminal is Windows and the Sen process is on a Linux computer.

For example, here you can see how we start a Sen process that hosts a shell that can only be
remotely accessed, and then we use the `sen rshell` tool to connect to it.

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/docs-assets/shell_remote.gif){: style="width:1200px;"}

To configure your shell to accept remote connections you need to set the `serverEnabled` and
`serverPort` parameters. The last example used the following:

```yaml title="configuring a remote shell"
# Here we instantiate a shell, but it is meant to be accessed remotely.
# We do so by setting the serverEnabled parameter to true. You can optionally
# set a custom port using the serverPort parameter.
#
# If you start this process, you will see nothing, but if you start a connection
# to it, you will see that it is like working locally. To do so run:
#
#   sen rshell <hostname>:8094
#
load:
  - name: shell
    group: 2
    open: [ local.tutorial ]  # to see the created objects
    serverEnabled: true
    serverPort: 8094
```
