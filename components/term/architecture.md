# Term Component Architecture

## Introduction

The term component is a TUI-based interactive terminal for the Sen kernel. It provides an
environment for discovering objects, invoking methods, monitoring properties, and streaming
events from a single scrolling view built on the FTXUI framework.

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
- **UI layer** (`app.*`, `app_renderers.*`, `output_pane.*`, `input_pane.*`, `banner.*`, `styles.h`) owns the
  FTXUI screen and layout. It knows nothing about Sen objects or commands.
- **Command layer** (`command_engine.*`) dispatches user input to handlers and pushes results to
  the UI via the App's public interface.
- **Data layer** (`object_store.*`, `completer.*`, `scope.*`) manages object discovery, completion
  indices, and scope navigation.
- **Form layer** (`arg_form.*`) builds type-aware guided-input forms for method arguments.
- **Rendering layer** (`value_formatter.*`, `signature_renderer.*`, `tree_view.*`) formats Sen
  values and type metadata as FTXUI elements.
- **Support** (`log_router.*`, `log_sink.*`, `output_capture.*`, `suggester.*`, `parse_utils.h`)
  handles logging, stderr capture, edit-distance suggestions, and tokenization.

## Key Design Decisions

### Layout

There is one layout: output scrolling above a rule, with the input line below it. Log lines and
event emissions arrive in that same stream rather than in panes of their own. The `tui`/`repl` mode
flag, the status bar, the watch pane and the logs/events panes have all been removed, along with the
`tuiOnly` descriptor field that gated commands per mode.

### Single-Thread Model

All state lives on the Sen component thread. The FTXUI loop is driven synchronously from the
kernel's exec loop callback. This eliminates the need for mutexes or pending queues between the
UI and the command/data layers.

### Command Table

Built-in commands are defined in a single descriptor table that combines the command name,
category, usage text, help detail, completion hint, and handler pointer. Adding a command means one
table entry plus the handler implementation. The help output, Tab completion, suggestions and
dispatch all read from that one table.

### Incremental Completion

The Completer maintains its object index incrementally via add/remove callbacks from the
ObjectStore rather than rebuilding from scratch each cycle. A full rebuild only triggers when
the navigation scope changes.

### Watch/Listener Lifecycle

Listeners follow a three-state lifecycle: pending (the object has not been discovered yet),
connected, and disconnected (the object was removed). They reconnect when the object reappears,
matched on the object's full local name recorded when the listener was created rather than on the
name the user typed, which is relative to whatever scope they were in. Emissions arrive inline with
command output. Property watches, which had the same lifecycle and their own pane, have been
removed.

### Form Field Tree

The ArgForm mirrors the Sen type tree: struct fields become named children, sequences become
indexed children, variants get a type selector plus a value subtree. Each leaf has a text buffer
and a type-specific editor. Focus navigation traverses the leaves in pre-order.

### Themes and Styles

The component ships with 10 color themes defined in `theme.h/cpp`. A `Theme` carries 25 colour
fields covering completion, value formatting, tree connectors, banners, the input line and log
levels. The ten themes are not ten tables: the fields fall into 10 distinct colours, each identical
across every theme, so each theme is a 10-colour `Palette` and one `themeFromPalette` spreads
it. The active theme is a process-wide singleton set during
component init and switchable at runtime via the `theme` command.

All rendering code reads colors through named style functions in `styles.h`, which delegate to the
active theme. This indirection means that swapping the theme instantly recolors the entire UI on
the next render tick.

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
  area, and the log router provides per-logger level control. There are no panes: results, log lines
  and event emissions share one stream.

Stderr capture is implemented with raw POSIX pipes (Linux/macOS) and the WinAPI pipe equivalents
(Windows). The term links no networking library of its own.
