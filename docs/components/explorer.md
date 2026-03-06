# The Sen Explorer

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/fix/images/explorer_overview.gif){: style="width:1200px;"}

The Sen Explorer component starts a lightweight GUI to helps you understand and monitor the Sen
world. You can see it as a graphical Shell. With it, you are able to:

- View the available sessions.
- View the buses which are available within the sessions.
- Connect to specific buses. You can concurrently connect to multiple buses and multiple sessions.
- List the objects which are published it the buses you are connected to.
- For each object, you are able to see the owner process, and all properties.
- Monitor the events produced by the objects, inspect their arguments and filter them.
- Plot data from multiple sources, independently of the object, property or field within.

The GUI itself should be self-explanatory, so this document will only highlight the most relevant
points for using it.

## Getting it started

You can use it in two ways:

1. Start it stand-alone, by executing `sen explorer`.
2. Embed it in your process by loading the `explorer` component.

The stand-alone mode will let you see whatever is published on the sessions and buses, but requires
networking (by definition). It is a good way to visualize what your components and processes are
doing without having to touch anything.

The embedded mode works exactly the same as the stand-alone, but also gives you access to the
objects which are not published over the network (put in the "local" session). It might also be more
responsive in some cases, as there's no need to transport the data across processes.

To load the explorer, add it to the configuration file.

```yaml title="embedding the explorer in your process"
load:
  - name: explorer
    group: 2
```

The explorer allows you to dock and position the different windows, and the configuration is
automatically saved and restored the next time you run it.

You can also define your own layout file:

```yaml title="embedding the explorer in your process"
load:
  - name: explorer
    group: 2
    layoutFile: my_imgui_layout_file.ini
```

## Explorer FAQs

1. What libraries do I need to install to run it? It's self-standing and makes use of OpenGL.
2. What library did you use to create the GUI? ImGui
