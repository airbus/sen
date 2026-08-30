# Configuring your environment

Sen measures time, and the machine underneath it is not a constant. The same component doing the
same work can take visibly different amounts of time from one run to the next, and most of the
reasons have nothing to do with your code. This page covers what to do about that, and how Sen reads
the clock.

## Quieten the machine

Start with the easy part. Anything else running competes with Sen for cores, cache and memory
bandwidth, and it will show up in your timings. During development, close the browsers, mail clients
and chat applications before you take numbers you intend to trust.

Also detach the debugger. An attached debugger changes the timing of the process it is attached to,
so a run under the debugger is not a run you can measure.

## Why the same work takes different amounts of time

Most of the variance you will see comes from how modern processors work.

**The core does not execute your instructions in the order you wrote them.** It runs independent
instructions in parallel, reorders them, and guesses which way branches will go so it can start work
before the answer is known. A measurement therefore depends on the code surrounding the part you
care about, not only on the part itself. Timing a small piece of code in isolation and timing it in
place can give genuinely different answers, and both can be correct.

**Your core may not be entirely yours.** With simultaneous multithreading, two hardware threads
share one physical core's execution resources. What the sibling thread is doing changes what is left
for you, and it does not change it uniformly: a sibling saturating the floating-point units
interferes with your floating-point work far more than with your integer work. So what matters is
not just how loaded the machine is, but with what.

**The clock rate is not fixed.** Separate mechanisms move it, and each adds noise:

- *Turbo.* When there is thermal and power headroom, cores run above their rated base frequency.
  Nothing guarantees the CPU reaches a given turbo frequency, and nothing guarantees it stays there,
  so a measurement can depend on how long the machine has been working and how warm it is.
- *Power saving.* Idle cores are dropped to lower frequencies or parked. When work arrives the core
  has to ramp back up, and that ramp lands in your earliest samples.
- *Wide SIMD.* On Intel parts, wide vector instructions draw enough extra power that the core lowers
  its frequency to run them, sometimes sharply. Code that uses them can slow down the code around
  it.

For repeatable numbers, pin the frequency: disable turbo and disable power saving, and accept the
base frequency. You will measure a slower machine, but you will measure the same machine twice.

## How Sen reads the clock

Reading the time looks free and is not. `clock_gettime()` and `gettimeofday()` are system calls;
Linux hands them out through the vDSO so they do not have to trap into the kernel, which makes them
much cheaper than an ordinary system call but still costly enough to matter when Sen is timing work
at its own granularity.

x86 processors offer something cheaper. The TSC (Time Stamp Counter) is a register that a single
instruction, `RDTSC`, reads from user space in a handful of cycles.

The catch is that the TSC is only a usable clock if it ticks at a steady rate. What Sen needs is an
*invariant* TSC: one that runs at a constant rate whatever P-, C- or T-state the core is in, and
that is synchronized across cores. Sen asks the processor directly, reading
`CPUID.80000007H:EDX[8]`, and falls back to the ordinary system timers when the bit is not set,
logging a warning as it does so. On Intel parts, that bit means Nehalem or later.

This is x86 only, on both Linux and Windows. On AArch64, including Apple silicon, Sen never takes
the TSC path and always uses the system timers.

The processor is not the only thing that can get this wrong: hypervisors emulate the counter,
firmware problems and errata affect it, and an operating system can report an invariant TSC that
does not behave like one. On Linux you can cross-check what the kernel decided for its own use:

```shell
$ cat /sys/devices/system/clocksource/clocksource0/current_clocksource
tsc
```

Any other value here, `kvm-clock` for example, means the kernel did not trust the TSC either.

If you want Sen to leave the TSC alone, set the `SEN_AVOID_TSC` environment variable. Only its
presence is tested, so any value turns the TSC off. `SEN_AVOID_TSC=0` disables it just as firmly as
`SEN_AVOID_TSC=1`. To switch it back on, remove the variable from the environment instead of giving
it a falsy value.

## Thread priority, CPU affinity and stack size

Every component runs in its own thread, so these are **per-component** settings rather than
process-wide ones. They go on the component's entry in the configuration file, alongside `group` and
`freqHz`, and they work the same way under `load:` and `build:`.

```yaml
build:
  - name: controlLoop
    group: 3
    freqHz: 100
    priority: nominalMax    # lowest | nominalMin | nominalMax | highest
    cpuAffinity: 12         # bitmask: bits 2 and 3, so processors 2 and 3
    stackSize: 1048576      # bytes; 0 or omitted means the system default
```

**Priority** takes one of four values. `nominalMin` and `nominalMax` bound what is intended for
ordinary component work; `lowest` is for background work that should yield to everything else, and
`highest` is for supervisory work. What these map onto is the operating system's own scheme, so the
practical spread between them depends on the platform and on how the process was started.

**CPU affinity** is a bitmask, one bit per processor: `1` is processor 0, `2` is processor 1, `12`
is processors 2 and 3. The field is 64 bits wide, so processors beyond the first 64 cannot be
addressed this way.

!!! warning "An affinity Sen cannot apply stops the process"

    If the operating system rejects the mask, which is what happens inside a container whose
    processor set does not include the processors you asked for, thread creation fails, and a
    component that cannot create its thread terminates the whole process rather than continuing
    without the setting. It is reported before that happens, so the cause is in the log, but the
    process does not survive it.

    The same configuration can behave differently on a developer machine and in a container: a
    mask that is valid on the host may not be valid inside the container.

**Stack size** is in bytes and only worth setting when you know a component needs more than the
default, or when you are running many components on a memory-constrained target and want less.

## CPU shielding

Shielding, keeping the operating system's own scheduler away from a set of processors so that only
what you place there runs, is done to the operating system, not to Sen. On Linux that is
`isolcpus`, `cset shield` or a cgroup, depending on your distribution and how the machine is
managed.

Sen's part is the second half: once processors are reserved, `cpuAffinity` is how you put a
component on them. Set the affinity to the shielded processors and give the component a priority
that suits its role, and the two together give it a processor to itself.

## Queue sizing

Each component also has `inQueue` and `outQueue`, which decide how much traffic it will buffer and
what happens when the buffer is full. `evictionPolicy` takes `dropOldest` or `dropNewest`, and
`maxSize: 0` means unbounded. These matter when a component cannot keep up with what is being sent
to it, because they decide where the loss appears.
