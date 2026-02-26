# The Log Master

This component allows you to control your loggers.

```yaml
load:  
  - name: logmaster
    group: 4
```

This will automatically:

- Create one instance of a LogMaster object in the "local.log" bus.
- Create one instance of a Logger object per detected spdlog logger.

The LogMaster instance allows you to control all the loggers at the same time. You can also
individually control each logger. The interface of these classes is:

```rust title="Log Master interface"
--8<-- "snippets/logmaster.stl"
```

You can set your custom target bus by specifying a `targetBus` value. This would allow you to expose
these loggers over the network when using a data transport service (such as the ether component).
