# The Sen Terminal (BETA)

[Term](https://raw.githubusercontent.com/airbus/sen/refs/heads/fix/images/term.jpg){: style="width:700px"}

The Sen Terminal (`term`) is a modern TUI-based interactive terminal for the Sen kernel. Built on
the FTXUI framework, it provides a rich, mouse-enabled interface with multiple panes, guided-input
forms, live property monitoring, and event streaming. It is designed as the successor to the
[shell](shell.md) component, offering a significantly richer interactive experience while
maintaining the same core workflow: open buses, discover objects, invoke methods, and inspect types.

The terminal supports two display modes: **TUI** (the default, full multi-pane layout) and
**REPL** (a simplified single-pane layout without the watch pane, status bar, or log/event panes).
Both modes share the same command set, Tab completion, value formatting, and themes. See the
[Display modes](#display-modes) section for details.

## Getting it started

You can load the term component into any kernel configuration file:

```yaml
load:
  - name: term
    group: 2
    open:
      - local.main
```

You can also run it standalone with the `sen term` shortcut:

```shell
$ sen term              # starts in TUI mode (default)
$ sen term --repl       # starts in REPL mode
```

Or as part of a larger configuration:

```shell
$ sen run my_config.yaml
```

On startup, the terminal displays a welcome banner showing the Sen version, compiler information,
git branch, and a random engineering quote. Below the banner, a prompt appears ready for input.

<!-- TODO: screenshot of the startup banner -->
<!-- ![Startup banner](../assets/images/term_startup.png){: style="width:900px"} -->

To exit the terminal, type `exit`, `shutdown`, or press Ctrl+D or Escape. These request the kernel
to stop all components gracefully. Pressing Ctrl+C kills the terminal immediately without shutting
down the kernel.

## Display modes

The `mode` configuration option selects the display layout:

| Mode   | Description                                                                                     |
|--------|-------------------------------------------------------------------------------------------------|
| `tui`  | Full multi-pane TUI with status bar, watch pane, log/event panes, and F-key shortcuts (default) |
| `repl` | Simplified single-pane layout. Command output, completions, and the prompt only                 |

### REPL mode

REPL mode provides a lighter interactive experience. It uses the same fullscreen rendering as TUI
mode but strips away the peripheral panes:

- No status bar, watch pane, or log/event panes
- Events from listeners are printed inline with command output
- Logs go to stderr via spdlog (not captured into a pane)
- Mouse wheel and PageUp/PageDown scroll the command output
- The `watch`, `unwatch`, and `watches` commands are not available

All other commands, Tab completion, value formatting, themes, guided-input forms, and async call
tracking work identically in both modes.

When running standalone, use the `--repl` flag:

```shell
$ sen term --repl
```

Or set it in the configuration file:

```yaml
load:
  - name: term
    group: 2
    mode: repl
    open:
      - local.main
```

## Layout overview

The terminal screen is divided into five areas that work together:

![Layout overview](https://raw.githubusercontent.com/airbus/sen/refs/heads/fix/images/term_layout_annotated.jpg){: style="width:900px"}

### Command pane

The command pane is the main output area, occupying the top-left of the screen. Every command you
type produces output here: `ls` results, method return values, `inspect` output, help text, and
discovery notifications (objects appearing and disappearing).

The pane maintains a scrollable history of up to 5000 lines. Scroll with:

- **PageUp / PageDown**: move by page
- **Mouse wheel**: scroll line by line
- **Home / End**: jump to top or bottom of history

When new output is appended, the pane automatically scrolls to the bottom. If you've scrolled up
to read earlier output, the auto-scroll pauses until you return to the bottom.

### Watch pane

The watch pane appears on the right side of the screen and displays live property values. Toggle
its visibility with **F5**. When hidden, the status bar shows the watch count and a reminder that
F5 opens it.

Property values update in real time. When a value changes, the property name briefly flashes bold
to draw your attention. Values are formatted with type-aware coloring: numbers in green, strings
in orange, enums with their description, structs and sequences as indented trees.

The watch pane is grouped by object: each object's name appears as a header, with its properties
listed below using tree connectors. You can resize the pane by dragging its left border with the
mouse.

### Log pane

The log pane appears at the bottom of the screen and has two tabs: system log output from spdlog
and event emissions from listeners. Toggle the logs tab with **F3** and the events tab with **F4**.
Resize the pane by dragging its top border.

The log pane maintains up to 1000 lines of history. When hidden, the status bar shows an unread
count if new messages have arrived.

### Status bar

The status bar is a solid-colored strip between the command pane and the log pane. It displays at
a glance:

- **Left side**: application name and hostname, current scope path
- **Right side**: object counts (scope/total), active watches, active listeners, open sources,
  pending calls, and the current time

When a guided-input form is open, the status bar switches to show contextual key hints for the
current editor (e.g., "Tab: next field  Enter: submit  Esc: cancel").

### Input line

The input line sits at the very bottom of the screen. It shows the prompt (e.g., `sen:/local.main >`)
followed by your typed text. Features:

- **History navigation**: Up/Down arrows cycle through previously typed commands. History is
  persisted to disk across sessions (`~/.sen_history.txt`).
- **Cursor movement**: Left/Right arrows, Home/End, Backspace, Delete all work as expected.
- **Line wrapping**: Long commands automatically wrap to multiple lines rather than scrolling
  horizontally.
- **Paste**: Your terminal's native paste (Ctrl+Shift+V on Linux, Ctrl+V in Windows Terminal)
  works normally since text arrives as keystrokes.

## Tab completion

Tab completion is deeply integrated and available in almost every context. Press **Tab** to trigger
it; a multi-column grid of candidates appears above the input line.

### How it works

1. **Type a partial word and press Tab.** If there's a single match, it auto-inserts. If multiple
   matches exist, the grid appears showing all candidates.
2. **Press Tab again** to cycle forward through candidates. **Shift+Tab** cycles backward. The
   selected candidate is highlighted (inverted text).
3. **Press Enter** to accept the selected candidate and execute. **Press `.`** to accept and
   continue typing a deeper path (e.g., accepting `showcase` then typing `.` to see its methods).
4. **Press Escape** to dismiss the completion list and restore your original text.

### What gets completed

| Context                        | What Tab completes                                        |
|--------------------------------|-----------------------------------------------------------|
| Empty input                    | All built-in commands                                     |
| Partial command (`he`)         | Matching commands (`help`)                                |
| Object path (`local.`)         | Available buses, then objects                             |
| Object + dot (`showcase.`)     | Methods, properties, and events on that object            |
| `open` argument                | Available (not-yet-opened) sources                        |
| `close` argument               | Currently open sources and queries                        |
| `cd` argument                  | Child scopes, `..`, `/`, `@query` names                   |
| `query` argument               | `SELECT`, type names, `FROM`, bus names, `WHERE` keywords |
| `watch` / `unwatch` argument   | Object paths, object.property paths, `all`                |
| `listen` / `unlisten` argument | Object paths, object.event paths, `all`                   |
| `log` argument                 | Subcommand (`level`), log level names, logger names       |
| `theme` argument               | Available theme names                                     |
| `help` argument                | Command names                                             |
| `inspect` argument             | Object paths and type names                               |
| `types` argument               | Registered type names                                     |
| `units` argument               | Unit category names                                       |

### Auto-chaining

When there's a single match at an intermediate path level, Tab automatically inserts it and
continues to the next level. For example, if `local` is the only session, typing `l` and pressing
Tab produces `local.` and immediately shows the buses under it. If `main` is the only bus, another
Tab produces `local.main.` and shows its objects.

This means you can reach deep objects with rapid Tab presses without manually typing dots.

## Navigation and scoping

The terminal supports scoped navigation, which narrows the view to a specific part of the object
hierarchy. This shortens the object paths you need to type and focuses `ls` output on what's
relevant.

### The `cd` command

```
/ > cd local.main
/local.main > cd ..
/ > cd -
/local.main >
```

Supported targets:

| Target        | Effect                                       |
|---------------|----------------------------------------------|
| `session.bus` | Navigate directly to a bus                   |
| `session`     | Navigate to a session (see its buses)        |
| `objectGroup` | Navigate into a sub-group of the current bus |
| `..`          | Go up one level                              |
| `/`           | Return to root                               |
| `-`           | Go back to the previous scope                |
| `@queryname`  | Scope to the results of a named query        |

When scoped to a bus, object paths become relative. Instead of typing
`local.main.showcase.moveTo`, you can type just `showcase.moveTo`.

### The `ls` command

`ls` lists the objects visible in the current scope as a tree view with type annotations:

```
/local.main > ls
  ├── showcase  [term_showcase.ShowcaseImpl]
  ├── worker_0  [term_showcase.Worker]
  └── worker_1  [term_showcase.Worker]
```

You can filter by passing a partial name: `ls worker` shows only matching entries. At root scope,
`ls` shows sessions and buses.

### The `pwd` command

`pwd` prints the current scope path. Useful in scripts or when the prompt is too long to read at a
glance.

## Opening and closing buses

The terminal does not see any objects by default. You must open sessions and buses so that the
terminal can start discovering objects.

### Opening sources

```
/ > open local
  + source local opened
/ > open local.main
  + source local.main opened
  + showcase  [term_showcase.ShowcaseImpl, Native]
  + worker_0  [term_showcase.Worker, Native]
```

When you open a session (single name like `local`), the terminal discovers its available buses.
When you open a bus (dotted name like `local.main`), the terminal subscribes to all objects on
that bus and starts receiving discovery notifications.

Discovery notifications appear in the command pane. For small batches (up to 5 objects), each
object is listed individually with its type. For larger batches, a summary count is shown.

### Closing sources

```
/ > close local.main
  - source local.main closed
```

Closing a bus disconnects all subscriptions. Any watches or listeners on objects from that bus
will enter a disconnected state (and reconnect if the bus is reopened).

### Auto-opening at startup

To avoid typing `open` commands every time, list your sources in the configuration:

```yaml
load:
  - name: term
    group: 2
    open:
      - local.main
      - local.kernel
```

## Calling methods

Method invocation is the primary way to interact with objects. Type the object path, a dot, and
the method name:

```
/local.main > showcase.ping
```

### Methods without arguments

If the method takes no arguments, it executes immediately when you press Enter. The command pane
shows a spinner while the call is in flight, which turns into a green checkmark on success or a
red cross on failure. The return value (if any) is displayed with type-aware formatting below the
echo line.

### Methods with arguments: the guided-input form

When a method requires arguments, pressing Enter after the method name opens a guided-input form
at the bottom of the screen. The form displays each argument as a named field with a type-specific
editor.

<!-- TODO: animated gif showing the form opening, filling fields, and submitting -->
<!-- ![Guided-input form](../assets/images/term_form.gif){: style="width:900px"} -->

Navigate between fields with **Tab** (forward) and **Shift+Tab** (backward). The currently focused
field is highlighted, and the status bar shows the field's type and available key bindings.

#### Scalar editors

| Type                         | Editor               | Keys                                                                      |
|------------------------------|----------------------|---------------------------------------------------------------------------|
| **bool**                     | Toggle checkbox      | Space to toggle                                                           |
| **integer** (i32, u64, etc.) | Text field with spin | Up/Down to increment/decrement, clamped to type range                     |
| **float** (f32, f64)         | Text field           | Type decimal numbers                                                      |
| **string**                   | Text field           | Free-form text input                                                      |
| **enum**                     | Cycle selector       | Left/Right arrows to cycle through enumerators; description shown in hint |
| **Duration**                 | Text field           | Type with unit suffix (e.g., `100ms`, `1.5s`, `2m`)                       |
| **Timestamp**                | Text field           | Type as ISO 8601 string                                                   |

#### Composite editors

| Type         | Editor                              | Keys                                                               |
|--------------|-------------------------------------|--------------------------------------------------------------------|
| **struct**   | Named sub-fields in a tree          | Tab into each field                                                |
| **sequence** | Indexed elements, dynamically sized | **Ctrl+N** to add element, **Ctrl+X** to remove focused element    |
| **variant**  | Type selector + value editor        | Left/Right to change the active type; value editor adapts          |
| **optional** | Presence toggle + value editor      | **Ctrl+O** to toggle between empty and filled                      |
| **quantity** | Value field + unit selector         | Edit the numeric value; Left/Right on the unit row to change units |

Bounded sequences (with a max size defined in the type) show a count indicator and disable Ctrl+N
when at capacity. Fixed-size arrays start with all elements pre-filled and disable both add and
remove.

#### Inline arguments

You can also pass arguments directly on the command line without opening the form. For simple
types, just type the value after the method name:

```
/local.main > showcase.setFraction 0.75
```

For complex types, use JSON syntax:

```
/local.main > showcase.moveTo {"x": 10, "y": 20}
```

If you type the method name with no arguments but the method expects them, the form opens. If you
type invalid arguments, an error is shown with the expected method signature.

<!-- TODO: screenshot of the method signature display (manpage-style) -->
<!-- ![Method signature](../assets/images/term_signature.png){: style="width:900px"} -->

### Async call tracking

All method calls are asynchronous. The command pane shows a braille spinner next to the call text
while the call is pending, along with an elapsed-time counter. When the result arrives, the spinner
is replaced by:

- A green **checkmark** with the return value (on success)
- A red **cross** with the error message (on failure)

Multiple calls can be in flight simultaneously. Each one tracks independently and resolves in the
order the results arrive.

## Watching properties

The watch system is one of the terminal's most powerful features. It lets you monitor property
values in real time, displayed in the dedicated watch pane rather than cluttering the command
output.

<!-- TODO: animated gif showing watches being added, values changing with flash highlights -->
<!-- ![Watch system](../assets/images/term_watches.gif){: style="width:900px"} -->

### Adding watches

**Watch a single property:**

```
/ > watch local.main.showcase.temperature
  Watching 'local.main.showcase.temperature'.
```

**Watch all properties on an object:**

```
/ > watch local.main.showcase
  Watching 12 properties on 'local.main.showcase'.
```

This creates one watch entry per property. The watch pane opens automatically when you add watches.

### Watch pane display

Watches are grouped by object path. Each object appears as a section header, with its properties
listed below using tree connectors:

```
 Watches (F5)
 ├ showcase
 │ ├ temperature: 23.5
 │ ├ status: running (system is operational)
 │ ├ workerCount: 3
 │ └ enabled: true
 └ worker_0
   ├ position: {x: 10, y: 20}
   └ health: 0.95
```

Values are formatted with type-aware styling:

- **Numbers** in light green
- **Strings** in orange with quotes
- **Enums** in green with description in parentheses
- **Booleans** in green (`true` / `false`)
- **Structs** as indented trees with field names
- **Quantities** with unit abbreviation (e.g., `42 m`)

### Change highlighting

When a property value changes, its name briefly flashes **bold** for about 20 frames (~0.7 seconds
at 30 Hz), then fades back to normal. This makes it easy to spot which values are actively
updating, even in a dense watch list.

### Connection lifecycle

Watches track the lifecycle of their target objects:

| State            | Appearance                   | Meaning                                                         |
|------------------|------------------------------|-----------------------------------------------------------------|
| **Connected**    | Normal text with live values | Object is present and property is being monitored               |
| **Disconnected** | Greyed out, last known value | Object was removed from the bus (e.g., a worker was despawned)  |
| **Pending**      | Yellow "(waiting)" text      | Object has never been seen; watch will activate when it appears |

When a disconnected object reappears (e.g., a worker is respawned), the watch automatically
reconnects, fetches the current value, and returns to the connected state. No user intervention
is needed.

### Pending watches

You can watch objects that don't exist yet:

```
/ > watch local.main.worker_5
  Watching 'local.main.worker_5' (pending - object not yet discovered).
```

The watch pane shows the entry as pending. When `worker_5` eventually appears on the bus, the
terminal automatically resolves the path, subscribes to the property (or all properties if it was
a whole-object watch), and begins displaying live values.

This is particularly useful for dynamic systems where objects come and go.

### Removing watches

```
/ > unwatch local.main.showcase.temperature    # remove a single watch
/ > unwatch local.main.showcase                # remove all watches on an object
/ > unwatch all                                # remove everything
```

The `watches` command lists all currently active watch entries.

### Config-driven watches

Pre-configure watches in your YAML file so they activate automatically at startup:

```yaml
load:
  - name: term
    group: 2
    open: [local.main]
    watch:
      - local.main.showcase.temperature
      - local.main.showcase.workerCount
      - local.main.worker_0
```

Config-driven watches start in pending state and activate as objects are discovered. This lets you
set up a monitoring dashboard that's ready as soon as the system starts.

## Listening to events

Event listeners subscribe to object events and stream their emissions to the dedicated events pane
(toggle with **F4**). This separates events from diagnostic log output for easier monitoring.

### Adding listeners

**Listen to a single event:**

```
/ > listen local.main.showcase.thresholdCrossed
  Listening to 'local.main.showcase.thresholdCrossed'.
```

**Listen to all events on an object:**

```
/ > listen local.main.worker_0
  Listening to 3 events on 'local.main.worker_0'.
```

### Event display

Each event emission appears in the log pane as a single line with:

- **Timestamp** of the event emission (from the source, not the display time)
- **[event]** badge in yellow
- **Event key** (e.g., `local.main.showcase.thresholdCrossed`)
- **Argument values** formatted inline with named fields

```
14:32:05.123 [event] local.main.showcase.thresholdCrossed  property: "temperature"  oldValue: 22.5  newValue: 25.1
```

### Lifecycle

Like watches, listeners support:

- **Pending mode**: listen to objects that don't exist yet; activates when discovered
- **Auto-reconnection**: if the object disappears and returns, the listener resubscribes

### Removing listeners

```
/ > unlisten local.main.showcase.thresholdCrossed    # remove a single listener
/ > unlisten local.main.worker_0                     # remove all listeners on an object
/ > unlisten all                                     # remove everything
```

The `listeners` command lists all active listener entries.

### Config-driven listeners

```yaml
load:
  - name: term
    group: 2
    open: [local.main]
    listen:
      - local.main.showcase.thresholdCrossed
      - local.main.worker_0
```

## Creating queries

Queries let you define named object filters using Sen's selection language. They control which
objects are visible to the terminal and can be navigated into as a scope.

<!-- TODO: animated gif showing query creation, cd @query, ls showing filtered results -->
<!-- ![Queries](../assets/images/term_queries.gif){: style="width:900px"} -->

### Creating a query

```
/ > query workers SELECT term_showcase.Worker FROM local.main
  + query 'workers' created
```

The selection expression follows Sen's interest syntax:

- `SELECT <TypeName> FROM <session>.<bus>`: match objects of a specific type
- `SELECT * FROM <session>.<bus>`: match all objects
- `WHERE <condition>`: add property-based filters

The terminal automatically opens the bus specified in the `FROM` clause if it isn't already open.

### Using queries as scopes

Navigate into a query's result set with `cd @name`:

```
/ > cd @workers
@workers > ls
  worker_0
  worker_1
```

Only objects matching the query appear in `ls` output. This is a powerful way to focus on a subset
of objects in a busy system.

### Managing queries

- `queries`: list all active queries with their selection expressions
- `close <session>.<bus>.<queryname>`: remove a query

### Config-driven queries

```yaml
load:
  - name: term
    group: 2
    open: [local.main]
    query:
      - name: workers
        selection: "SELECT term_showcase.Worker FROM local.main"
      - name: all
        selection: "SELECT * FROM local.main"
```

## Inspecting types and objects

The `inspect` command provides detailed structural information about objects and types, displayed
with colored formatting and tree connectors.

### Inspecting an object

```
/ > inspect local.main.showcase
```

This displays:

1. **Class name** with parent classes (inheritance chain)
2. **Class description** (if available), wrapping to fit the terminal width
3. **Properties section**: each property with its name, type, and description. Properties are
   listed in declaration order, including inherited properties from parent classes.
4. **Methods section**: each method with its name, argument types, return type, and description.
   Inherited methods are included.

### Inspecting a type

You can inspect any registered type by name:

```
/ > inspect term_showcase.Point
```

For struct types, this shows all fields with their types and descriptions. For enum types, it
shows all enumerator values with their names, numeric keys, and descriptions. For sequence types,
it shows the element type and bounds (if any). For variant types, it shows all alternatives.

### Listing types

```
/ > types                   # list all registered types
/ > types Worker            # filter by name substring
```

Tab completion is available for type names in both `inspect` and `types`.

### Method signatures

When you type a method name and press Enter without arguments (for a method that requires them),
the terminal displays a manpage-style signature:

```
  METHOD showcase.moveTo
    Move the object to a new position.

  ARGUMENTS
    target : Point   the destination coordinates

  RETURNS void
```

Section headers are bold, type names are in teal, and descriptions are dimmed. This provides
quick reference without needing to run `inspect` on the whole object.

## Log management

The `log` command controls spdlog log verbosity at runtime, both globally and per-logger.

### Setting log levels

```
/ > log                              # show all loggers and their current levels
/ > log level info                   # set global level to info
/ > log level myComponent debug      # set myComponent's logger to debug
/ > log level trace                  # set global level to trace (very verbose)
```

Available levels: `trace`, `debug`, `info`, `warn`, `error`, `critical`, `off`.

### Log pane

The bottom pane has two tabs: **Logs** (toggle with **F3**) and **Events** (toggle with **F4**).
The logs tab shows system log output routed through spdlog. The events tab shows event listener
emissions in a structured two-level format with the event name and arguments on separate lines.
Each tab maintains up to 1000 lines of scrollable history.

When the bottom pane is hidden and new messages arrive, the status bar shows an unread count
to remind you to check.

### Important: use spdlog, not printf

The term component captures stderr and routes spdlog output to the log pane. However, **stdout
cannot be captured** because the TUI framework (FTXUI) uses it for rendering. Any `printf()`,
`std::cout`, or direct `write(STDOUT_FILENO, ...)` call from a component will corrupt the terminal
display.

Components running alongside the term should use spdlog for all diagnostic output:

```cpp
spdlog::info("my message: {}", value);     // appears in the log pane
spdlog::debug("debug info: {}", detail);   // appears if log level allows
```

This is the same constraint as vim, tmux, and other TUI applications: the terminal's stdout is
owned by the UI framework.

## Themes

The terminal ships with 10 color themes that control the appearance of all UI elements: panes,
status bar, completion area, value formatting, tree connectors, and log levels.

### Switching themes at runtime

```
/ > theme nord
  Theme changed to 'nord'.
```

Tab completion lists all available themes. The change takes effect immediately.

### Available themes

| Theme             | Style  | Description                                  |
|-------------------|--------|----------------------------------------------|
| `oneDark`         | Dark   | Atom One Dark                                |
| `oneLight`        | Light  | Atom One Light                               |
| `catppuccinMocha` | Dark   | Catppuccin Mocha (warm, pastel, **default**) |
| `catppuccinLatte` | Light  | Catppuccin Latte (warm, pastel)              |
| `dracula`         | Dark   | Dracula (purple accents)                     |
| `nord`            | Dark   | Nord (arctic blue)                           |
| `gruvboxDark`     | Dark   | Gruvbox Dark (warm retro)                    |
| `gruvboxLight`    | Light  | Gruvbox Light (warm retro)                   |
| `tokyoNight`      | Dark   | Tokyo Night (blue-purple)                    |
| `solarizedLight`  | Light  | Solarized Light (classic)                    |

### Setting the theme in configuration

```yaml
load:
  - name: term
    group: 2
    theme: nord
```

The theme names in the configuration file match the command names exactly.

### Setting the theme via environment variable

The `SEN_TERM_THEME` environment variable overrides both the default and configuration file theme:

```shell
$ SEN_TERM_THEME=nord sen run my_config.yaml
```

Priority order: default (`catppuccinMocha`) < configuration file < `SEN_TERM_THEME`.

## Kernel status

The `status` command displays a snapshot of the kernel's runtime state:

```
/ > status
  Run mode  realTime

  Components
  Name          Group  Cycle time  Objects
  term              2    30.0 Hz        0  realtime
  my_package        1   100.0 Hz       12  realtime
  showcase          1       N/A         5
```

When a transport is active (networked setup), the output also includes a transport section
showing cumulative UDP and TCP bytes sent and received.

## Mouse interaction

The terminal supports full mouse interaction:

- **Scroll** the command pane, watch pane, or log pane with the mouse wheel
- **Drag** the border between the command pane and log pane to resize vertically
- **Drag** the border between the command pane and watch pane to resize horizontally
- **Select text** by holding Shift while clicking and dragging (standard terminal behavior when
  mouse tracking is active), then copy with your terminal's Copy shortcut (Ctrl+Shift+C on most
  Linux terminals)

## Error handling

When something goes wrong, the terminal displays a styled error box:

```
  Error: Unknown Object
    No object named 'showcase2' in the current scope.
    Did you mean: showcase?
    Use 'ls' to see visible objects.
```

Errors include contextual hints:

- **Did-you-mean suggestions** based on edit distance when an object name is close to a known one
- **Expected type information** when method arguments don't match
- **Method signatures** when arguments are missing or malformed

## Configuration reference

All configuration fields with their defaults:

```yaml
load:
  - name: term
    group: 2

    # Starting scope. Empty string means root (/). Set to a bus address
    # (e.g., "local.main") to start scoped to that bus.
    initialScope: ""

    # Timestamp display format: "utc" or "local".
    timeStyle: utc

    # Color theme (default: catppuccinMocha). See the Themes section for
    # available values.
    theme: catppuccinMocha

    # Display mode: "tui" (full multi-pane layout) or "repl" (simplified
    # single-pane layout without watches, status bar, or log pane).
    mode: tui

    # Set to true to suppress the welcome banner on startup.
    noLogo: false

    # Timeout for async method calls. Methods that don't respond within
    # this duration are reported as failed.
    callTimeout: 30s

    # Bus address where the logmaster component publishes log messages.
    logBus: "local.log"

    # Sources to open automatically at startup. Each entry is a session
    # name (e.g., "local") or a bus address (e.g., "local.main").
    open:
      - local.main

    # Named queries to create at startup. Each entry has a name and a
    # Sen selection expression.
    query:
      - name: workers
        selection: "SELECT term_showcase.Worker FROM local.main"

    # Property watches to register at startup. Each entry is an object
    # path (watches all properties) or an object.property path (watches
    # one property). Watches start in pending state and activate as
    # objects are discovered.
    watch:
      - local.main.showcase.temperature
      - local.main.worker_0

    # Event listeners to register at startup. Same syntax as watch but
    # for events. Each entry is an object path (listens to all events)
    # or an object.event path.
    listen:
      - local.main.showcase.thresholdCrossed
```

## Command reference

| Command     | Syntax                             | Description                                                  |
|-------------|------------------------------------|--------------------------------------------------------------|
| `help`      | `help [command]`                   | Show all commands, or detailed help for a specific command   |
| `cd`        | `cd <target>`                      | Change scope (`..`, `/`, `-`, `session.bus`, `@query`)       |
| `pwd`       | `pwd`                              | Print the current scope path                                 |
| `ls`        | `ls [filter]`                      | List objects in the current scope, optionally filtered       |
| `open`      | `open <source>`                    | Open a session or bus for object discovery                   |
| `close`     | `close <source>`                   | Close a source, bus, or query                                |
| `query`     | `query <name> <selection>`         | Create a named object filter                                 |
| `queries`   | `queries`                          | List all active queries                                      |
| `watch`     | `watch <object[.property]>`        | Monitor property values in the watch pane (TUI only)         |
| `unwatch`   | `unwatch <key \| all>`             | Stop watching properties (TUI only)                          |
| `watches`   | `watches`                          | List all active watches (TUI only)                           |
| `listen`    | `listen <object[.event]>`          | Subscribe to event emissions in the log pane                 |
| `unlisten`  | `unlisten <key \| all>`            | Stop listening to events                                     |
| `listeners` | `listeners`                        | List all active listeners                                    |
| `inspect`   | `inspect <object \| type>`         | Show the structure of an object or type                      |
| `types`     | `types [filter]`                   | List all registered types, optionally filtered               |
| `units`     | `units [category]`                 | List all registered units, optionally filtered by category   |
| `log`       | `log [level <lvl>] [<name> <lvl>]` | Show or set global / per-logger log verbosity                |
| `theme`     | `theme <name>`                     | Switch the color theme                                       |
| `status`    | `status`                           | Show kernel runtime status (run mode, components, transport) |
| `clear`     | `clear`                            | Clear the command output pane                                |
| `exit`      | `exit`                             | Shut down the kernel                                         |
| `shutdown`  | `shutdown`                         | Shut down the kernel                                         |

## Keyboard shortcuts

| Key                   | Context         | Action                                   |
|-----------------------|-----------------|------------------------------------------|
| **Tab**               | Input line      | Auto-complete / cycle candidates forward |
| **Shift+Tab**         | Input line      | Cycle candidates backward                |
| **Enter**             | Input line      | Submit command                           |
| **Enter**             | Completion menu | Accept selected candidate                |
| **Escape**            | Completion menu | Dismiss and restore original text        |
| **Escape**            | Form            | Cancel the form and return to input      |
| **Up / Down**         | Input line      | Navigate command history                 |
| **Left / Right**      | Input line      | Move cursor                              |
| **Home / End**        | Input line      | Jump to start / end of input             |
| **PageUp / PageDown** | Any             | Scroll the command pane                  |
| **F1**                | Any             | Show help (command list)                 |
| **F2**                | TUI             | Clear active bottom tab                  |
| **F3**                | TUI             | Toggle logs tab                          |
| **F4**                | TUI             | Toggle events tab                        |
| **F5**                | TUI             | Toggle watch pane                        |
| **Ctrl+C**            | Any             | Kill the terminal (no kernel shutdown)   |
| **Ctrl+D**            | Input line      | Graceful shutdown                        |
| **Escape**            | Input line      | Graceful shutdown                        |
| **Tab / Shift+Tab**   | Form            | Navigate to next / previous field        |
| **Space**             | Form (boolean)  | Toggle the boolean value                 |
| **Left / Right**      | Form (enum)     | Cycle through enumerator values          |
| **Up / Down**         | Form (integer)  | Increment / decrement by 1               |
| **Left / Right**      | Form (unit)     | Cycle through available units            |
| **Ctrl+N**            | Form (sequence) | Add a new element to the sequence        |
| **Ctrl+X**            | Form (sequence) | Remove the focused element               |
| **Ctrl+O**            | Form (optional) | Toggle between empty and filled          |
| **Left / Right**      | Form (variant)  | Change the active variant type           |
