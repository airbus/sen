# The Tracy component

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/docs-assets/tracy.webp){: style="width:1200px;"}

This [component](../users_guide/glossary.md#component) connects the
[Tracy](https://github.com/wolfpld/tracy) profiler to the Sen tracing API.

## Getting it started

In your Sen process, load the `tracy` component:

```yaml
load:
  - name: tracy
    group: 1
```

When you start your Sen process Tracy will be running in the background, waiting for the connection
with the Tracy front-end.

Then, start the Tracy front-end. You should be able to find your process and start the profiling
session.

If you want to have the *full overview* of what your process is doing, you will need to run it with
administrator privileges. This is to allow Tracy access to the kernel monitoring mechanisms.

## Network settings

To customize Tracy's networking behavior, you can use the following environment variables:

| Variable               | Description                                                  |
| ---------------------- | ------------------------------------------------------------ |
| `TRACY_ONLY_LOCALHOST` | Set to 1 to restrict the Tracy traffic to the local computer |
| `TRACY_ONLY_IPV4`      | Set to 1 to restrict the usage of Tracy to IPv4              |
| `TRACY_PORT`           | The port that Tracy should use for communication             |

To enable network communication, Tracy needs to open a listening port. Make sure it is not blocked
by an overzealous firewall or antivirus program.

## Displayed information

Apart from the profile information published by you, Sen will emit the following information:

- A message will be posted every time a component's thread misses a frame due to over-sleeps (OS
  scheduling issues).
- A message will be posted every time a component's execution time is exceeded (overrun).
- The size of the component's queues, plotted as `<component> work queue size` and
  `<component> event queue size`.
- The number of nanoseconds that components oversleep, plotted as `<component> oversleep (ns)`.

## Further information

At this point, it's more about how to use Tracy. You can get a lot of information in the
[provided documentation](https://github.com/wolfpld/tracy).
