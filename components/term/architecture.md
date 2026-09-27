# Term Component Architecture

## Introduction

The term component is a TUI-based interactive terminal for the Sen kernel. It provides an environment
for discovering objects, invoking methods, reading properties, and streaming events from a single
scrolling view built on the FTXUI framework. A property can be read on demand; nothing follows one over
time.

This document explains the structure and design of the component. It assumes familiarity with the
Sen kernel's object model (objects, properties, methods, events, buses, sessions).

## Directory Structure

```text
[components/term]
├── src/                        # Source code
├── stl/                        # STL type definitions (Configuration, enums)
├── test/
│   ├── unit/                   # Unit tests
│   └── integration/            # Config files and test packages
├── architecture.md             # This document
```

User documentation: `docs/components/term.md`.

## Overview

The component follows a layered design:

- **Component layer** (`component.cpp`) creates and wires all objects, then enters the exec loop.
- **UI layer** (`app.*`, `app_renderers.*`, `output_pane.*`, `input_pane.*`, `banner.*`, `styles.h`) owns
  the FTXUI screen, the layout and the key bindings. It does not dispatch commands: it hands a submitted
  line to a callback. It does reach Sen's type metadata, because the guided-input form lives in the UI
  state and its editors are chosen per type.
- **Command layer** (`command_engine.*`) dispatches user input to handlers and pushes results to
  the UI via the App's public interface.
- **Data layer** (`object_store.*`, `completer.*`, `scope.*`) manages object discovery, completion
  indices, and scope navigation.
- **Form layer** (`arg_form.*`) builds type-aware guided-input forms for method arguments.
- **Rendering layer** (`value_formatter.*`, `signature_renderer.*`, `tree_view.*`, `text_table.*`,
  `text_wrap.h`, `byte_format.h`, `unicode.h`) formats Sen values and type metadata as FTXUI elements.
- **Support** (`log_router.*`, `log_sink.*`, `output_capture.*`, `clipboard.*`, `signal_stack.*`,
  `suggester.*`, `parse_utils.h`, `type_peel.h`, `util.*`) handles logging, stderr capture, the system
  clipboard, the alternate signal stack the crash handler needs, edit-distance suggestions, tokenization
  and alias peeling.

## Key Design Decisions

### Layout

There is one layout: output scrolling above a rule, with the input line below it. Log lines and
event emissions arrive in that same stream rather than in panes of their own. There is no status bar and
no second mode, so every command is available wherever the user is.

### Single-Thread Model

Everything the UI, command and data layers hold lives on the Sen component thread, and the FTXUI loop is
driven synchronously from the kernel's exec loop callback, so nothing between those layers needs a mutex
or a pending queue.

What does run on other threads owns a mutex and a bounded queue that the component thread drains: the
log router takes lines from whichever thread logged, the stderr capture has a reader thread, and the
clipboard has one worker so a helper process cannot stall a frame.

### Command Table

Built-in commands are defined in a single descriptor table that combines the command name,
category, usage text, help detail, completion hint, and handler pointer. Adding a command means one
table entry plus the handler implementation. The help output, Tab completion, suggestions and
dispatch all read from that one table.

### Incremental Completion

The Completer maintains its object index incrementally via add/remove callbacks from the
ObjectStore rather than rebuilding from scratch each cycle. A full rebuild only triggers when
the navigation scope changes.

### Listener Lifecycle

Listeners follow a three-state lifecycle: pending (the object has not been discovered yet),
connected, and disconnected (the object was removed). They reconnect when the object reappears,
matched on the object's full local name recorded when the listener was created rather than on the
name the user typed, which is relative to whatever scope they were in. Emissions arrive inline with
command output. Events are the only thing a listener follows; there is no property watch.

### Form Field Tree

The ArgForm mirrors the Sen type tree: struct fields become named children, sequences become
indexed children, variants get a type selector plus a value subtree. Each leaf has a text buffer
and a type-specific editor. Focus navigation traverses the leaves in pre-order.

### Themes and Styles

The component ships with 10 color themes defined in `theme.h/cpp`. A `Theme` carries 25 color fields
covering completion, value formatting, tree connectors, banners, the input line and log levels. Those 25
fields take their values from a `Palette` of 10, because within any one theme the fields fall into 10
groups that always share a color. So a theme is written as 10 colors and `themeFromPalette` spreads them
over the 25 fields. The active theme is a process-wide singleton, set during component init and
switchable at runtime with the `theme` command.

Rendering reads colors through the named style functions in `styles.h`, which delegate to the active
theme, so switching the theme recolors the whole screen on the next render tick. The banner is the one
exception: it reads the active theme directly to build its color bar.

The theme enum (`ThemeStyle`) is defined in the STL, so the same names are used for configuration,
runtime selection, and Tab completion. The `theme` command and the completer both resolve names
through the Sen meta-type system (`MetaTypeTrait<ThemeStyle>::meta()`).

### Stdout Limitation

FTXUI uses stdout (fd 1) for terminal rendering, including escape sequences for cursor
positioning, color, and terminal size queries. Stderr is captured via a pipe and drawn in the output
area, but stdout cannot be redirected without breaking FTXUI's rendering. Components running
alongside the term must use spdlog for diagnostic output instead of printf or std::cout.

## External Dependencies

- **FTXUI** (v7.0.3): TUI framework. Element-based declarative rendering, component/event system,
  and terminal management.
- **spdlog**: Logging. A custom sink registered with the kernel routes log messages into the output
  area, and the log router provides per-logger level control. Results, log lines and event emissions
  share one stream.

Stderr capture uses raw pipes: `pipe`, `dup2` and `read` on Linux and macOS, and the CRT's `_pipe`,
`_dup2` and `_read` on Windows. The term links no networking library of its own.

### The Crash Handler and the Signal Stack

FTXUI installs its own handlers for the fatal signals when it takes the terminal, with a plain
`sigaction` and no `SA_ONSTACK`. Its handler restores the terminal and re-raises into the handler it
displaced, so a crash still reaches the kernel's crash reporter, but the missing flag undoes the
alternate signal stack the kernel gives every thread it starts. Without that stack a stack overflow
faults again inside the handler and the process dies with no dump. `signal_stack.*` puts the flag back
after the terminal is installed, changing nothing else about the handlers.

Term also hands the kernel the descriptor it saved for stderr, so the crash banner goes there rather
than into the pipe term captured, and the two threads it starts itself, the stderr reader and the
clipboard worker, ask the kernel for that signal stack when they start, because the kernel did not start
them.
