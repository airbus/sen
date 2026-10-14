# Logging

Sen offers helpers for those using the [spdlog](https://github.com/gabime/spdlog) library.

We now add logging to a component so that:

- the logs are written to the standard output and to file,
- the standard output shows more detailed logs than the file, and
- the logs include the filepath and line number from where the log was triggered.

## Configuring the sinks

The configuration file specifies the sinks, their log level, and other properties. The file
`libs/kernel/stl/sen/kernel/log.stl` lists available sinks and their properties. In this example,
the logger that uses these two sinks is named `my_logger` and modifies the log's output pattern, as
described in `spdlog`'s [documentation](https://github.com/gabime/spdlog/wiki/3.-Custom-formatting).

```yaml title="Example configuration"
kernel:
  logConfig:
    backtrace: true
    sinks:
      - name: "stdout_sink"
        singleThreaded: true
        level: trace
        config:
          type: Stdout
          value: {}
      - name: "file_sink"
        singleThreaded: true
        level: debug
        config:
          type: BasicFile
          value:
            fileName: logs/my_log.txt
            truncate: false
            createParentDir: true
    loggers:
      - name: "my_logger"
        sinks: ["stdout_sink", "file_sink"]
        pattern: "[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%g:%#] [%l] %v"
        level: trace  # has priority if more restrictive than the sinks' levels

load:
  - name: term
    group: 2
    open: [my.tutorial]  # to see the created objects

build:
  - name: myComponent
    group: 3
    freqHz: 30
    imports: [my_package]
    objects:
      - class: my_package.MyClassImpl
        name: myClass
        bus: my.tutorial
        prop1: some value
```

!!! note "A terminal component shows your console output in its own pane"

    The configuration above loads *term*, which draws in the terminal and would otherwise paint over
    anything else written there. Instead of silencing console output, *term* captures the standard
    output and error descriptors and draws what arrives on them in its own output area.

    So a console sink keeps working and its lines appear in *term*'s pane, colours included, rather
    than flashing on screen and being overwritten by the next frame. This holds for anything that
    reaches those descriptors, not only spdlog: `printf` and `std::cout` arrive the same way, from a
    logger the kernel has never heard of as readily as from one it made.

    A file sink, a network sink and the crash report's log ring are unaffected. A logger configured
    with only a file sink writes nothing to a descriptor, so nothing appears in the pane — which is
    what asking for a file and not the console means.

## Rendering the logs in a component

A component that draws in the terminal captures the standard output and error descriptors rather
than registering anything with the kernel's logging. `term` does this in `OutputCapture`: it dups
each descriptor, puts a pipe in its place, and draws what it reads in its own pane.

Capturing is what makes the display complete. A sink reaches spdlog and nothing else: it misses
`printf` and `std::cout`, which reach the descriptor all the same and would be painted over.
Capturing the descriptor sees all of them and needs nothing registered, declared or configured by
whoever wrote the output.

Two things the component has to arrange:

- **Keep a way back to the real terminal.** Once the descriptor is a pipe, the component's own
  rendering would be captured along with everything else. `term` dups the original first and sends
  its renderer's writes there, telling the two apart by the writing thread.
- **Tell the UI library the terminal size.** ftxui reads it with `ioctl(STDOUT_FILENO, TIOCGWINSZ)`,
  a descriptor number rather than the terminal, so with a pipe there it silently falls back to
  80x24. `term` reads the size from the saved descriptor and passes it to `Terminal::SetFallbackSize`
  on every drain, so a resize is not missed.

Captured lines arrive formatted by whoever wrote them, escapes and all. `term` translates the SGR
sequences into its UI library's own styling, so a console sink's colours survive — which matters
because the colour is what still says a warning is a warning once nothing carries the level as a
value.

`KernelApi::setAllLoggersLevel` sets the level on every logger and on every logger made afterwards,
which is what a user changing the level at runtime needs; `getAllLoggersLevel` reads it back. It
replaces the per-logger levels the configuration file asked for, with no way back to them. Levels
are not something capturing can change from the outside, which is why they stay in the API.

## Getting a logger

To get the logger, we need to fetch it from the `spdlog` registry. To make it easy, we made the
registration of the logger accessible automatically via the `sen::kernel::KernelApi` using the
static method called `getOrCreateLogger`:

```c++ title="getLogger function" linenums="1"
#include <sen/kernel/component_api.h>

#include <spdlog/spdlog.h>

[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger()
{
  return sen::kernel::KernelApi::getOrCreateLogger("my_logger");
}
```

`component_api.h` declares spdlog's logger instead of including spdlog's headers, so a component
that never logs compiles neither spdlog nor fmt. The declaration is enough to hold a logger and pass
it around; calling a method on one needs the definition, so include `<spdlog/spdlog.h>` where you
log. `add_sen_package` links spdlog, so your `CMakeLists.txt` needs nothing.

On MSVC with warnings as errors, add `/wd4251 /wd4275` where you compile spdlog's headers: spdlog's
`logger` is a dll-interface class with `std::string` and `std::vector` members, and its conan recipe
does not carry the suppressions spdlog's own CMake sets when it builds shared.

## Where spdlog has to be at run time

Sen links spdlog as a shared library, so it has to be reachable wherever a binary that uses it runs.
Conan's run environment covers that: activate `conanrun` and spdlog's directory is on the library
path, on Linux, macOS and Windows alike.

What it does not cover is a binary started without it. On Windows an executable of your own will not
start, because Windows looks for a DLL beside the executable or on PATH and has no rpath. A container
or another machine that does not hold your conan cache cannot use the absolute paths the run
environment exports. An artefact you ship has to carry spdlog with it.

The answer in each case is to put spdlog's library beside your binaries, as Sen's own install does:
`<prefix>/lib` on Linux and macOS, `<prefix>/bin` on Windows.

## Compile-time log levels

You will normally use the syntax `logger->info(..)` (or `trace` etc.), but be aware that this won't
output the file and line number. This is only included when using macros such as
`SPDLOG_LOGGER_DEBUG()`. For those to work, `SPDLOG_ACTIVE_LEVEL` needs to be defined before
including `spdlog` headers.

```c++
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>
```

`spdlog.h` is where the macros live. You do not need a sink header: the sinks come from the
configuration above, not from your code.

You can now use your logger in the component's `run()` function:

```c++ title="run function" linenums="1"
sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
{
  const auto logger = getLogger();

  logger->info("MyComponent started running; this log does not show the file and line");
  logger->trace("By default, this goes to stdout. Set file_sink to 'trace' to also see it there");

  auto func = [&]() { SPDLOG_LOGGER_DEBUG(logger, "MyComponent iteration; this shows the file and line"); };
  return api.execLoop(sen::Duration::fromHertz(1.0), func);
}
```
