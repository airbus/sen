# Using component groups

## Seeing the group order in the log

Let's assume we have a handwritten component with the following `run` method:

```c++ title="component.cpp"
sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
{
  std::cout << "MyComponent: running\n";
  auto result = api.execLoop(sen::Duration::fromHertz(1.0));
  std::cout << "MyComponent: finished running\n";
  return result;
}
```

And modify your configuration file to enable kernel logs:

```yaml title="config.yaml"
# you can configure the kernel that will
# host your components. Here we only show
# how to set the global log level for the kernel.
kernel:
  logConfig:
    level: debug

load:
  - name: shell
    group: 1

  - name: my_component
    group: 2
```

```hl_lines="22 26"
➜ ./sen run config.yaml
[2023-03-07 10:02:21.866] [sen.kernel] [debug] advancing to group 1
[2023-03-07 10:02:21.866] [sen.kernel] [debug] loading shell
[2023-03-07 10:02:21.867] [sen.kernel] [debug] loading kernel
[2023-03-07 10:02:21.867] [sen.kernel] [debug] initializing shell
    _________  __
   / __/ __/ |/ /          compiler GNU-11.3.0 [mode: debug]
  _\ \/ __/    /           revision 5c937e6e5d344315b8c07c0af7e14cb682a837dc
 /___/___/_/|_/   0.0.1    branch   refs/heads/master [modified]
▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄

[2023-03-07 10:02:21.867] [sen.kernel] [debug] initializing kernel
[2023-03-07 10:02:21.867] [sen.kernel] [debug] running shell
[2023-03-07 10:02:21.867] [sen.kernel] [debug] running kernel
[2023-03-07 10:02:21.867] [sen.kernel] [debug] reached group 1
[2023-03-07 10:02:21.867] [sen.kernel] [debug] advancing to group 2
[2023-03-07 10:02:21.867] [sen.kernel] [debug] loading my_component
[2023-03-07 10:02:21.867] [sen.kernel] [debug] initializing my_component
[2023-03-07 10:02:21.867] [sen.kernel] [debug] running my_component
[2023-03-07 10:02:21.867] [sen.kernel] [debug] reached group 2
[2023-03-07 10:02:21.867] [sen.kernel] [debug] running
MyComponent: running
sen:host/config> shutdown
shutting down...
[2023-03-07 10:03:26.618] [sen.kernel] [debug] stopping my_component
MyComponent: finished running
[2023-03-07 10:03:26.868] [sen.kernel] [debug] unloading my_component
[2023-03-07 10:03:26.868] [sen.kernel] [debug] reached group 1
[2023-03-07 10:03:26.868] [sen.kernel] [debug] stopping shell
[2023-03-07 10:03:26.889] [sen.kernel] [debug] stopping kernel
[2023-03-07 10:03:26.903] [sen.kernel] [debug] unloading shell
[2023-03-07 10:03:26.903] [sen.kernel] [debug] unloading kernel
[2023-03-07 10:03:26.903] [sen.kernel] [debug] reached group 0
bye ☺
```

You can see the progression of the initialization logic:

1. The kernel starts at group 0, and it advances to group 1.
2. In group 1, the kernel finds the *shell* and *kernel* components, so it: (1) loads them, (2)
   initializes them (this makes the shell print the banner), and (3) runs them.
3. In group 2, the kernel finds *my_component*, so it (1) loads it, (2) initializes it and (3) runs
   it (this is what prints the message).
4. The kernel does not find any other group, so it reaches the "running" state.

At this point we command the kernel to shut-down (using the *shell*), so the kernel does the same
process, but in reverse.

1. In group 2, the kernel finds *my_component*, so it stops it and then unloads it.
2. In group 1, the kernel finds the *shell* and *kernel* components, so it stops them and then
   unloads them.
3. The kernel does not find any other group, so it reaches the "stopped" state, and finishes the
   execution.

You see that during start-up the kernel proceeds by groups, and within each group it makes three
passes over the components: it loads all of them, then initializes all of them, then runs all of
them. During shut-down it works through the groups in reverse, stopping all the components in a
group before unloading any of them.

Group numbers are used as written: the kernel counts up from 1 and skips any number nothing is
assigned to, so leaving gaps costs nothing and only the relative order matters.
