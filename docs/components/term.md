# The Sen Terminal (BETA)

[Term](https://raw.githubusercontent.com/airbus/sen/refs/heads/fix/images/term.jpg){: style="width:700px"}

The Sen Terminal (`term`) lets you open buses, find the objects on them, call their methods, watch
their events and inspect their types, from one prompt. It is the successor to the
[shell](shell.md) and the two share the same workflow.

Term is in beta. If something here does not match what you see, the code is right and this page is
wrong — please say so.

## Getting it started

Load it into any kernel configuration:

```yaml
load:
  - name: term
    group: 2
    open:
      - local.main
```

Or run it on its own:

```shell
sen term
sen run my_config.yaml
```

On startup you get a banner with the Sen version, the compiler, the git branch and a quote, then a
prompt. `exit`, `shutdown`, Ctrl+D on an empty line, or Escape twice all ask the kernel to stop every
component gracefully.

## The screen

Output scrolls above a horizontal rule; the prompt sits below it. There are no panes: command
results, log lines and event emissions all arrive in the same stream, in the order they happened.

The prompt shows your current scope, for instance `sen:/local.main❯`.

## Typing and editing

| Key                     | Does                                                           |
|-------------------------|----------------------------------------------------------------|
| Left / Right            | Move one character                                             |
| Ctrl+Left / Ctrl+Right  | Move one word. A dotted path is several words                  |
| Home / End              | Start or end of the line                                       |
| Backspace / Delete      | Erase one character either side of the cursor                  |
| Ctrl+W or Ctrl+Backspace| Erase the word before the cursor                                |
| Up / Down               | Walk the command history                                       |
| Tab / Shift+Tab         | Completion, forwards and backwards                             |
| Escape                  | Clear the line; on an empty line, arm exit; Escape again exits |
| Ctrl+D                  | Exit, from an empty line only                                   |
| Ctrl+Y                  | Copy the current selection to the clipboard                    |

Movement and erasing work in characters, not bytes, so accented letters, CJK text and emoji behave
the way you expect. History is kept across sessions in a file, capped at 2000 lines.

## What the line accepts

The command line holds 8192 bytes, and each field of a guided-input form the same. Past that,
characters are refused and the term says so once; Escape clears the line. A line that was cut is not
submitted — pressing Enter says so instead, because sending it would report a parse failure about text
you never finished.

A pasted line is typed, not parsed: the terminal protocol gives no way to tell a paste from someone
typing very fast, so every character in it is read as a keystroke. Two consequences worth knowing.
A newline in pasted text submits the line, so pasting three lines runs three commands. And a control
character in it does whatever that key does — a stray `Tab` opens completion, `Escape` clears the
line. Ctrl+D is the exception: it exits only from an empty line, so it cannot end the session from
inside a paste.

A line long enough to wrap shows the prompt's row, the rows around the cursor, and a note saying how
many rows are not shown.

## Tab completion

Press **Tab**. One match is inserted; several open a grid above the prompt. Tab again cycles
forward, Shift+Tab backward, Enter accepts and runs, `.` accepts and lets you keep typing a deeper
path, Escape dismisses and restores what you had typed.

When only one match exists at an intermediate level, Tab inserts it and moves on — so `l`, Tab can
take you to `local.main.` in two keystrokes when there is only one session and one bus.

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
| `listen` / `unlisten` argument | Object paths, object.event paths, `all`                   |
| `log` argument                 | `level`, log level names, logger names                    |
| `theme` argument               | Available theme names                                     |
| `help` argument                | Command names                                             |
| `inspect` argument             | Object paths and type names                               |
| `types` argument               | Registered type names                                     |
| `units` argument               | Unit category names                                       |

## Navigation and scoping

Scoping narrows the view, which shortens what you have to type and focuses `ls`.

```text
/ > cd local.main
/local.main > ls
  showcase  [term_showcase.ShowcaseImpl, Native]
/local.main > cd ..
/ >
```

`cd ..` goes up one level, `cd -` returns to the previous scope, `cd /` to the root, and `cd @name`
enters a query.

Nested object groups are separated with `/`, the same way the rest of the path is, so whatever the
prompt shows is a target you can type back — leading separator included:
`cd /local.main/sensors/indoor` and `cd sensors` then `cd indoor` reach the same place. A target
containing a dot is always read as `session.bus`, wherever you are, so `cd a.b` at a bus scope goes to
bus `b` of session `a` rather than into a group.

`ls` takes an optional path or `@query`, and `pwd` prints the current scope.

## Opening and closing buses

```text
/ > open local.main
  + source local.main opened
  + 3 objects detected
/ > close local.main
```

Sources listed under `open` in the configuration are opened at startup. A `query` whose `FROM`
clause names an unopened bus opens it for you.

## Calling methods

Type the object path, a dot, and the method name.

```text
/local.main > showcase.ping
```

A method with no arguments runs immediately. The line shows a spinner with an elapsed-time counter
while the call is out, then a green check and the return value, or a red cross and the error. Calls
are asynchronous and several can be in flight at once. A call that gets no answer within
`callTimeout` is reported as timed out rather than spinning forever.

### Arguments on the command line

```text
/local.main > showcase.setFraction 0.75
/local.main > showcase.moveTo {"x": 10, "y": 20}
```

Simple values go straight after the method name; structures take JSON. Wrong arguments get an error
and the method's signature.

### The guided-input form

Press Enter on a method that needs arguments and a form opens under the prompt, one field per
argument, with an editor chosen for the type. Tab and Shift+Tab move between fields.

| Type                         | Editor               | Keys                                                                      |
|------------------------------|----------------------|---------------------------------------------------------------------------|
| **bool**                     | Toggle checkbox      | Space to toggle                                                           |
| **integer** (i32, u64, etc.) | Text field with spin | Up/Down to increment/decrement, clamped to type range                     |
| **float** (f32, f64)         | Text field           | Type decimal numbers                                                      |
| **string**                   | Text field           | Free-form text input                                                      |
| **enum**                     | Cycle selector       | Left/Right arrows to cycle through enumerators; description shown in hint |
| **Duration**                 | Text field           | Type with unit suffix (e.g., `100ms`, `1.5s`, `2m`)                       |
| **Timestamp**                | Text field           | Type as ISO 8601 string                                                   |

| Type         | Editor                              | Keys                                                               |
|--------------|-------------------------------------|--------------------------------------------------------------------|
| **struct**   | Named sub-fields in a tree          | Tab into each field                                                |
| **sequence** | Indexed elements, dynamically sized | **Ctrl+N** to add element, **Ctrl+X** to remove focused element    |
| **variant**  | Type selector + value editor        | Left/Right to change the active type; value editor adapts          |
| **optional** | Presence toggle + value editor      | **Ctrl+O** to toggle between empty and filled                      |
| **quantity** | Value field + unit selector         | Edit the numeric value; Left/Right on the unit row to change units |

A bounded sequence shows a count and refuses Ctrl+N at capacity. A fixed-size array starts full and
refuses both add and remove.

## Listening to events

```text
/local.main > listen showcase.thresholdCrossed
  Listening to 'showcase.thresholdCrossed'.
```

Naming an object without an event listens to all of its events. Emissions arrive inline, as a
two-line entry: a bullet, `session.bus.object → eventName` with the time on the right, then the
arguments indented beneath.

Listeners survive their object disappearing and reconnect when it returns, matched on the object's
full name rather than on whatever scope you were in. `unlisten <object>` drops the listeners on one
object, `unlisten all` drops every one, and `listeners` lists them.

Listeners named under `listen` in the configuration are registered at startup.

## Creating queries

A query is a named filter over objects, in Sen's selection language.

```text
/ > query workers SELECT term_showcase.Worker FROM local.main
  + query 'workers' created
/ > cd @workers
@workers > ls
  worker_0
  worker_1
```

`SELECT <Type> FROM <session>.<bus>` matches a type, `SELECT *` matches everything, and `WHERE`
filters on properties. `queries` lists them; `close <session>.<bus>.<name>` removes one.

## Inspecting types and objects

```text
/ > inspect local.main.showcase
/ > inspect term_showcase.Point
/ > types
/ > types Worker
```

Inspecting an object shows its class and inheritance chain, its description, then its properties and
methods with types and descriptions, inherited members included. Inspecting a type shows a struct's
fields, an enum's values and keys, a sequence's element type and bounds, or a variant's
alternatives.

Pressing Enter on a method that needs arguments prints its signature:

```text
  METHOD showcase.moveTo
    Move the object to a new position.

  ARGUMENTS
    target : Point   the destination coordinates

  RETURNS void
```

`units` lists the unit categories and the units in each, and takes a filter.

## Logging

```text
/ > log                              # every logger and its level
/ > log level info                   # the global level
/ > log level myComponent debug      # one logger
```

Levels are `trace`, `debug`, `info`, `warn`, `error`, `critical` and `off`. Log lines arrive in the
same stream as everything else, carrying their timestamp, logger name and level.

## Logging from other components

Term captures the process's standard error and shows it. It cannot capture standard output, because
that is what FTXUI draws the interface on — a `printf`, a `std::cout` or a `write(STDOUT_FILENO, …)`
from any component will land on top of the display.

Use spdlog, obtained through the kernel:

```cpp
// in your component
getLogger()->info("my message: {}", value);
```

Take the logger from `KernelApi::getOrCreateLogger("<component>")` rather than calling `spdlog::`
free functions, and walk loggers with `KernelApi::applyToAllLoggers`. A component's own `spdlog::`
calls reach a different logger registry than the kernel's, so a logger created that way is invisible
to `log` and cannot be levelled.

## Themes

```text
/ > theme nord
  Theme changed to 'nord'.
```

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

Set one in the configuration with `theme: nord`, or in the environment with `SEN_TERM_THEME=nord`.
Precedence runs default (`catppuccinMocha`), then configuration, then the environment variable.

## Kernel status

```text
/ > status
  Run mode  realTime

  Components
  Name          Group  Cycle time  Objects
  term              2    30.0 Hz        0  realtime
  my_package        1   100.0 Hz       12  realtime
  showcase          1       N/A         5
```

With a transport active, the output also carries cumulative UDP and TCP bytes sent and received.

## Mouse

Scroll the output with the wheel. Drag across text to select it, and the selection is copied to the
clipboard when you release, with a note saying how much was copied; Ctrl+Y copies the current
selection on demand. Copying uses the OSC 52 terminal escape, so it works over ssh, and also hands
the text to `pbcopy`, `wl-copy` or `xclip` when one is available locally.

## Commands

Type `help` for the full list with usage and descriptions, or `help <command>` for one of them. The
prompt's own list is generated from the command table, so it cannot drift from what the build
actually offers.

## Configuration reference

--8<-- "components/term/stl/term.stl:config"
