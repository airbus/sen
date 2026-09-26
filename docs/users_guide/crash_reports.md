# Crash reports

A crash report is what Sen leaves behind when one of your processes stops unexpectedly, so that you
do not have to reproduce the failure to find out what happened. Under `sen run` it is on by default
and there is nothing to switch on or link against.

An uncaught exception reaches Sen somewhere it is still safe to allocate, so Sen writes a report
itself, as a JSON file, and prints the path to standard error. A fault -- an invalid access, a
division by zero, an illegal instruction -- never reaches Sen's own code at all.

Both routes write a minidump of the crashing process from the outside. Arming crash reporting does
not change how a process dies.

## Try it

`examples/packages/crash_demo` is an object that fails on purpose, so you can produce a crash and
open what it leaves behind. It is built with the rest of the examples, and run
from the directory holding Sen's binaries, which is where the kernel finds the package:

```console
$ ./sen run <sen>/examples/packages/crash_demo/config.yaml
[kernel] [debug] a fault will be dumped under crash-files/pending
Segmentation fault (core dumped)

$ ls crash-files/pending
17b06ad8-7534-423d-bc11-11450f7c38af.dmp   17b06ad8-7534-423d-bc11-11450f7c38af.meta
```

It runs for a few cycles and then writes through a null pointer. The process dies of the fault, the
dump is written from outside it, and the shell reports a core as well. `cyclesBeforeFailing` decides
how long it runs first; set it to `0` to fail on the first cycle.

Set `throwInstead: true` in the configuration and it fails the other way, by letting an exception
escape. That route leaves a report as well as a dump, and says where it put it:

```console
$ ./sen run <sen>/examples/packages/crash_demo/config.yaml
Crash report written to .../crash-files/throw_2026_09_25_07_31_02_0xa03fb0af51e222b8.json
Aborted (core dumped)
```

The exit status says which route the process took: `139` for the fault, which is the shell's way
of writing "killed by signal 11", and `134` for the abort.

## Where the files are

Both land under `crashReportDir`, or under the system temporary directory when you have not set one.

A minidump is `pending/<identifier>.dmp`, beside a small `.meta` file of the same name. The
`settings.dat` in the directory above belongs to the crash handler and is not a report.

The JSON report from an uncaught exception is written directly in `crashReportDir`, named after your
application and the time, and its path is printed when it is written:

```text
Crash report written to /var/crash/sen/flight_2026_09_25_00_23_50_0x4c9e.json
```

## Reading a minidump

A minidump is the format Windows debuggers have read for twenty years, so the tools already exist
and you do not need Sen to look inside one.

On Windows, open the `.dmp` in Visual Studio or WinDbg. It finds the faulting thread, the stacks of
every other thread and the loaded modules.

On Linux a minidump is not a core file, so `gdb` will not open one directly. **Sen ships no tool
for any of this**; third-party ones do the job, and the second runs on either platform:

| Tool | What it does | Where it comes from |
| --- | --- | --- |
| `minidump-2-core` | turns the dump into an ELF core that `gdb` opens normally | [Breakpad](https://chromium.googlesource.com/breakpad/breakpad) |
| `minidump-stackwalk` | prints every thread's stack, and the annotations below, without a debugger. Windows and Linux alike | [rust-minidump](https://github.com/rust-minidump/rust-minidump), installable with `cargo install minidump-stackwalk` |

```shell
minidump-2-core crash-files/pending/<identifier>.dmp > crash.core
gdb ./my-sen-binary crash.core
```

Whichever you use, you need the binaries and their debug information from the machine that
crashed. A dump opened against different binaries gives addresses and no names.

### What is named in a dump

Your own components are yours to build, so their frames are named as well as you keep their debug
information.

Sen's own frames need the `-release-symbols` archive. A release build is stripped, so the binaries
carry no description of which address belongs to which function, and a dump of one gives you the
module a frame is in and not the name. That description is published separately, for exactly those
binaries, and the debugger reunites the two. [Installing Sen](../getting_started/install.md#reading-a-crash)
says which archive to take and how to point a debugger at it.

**It has to be the symbols for the build that crashed.** The `-relwithdebinfo` archive is a
different build of the same source, optimised differently, so its debug information describes
different machine code. Pointed at a release dump it resolves nothing useful. The name invites the
mistake, which is why the install guide spells the four archives out.

Without the symbols you still get the module each frame is in, which answers whether the crash was
in your code, in Sen's or in a system library. What you do not get is the function.

### What a minidump does not contain

It carries the stacks of every thread and the list of loaded modules, which is why one is tens of
kilobytes rather than the size of the process. Backtraces resolve; reading an
arbitrary object out of the heap does not, because the memory it lived in was never in the file.

A dump also records where each module was on the machine that crashed. If you unpack the binaries
somewhere else, tell the debugger: `set sysroot` or `set solib-search-path` in gdb.

## What the dump carries that a debugger cannot work out

A debugger reads the dump and tells you where the code was. It cannot tell you what the kernel was
in the middle of, because that is Sen's own state rather than the operating system's. Sen records
what it knows as the dump is written, and any tool that reads Crashpad's annotations shows it:

| Annotation | What it says |
| --- | --- |
| `sen.version` | the Sen version |
| `sen.build` | the compiler, whether it is a debug or release build, the word size and the build time |
| `sen.source` | the git ref, commit and whether the tree was modified |
| `sen.application` | `appName` from the configuration |
| `sen.phase` | whether the kernel was arming, starting, running or stopping |
| `sen.components` | what was configured, and every package that loaded, with the version, compiler, mode and commit each was built from |
| `sen.logs` | the most recent log lines, ending with the last thing written before the process died |

`sen.version`, `sen.build` and `sen.source` answer a question the dump cannot: its module list
names the files that were loaded, but not which Sen they are. A dump from a machine you do not
administer is otherwise hard to place against a release.

### Reading them

The annotations sit in the minidump's own Crashpad stream, beside the threads and modules rather
than inside them, so the tool has to be asked for the raw streams. `minidump-stackwalk` is the same
one as for the stacks. It prints each annotation behind the module bookkeeping it belongs to, so
the second stage takes that off:

```shell
minidump-stackwalk --dump --brief crash-files/pending/<identifier>.dmp \
  | sed -n '/annotation_objects/,$p' \
  | sed -E 's/^[[:space:]]*module_list\[[0-9]+\]\.annotation_objects\["([^"]+)"\] = /\1 = /'
```

The same tool answers on Windows, where there is no `sed`. The Crashpad block is the last thing it
prints, so taking the tail and applying the same substitution gets there:

```powershell
(minidump-stackwalk --dump --brief crash-files\pending\<identifier>.dmp | Select-Object -Last 40) `
  -replace '^\s*module_list\[\d+\]\.annotation_objects\["([^"]+)"\] = ', '$1 = '
```

**`--json` does not carry them.** The processed output describes the threads, the modules and the
crash reason and says nothing about this stream, so a script reading that JSON sees none of this.
`--dump` is the raw-stream mode.

From a dump the crash example left, with the middle of `sen.logs` cut:

```text
sen.application = config
sen.build = GNU-12.3.0, release, 64 bit
sen.components = crashDemoComponent configured
crash_demo 0.0.1 GNU-12.3.0 release imported
sen.logs = [2026-09-25 18:57:21.278] [crash_demo] [info] cycle 1 of 6, still fine
[2026-09-25 18:57:23.779] [crash_demo] [info] cycle 6 of 6, still fine
[2026-09-25 18:57:24.278] [crash_demo] [warning] about to fail on purpose, by faulting

sen.phase = running
sen.source = not built from a git checkout
sen.version = 0.0.0
```

They come out in alphabetical order rather than the order of the table above, and a value holding
newlines -- `sen.components` and `sen.logs` -- runs on into the following lines with no name in
front of it.

`sen.components` is worth comparing against `sen.build`. A package built by a different compiler,
or in a different mode, shares C++ structures with a side that lays them out differently, and
neither its code nor Sen's has to be wrong for the process to die. Here both say `GNU-12.3.0` and
`release`, so that is not the explanation; a line that disagreed would be the first thing to chase.

The commit is what pins a package to the source it was built from, which a version string does not:
two builds calling themselves 2.3.1 can differ, and `+modified` says one was built from a tree with
uncommitted changes in it.

`sen.logs` holds roughly the last sixteen kilobytes, oldest whole lines dropped as newer ones
arrive. It carries what your loggers emitted, so their configured levels decide what is in it, and
a logger a component creates while it loads is included. The last line can be cut short: the
handler reads the buffer at whatever moment the process stopped, which may be part-way through a
write.

**A core converted from the dump does not carry them**, because a core file has nowhere to put
them. If you take the `minidump-2-core` route for the stacks, read the annotations from the dump
itself first.

Sen does not rely on a debugger showing this stream, and `minidump-stackwalk` is the route on both
platforms for that reason. If Visual Studio or WinDbg does surface it for you, nothing here stops
you reading it there.

`sen.phase` is worth reading first. A fault while the kernel is stopping is a different kind of
problem from the same fault while it is running, and the stack alone does not separate them.

## Arming it yourself

`sen run` arms crash reporting for you. A program that builds a `Kernel` of its own arms it by
calling `sen::kernel::crash::arm()` once, before the kernel starts, passing the directory the files
should go to.

```cpp
int main(int argc, char* argv[])
{
  if (const auto exitCode = sen::kernel::crash::runHandlerIfRequested(argc, argv))
  {
    return *exitCode;
  }

  if (!sen::kernel::crash::arm("/var/crash/sen"))
  {
    // a fault will not be dumped; the reason has been logged
  }

  // ... build and run your kernel
}
```

`runHandlerIfRequested` goes first, before anything else your program does. Sen has no separate
handler program to install: on Windows the handler is your own executable started a second time with
a private argument, and this call is what recognises that and becomes the handler instead of running
your application again. When it returns a value, that value is the process's exit code and nothing
else should run. On Linux it never claims the process, so the call costs nothing and your code stays
the same on both platforms. Leave it out on Windows and Sen does not start the handler at all, and
warns as it arms; starting it without that call would launch a second copy of your program.

`arm` returns whether the handler is running. False means a fault will not be dumped, and the reason
is on the kernel logger. An uncaught exception is still reported, because the terminate handler does
not depend on the handler process. In a service whose output nobody reads, check the return rather
than trusting the warning to be seen.

`arm` must run **while the process is still single threaded**. On Linux the handler is started by
forking, and a fork keeps every lock the other threads were holding at that moment, which can leave
the handler unable to start. Sen warns if it is armed in a process that already runs more than one
thread.

Arming twice does not move the files. The handler is given its directory when it starts and cannot
be told another one, so a second call keeps the first directory and says so.

Sen describes one kernel per process. A second `Kernel` replaces the first one's name, parameters
and component inventory rather than adding to them, so a crash in either is described as whichever
kernel registered last. Sen warns when the second one registers.

The handler process outlives the call and stays for the life of yours. Sen also creates and
registers a process-global spdlog logger named `kernel` at trace level when no logger by that name
exists, so if you already have one called `kernel`, Sen takes it over and changes its level.

## Core dumps

A minidump says what the threads were doing. A core dump says everything.

**Sen does not stand between a fault and the core.** It installs nothing that swallows the signal,
so the process dies of the fault exactly as it would have, the operating system writes the dump it
was going to write, and the exit status says the process was killed by a signal.

An uncaught exception ends the same way. Sen writes its report first and then lets the process
abort, which is what it would have done with no crash reporting at all, so that route leaves a core
and a minidump as well. The exit status then says the process was killed by a signal rather than
that it returned a failure, which matters if something is watching for a particular code.

### Turning them on, Linux

Nothing in Sen enables them. The limit belongs to the process, so set it in whatever starts yours.

From a shell, for that shell and everything it starts:

```shell
ulimit -c unlimited
```

Under systemd, in the unit rather than in a shell, because a unit does not inherit yours:

```ini
[Service]
LimitCORE=infinity
```

Where the file lands is `/proc/sys/kernel/core_pattern`. A plain name writes into the process's
working directory. A value beginning with `|` hands the dump to a program instead, and on most
distributions that program is `systemd-coredump`.

The limit still applies when the dump goes to a pipe. The kernel does not enforce `RLIMIT_CORE` on
a pipe, but it passes the soft limit to the collector, and `systemd-coredump` drops anything under
one page. So a machine with the limit at zero keeps nothing even though a collector is configured.
Raise the limit.

```shell
cat /proc/sys/kernel/core_pattern     # where dumps go, or which program gets them
coredumpctl list                      # what systemd-coredump has collected
coredumpctl debug <pid>               # open one in a debugger
```

In a container the limit is inherited from whatever ran the container, so raise it there, and a pipe
collector writes on the host rather than inside the container.

### Turning them on, Windows

Windows has no core limit and no `core_pattern`. Its equivalent is Windows Error Reporting, which
writes a dump per application when you ask it to. Create this key and the values under it:

```text
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps
```

| Value | Type | What it does |
| --- | --- | --- |
| `DumpFolder` | `REG_EXPAND_SZ` | where dumps go |
| `DumpCount` | `REG_DWORD` | how many to keep before the oldest is replaced |
| `DumpType` | `REG_DWORD` | `1` for a minidump, `2` for a full dump |

```powershell
$key = 'HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps'
New-Item -Path $key -Force
New-ItemProperty -Path $key -Name DumpFolder -PropertyType ExpandString -Value 'C:\dumps'
New-ItemProperty -Path $key -Name DumpCount  -PropertyType DWord -Value 10
New-ItemProperty -Path $key -Name DumpType   -PropertyType DWord -Value 2
```

Add your executable's name as a subkey of `LocalDumps` to set this for one application instead of
all of them. `procdump -ma -i C:\dumps` from Sysinternals does the same job without touching the
registry yourself, and undoes it with `procdump -u`.

## In a container

**A crash writes files, and a container that exits takes its filesystem with it.** This catches
people out because everything works when you run the image by hand and nothing survives when the
same image runs under `--rm`, or under an orchestrator that replaces the container after it dies.

Point `crashReportDir` at a mounted volume. Anywhere else and the dump is written correctly, once,
into a filesystem that is about to be discarded:

```shell
docker run --rm  -v /var/crash/sen:/crash  --ulimit core=-1  my-image sen run /etc/sen/flight.yaml
```

```yaml
kernel:
  crashReportDir: /crash
```

Core dumps need the same care, and `core_pattern` needs more. It is not namespaced: it is
the host kernel's setting, shared with every container on the machine, and you cannot set it from
inside one. What that means depends on what it holds.

| `core_pattern` on the host | Where the dump lands | What you must do |
| --- | --- | --- |
| A plain name, such as `core` | inside the container, in the working directory | mount that directory, or the dump dies with the container |
| An absolute path | inside the container, at that path | mount it |
| A pipe, a value beginning with a vertical bar | on the host, collected by that program | raise the limit anyway; the collector is given it and `systemd-coredump` honours it |

The core limit is the container's own, so raise it with `--ulimit core=-1` on `docker run`, or
`LimitCORE=infinity` on the unit that starts your container runtime. Raising it inside the image
with `ulimit -c` in an entrypoint works too, but only for what that entrypoint starts.

In Kubernetes an `emptyDir` disappears with the pod, which is the same trap one level up. Use a
`hostPath` or a persistent volume for `crashReportDir` if you want the dump to outlive the pod.

## What the handler process is

While a Sen process runs, a second process sits beside it doing nothing, waiting to be asked for a
dump. It appears in `ps` as `sen-crashpad`, and it carries the same command line as the process it
watches, because it is a copy of it.

It is not a program Sen installs: nothing extra is delivered, and a program that embeds the kernel
needs no additional file. It costs under a megabyte of memory, and it leaves when the process it
watches leaves, including when that process is killed outright.

If you see one on its own, with nothing beside it, that is worth reporting: it should not outlive
what it was watching.

### What it needs from the kernel

The handler reads the crashed process with `ptrace`. Where the kernel refuses there is no minidump,
and the only sign is the handler's own `no ptrace` on stderr.

`cat /proc/sys/kernel/yama/ptrace_scope`:

| Value | What happens |
| --- | --- |
| `0` or `1` | the dump is written. `1` is the common default |
| `2` | only with `CAP_SYS_PTRACE`, so `--cap-add=SYS_PTRACE` under docker |
| `3` | no minidump |

No such file means no restriction. Like `core_pattern` it is the host's setting, and a container
cannot change it.

Core dumps do not use `ptrace`, so they still work at `2` and `3`.

## Settings

| Setting | What it does |
| --- | --- |
| `crashReportDir` | where the files go; the system temporary directory when unset |
| `crashReportDisabled` | set it to `true` and `sen run` arms nothing, so nothing is written. Arming yourself ignores it, and it then suppresses only the JSON report |
