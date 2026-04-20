# Term Component Architecture

## Introduction

The term component is a TUI-based interactive terminal for the Sen kernel. It provides an
environment for discovering objects, invoking methods, monitoring properties, and streaming
events within a multi-pane interface built on the FTXUI framework.

This document explains the structure and design of the component. It assumes familiarity with the
Sen kernel's object model (objects, properties, methods, events, buses, sessions).

## Directory Structure

```
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
- **UI layer** (`app.*`, `app_renderers.*`, `status_bar.*`, `banner.*`, `styles.h`) owns the
  FTXUI screen and pane layout. It knows nothing about Sen objects or commands.
- **Command layer** (`command_engine.*`) dispatches user input to handlers and pushes results to
  the UI via the App's public interface.
- **Data layer** (`object_store.*`, `completer.*`, `scope.*`) manages object discovery, completion
  indices, and scope navigation.
- **Form layer** (`arg_form.*`) builds type-aware guided-input forms for method arguments.
- **Rendering layer** (`value_formatter.*`, `signature_renderer.*`, `tree_view.*`) formats Sen
  values and type metadata as FTXUI elements.
- **Support** (`log_router.*`, `log_sink.*`, `output_capture.*`, `suggester.*`, `parse_utils.h`)
  handles logging, stdout capture, edit-distance suggestions, and tokenization.

## Key Design Decisions

### Display Modes

The component supports two display modes selectable via the `mode` configuration option:

- **TUI** (default): fullscreen multi-pane layout with status bar, watch pane, log/event panes,
  and F-key shortcuts.
- **REPL**: fullscreen single-pane layout. No status bar, watch pane, or log/event panes. Events
  print inline with command output. The `watch`, `unwatch`, and `watches` commands are disabled.

Both modes share the same FTXUI Fullscreen rendering, command engine, completer, and formatting
pipeline. The mode flag propagates through the command table (`tuiOnly` field on descriptors),
which filters commands from dispatch, help output, Tab completion, and "did you mean" suggestions.

### Single-Thread Model

All state lives on the Sen component thread. The FTXUI loop is driven synchronously from the
kernel's exec loop callback. This eliminates the need for mutexes or pending queues between the
UI and the command/data layers.

### Command Table

Built-in commands are defined in a single descriptor table that combines the command name,
category, usage text, help detail, completion hint, availability flag (`tuiOnly`), and handler
pointer. Adding a command means one table entry plus the handler implementation. The help output,
Tab completion, suggestions, and dispatch logic all read from this table and filter by mode.

### Incremental Completion

The Completer maintains its object index incrementally via add/remove callbacks from the
ObjectStore rather than rebuilding from scratch each cycle. A full rebuild only triggers when
the navigation scope changes.

### Watch/Listener Lifecycle

Watches and listeners follow a three-state lifecycle: pending (object not yet discovered),
connected (actively subscribed), and disconnected (object removed). They automatically reconnect
when an object reappears. Both systems share this lifecycle pattern but differ in output
destination: watches update the persistent watch pane, listeners stream to the events tab of the
bottom pane (which also hosts the logs tab).

### Form Field Tree

The ArgForm mirrors the Sen type tree: struct fields become named children, sequences become
indexed children, variants get a type selector plus a value subtree. Each leaf has a text buffer
and a type-specific editor. Focus navigation traverses the leaves in pre-order.

### Themes and Styles

The component ships with 10 color themes defined in `theme.h/cpp`. Each theme is a `Theme` struct
with ~35 color fields covering all UI areas: panes, status bar, completion, value formatting, tree
connectors, banners, and log levels. The active theme is a process-wide singleton set during
component init and switchable at runtime via the `theme` command.

All rendering code reads colors through named style functions in `styles.h`, which delegate to the
active theme. This indirection means that swapping the theme instantly recolors the entire UI on
the next render tick.

The theme enum (`ThemeStyle`) is defined in the STL, so the same names are used for configuration,
runtime selection, and Tab completion. The `theme` command and the completer both resolve names
through the Sen meta-type system (`MetaTypeTrait<ThemeStyle>::meta()`).

### Stdout Limitation

FTXUI uses stdout (fd 1) for terminal rendering, including escape sequences for cursor
positioning, color, and terminal size queries. Stderr is captured via a pipe and routed to the
log pane, but stdout cannot be redirected without breaking FTXUI's rendering. Components running
alongside the term must use spdlog for diagnostic output instead of printf or std::cout.

## External Dependencies

- **FTXUI** (v6.1.9): TUI framework. Element-based declarative rendering, component/event system,
  and terminal management.
- **spdlog**: Logging. A custom sink routes log messages to the TUI log pane. The log router
  provides per-logger level control.
- **Asio**: Used for hostname retrieval (`asio::ip::host_name()`). Stderr capture is implemented
  with raw POSIX pipes (Linux/macOS) and the WinAPI pipe equivalents (Windows), not Asio.
