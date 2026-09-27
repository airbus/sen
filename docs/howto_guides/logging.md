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

!!! warning "A terminal component silences the console sinks"

    The configuration above loads *term*, and *term* draws the log lines itself in its own output area.
    To do that it takes the terminal over, and a console sink -- `Stdout`, `ColorStdout` or `Stderr` --
    writes to the same descriptor *term* is drawing on: keeping both would print every line twice, on
    top of the display. So while *term* is running, every console sink is silenced.

    In the example above that means `stdout_sink` produces nothing and `file_sink` keeps working. The
    file is where to look. Console sinks come back when *term* exits.

    The same applies to any component that claims the terminal. It does not affect a file sink, a
    network sink, or the crash report's log ring.

## Rendering the logs in a component

A component that displays log lines itself registers a sink with the kernel rather than walking
spdlog's registry. Walking it does not work and is not safe: each shared library has its own registry,
so a component's `spdlog::apply_all` never sees the kernel's loggers, and appending to a logger another
thread is emitting through is a use-after-free -- spdlog iterates a logger's sink vector without a lock.

The kernel puts one relay sink on every logger, in the only window where that is safe, and a component
registers behind the relay:

```cpp
#include <sen/kernel/component_api.h>

auto sink = std::make_shared<MySink>();

// `owned` says this component is drawing on the terminal, so console sinks are silenced while it runs.
// Use the default, `shared`, for a sink that is not a display -- a telemetry or audit sink.
auto registration = kernel::KernelApi::addLoggerSink(sink, kernel::KernelApi::TerminalOwnership::owned);
if (registration.isError())
{
    // Nothing is rendering logs. The console sinks are still live, so say so there.
}
else if (registration.getValue().terminalOwnedElsewhere)
{
    // Another component is already drawing logs. Registration is not arbitrated: both will draw.
}
```

The sink keeps its own pattern -- the relay passes the message on unformatted -- and it receives lines
from loggers created after it registered, which is what a registry walk could not do.

**Unregister before the state the sink writes into goes away.** A registered sink is held by the kernel
until it is removed, and it can be running on another thread at that moment, so stop it reaching your
state first and then remove it:

```cpp
sink->detach();                                  // your own: make the sink inert under its own lock
std::ignore = kernel::KernelApi::removeLoggerSink(sink);
```

`KernelApi::setAllLoggersLevel` sets the level on every logger and on every logger made afterwards,
which is what a user changing the level at runtime needs; `getAllLoggersLevel` reads it back. It
replaces the per-logger levels the configuration file asked for, with no way back to them.

## Getting a logger

To get the logger, we need to fetch it from the `spdlog` registry. To make it easy, we made the
registration of the logger accessible automatically via the `sen::kernel::KernelApi` using the
static method called `getOrCreateLogger`:

```c++ title="getLogger function" linenums="1"
#include <sen/kernel/component_api.h>

[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger()
{
  return sen::kernel::KernelApi::getOrCreateLogger("my_logger");
}
```

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
