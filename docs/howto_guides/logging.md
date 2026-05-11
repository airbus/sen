# Logging

Sen offers helpers for those using the [spdlog](https://github.com/gabime/spdlog) library.

We now extend the component of our previous example so that:

- the logs are written to the standard output and to file,
- the standard output shows more detailed logs than the file, and
- the logs include the filepath and line number from where the log was triggered.

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
          type: Stderr
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
  - name: shell
    group: 2
    open: [my.tutorial]  # to see the created objects

build:
  - name: myComponent
    freqHz: 30
    imports: [my_package]
    objects:
      - class: my_package.MyClassImpl
        name: myClass
        bus: my.tutorial
        prop1: some value
```

To get the logger, we need to fetch it from the `spdlog` registry. To make it easy, we made the
registration of the logger accessible automatically via the `sen::kernel::KernelApi` using the
static method called `getOrCreateLogger`:

```c++ title="getLogger function" linenums="1"
[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger()
{
  return sen::kernel::KernelApi::getOrCreateLogger("my_logger");
}
```

We can now use our logger in the component's `run()` function. We can flush the logger's buffer to
ensure that the logs are written to file immediately.

You will normally use the syntax `logger->info(..)` (or `trace` etc.), but be aware that this won't
output the file and line number. This is only included when using macros such as
`SPDLOG_LOGGER_DEBUG()`. For those to work, `SPDLOG_ACTIVE_LEVEL` needs to be defined before
including `spdlog` headers.

```c++
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
```

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
