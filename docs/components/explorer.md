# The Sen explorer

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/docs-assets/explorer_overview.webp){: style="width:1200px;"}

The Sen explorer component starts a lightweight GUI to help you understand and monitor the Sen
world. You can see it as a graphical shell. With it, you are able to:

- View the available sessions.
- View the buses which are available within the sessions.
- Connect to specific buses. You can concurrently connect to multiple buses and multiple sessions.
- List the objects which are published in the buses you are connected to.
- For each object, you are able to see the owner process, and all properties.
- Monitor the events produced by the objects, inspect their arguments and filter them.
- Plot data from multiple sources, independently of the object, property or field within.

The GUI itself should be self-explanatory, so this document will only highlight the most relevant
points for using it.

## Getting it started

You can use it stand-alone or embedded:

1. Start it stand-alone, by executing `sen explorer`.
2. Embed it in your process by loading the `explorer` component.

The stand-alone mode will let you see whatever is published on the sessions and buses, but requires
networking (by definition). It is a good way to visualize what your components and processes are
doing without having to touch anything.

The embedded mode works exactly the same as the stand-alone, but also gives you access to the
objects which are not published over the network (put in the "local"
[session](../users_guide/glossary.md#session)). It might also be more responsive in some cases, as
there's no need to transport the data across processes.

To load the explorer, add it to the configuration file.

```yaml title="embedding the explorer in your process"
load:
  - name: explorer
    group: 2
```

The explorer allows you to dock and position the different windows. It writes that arrangement to a
file called `last-layout.ini` in the working directory, and reads it back the next time you run it.

You can also start from a layout file of your own:

```yaml title="starting the explorer from your own layout"
load:
  - name: explorer
    group: 2
    layoutFile: my_imgui_layout_file.ini
```

The explorer reads that file when it starts, but it keeps auto-saving to `last-layout.ini`. To write
what you have on screen back into your own file, save it from the Layouts menu (or press Ctrl+S).
"Save Layout As" (Ctrl+Shift+S) lets you write it somewhere else.

## Explorer FAQs

1. What do I need to run it? On Windows the explorer draws with Direct3D 11, which comes with the
   system. Everywhere else it needs SDL2 and OpenGL 3. SDL2 is installed next to the Sen binaries,
   so the OpenGL driver is the only thing you have to provide yourself.
2. What library did you use to create the GUI? ImGui
