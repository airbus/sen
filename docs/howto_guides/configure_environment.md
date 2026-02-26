# Configuring your environment

In a multitasking operating system, applications compete for with each other to get resources. This
has a visible effect on any time and performance measurements taken by Sen.

In general, you should minimize interference caused by other programs running on the same machine.
If experiencing some performance unstability during development, close all web browsers, email
clients, chats, and all other non-essential processes. Also, check that you don’t have the debugger
connected into the Sen process, as it also impacts the timing results.

Today's processors are so complex that it can get near to impossible to predict how they will behave
in detail. Things like the cache configuration, prefetcher logic, memory timings, branch predictor,
execution unit counts are all elements that have gained relevance as mechanisms to raise the
instructions-per-cycle count.

In general:

- Be aware of superscalar out-of-order speculative execution: processors available on the market do
  not execute machine code linearly, as laid out in the source code.
- Understand simultaneous multithreading. It requires careful examination of what else is running on
  the machine, or even how the operating system schedules the threads of your own program, as
  various combinations of competing workloads (e.g., integer/floating-point operations) will be
  impacted differently.
- Disable turbo mode frequency scaling: While the CPU is more-or-less designed always to be able to
  work at the advertised base frequency, there is usually some headroom left, which allows usage of
  the built-in automatic overclocking. There are no guarantees that the CPU can attain the turbo
  frequencies or how long it will uphold them, as there are many things to take into consideration.
  Best keep it disabled and run at the base frequency. Otherwise, your timings won’t make much
  sense.
- Disable power saving. The same as turbo mode, but in reverse. While unused, processor cores are
  kept at lower frequencies (or even wholly disabled) to reduce power usage. When your code starts
  running, the core frequency needs to ramp up, which may be visible in the measurements. Again, to
  get the best results, keep this feature disabled.
- AVX offset and power licenses. Intel CPUs are unable to run at their advertised frequencies when
  they perform wide SIMD operations due to increased power requirements. Therefore, depending on the
  width and type of operations executed, the core operating frequency will be reduced, in some cases
  quite drastically. Be aware of this when using these instructions.

## Time sources and the TSC mechanism

Operating systems (like the Linux kernel) provide functions like `gettimeofday()` and
`clock_gettime()` system calls for high-resolution time measurement in user applications. The
`gettimeofday()` provides microsecond-level resolution, while `clock_gettime()` provides
nanosecond-level resolution. However, one major concern when using these system calls is the
additional performance cost caused by the overhead of the function call itself.

In order to minimize the performance cost of the `gettimeofday()` and `clock_gettime()` system
calls, the Linux kernel uses the `vsyscalls` (virtual system calls) and VDSOs (Virtual Dynamically
linked Shared Objects) mechanisms to avoid the overhead of switching from user to kernel space.

Although `vsyscalls` implementation of `gettimeofday()` and `clock_gettime()` is faster than regular
system calls, the perf cost of them is still too high to meet the latency measurement requirements
for some perf sensitive application.

The TSC (Time Stamp Counter) registry provided by x86 processors is a high-resolution counter that
can be read with a single instruction (`RDTSC`). On Linux this instruction can be executed from user
space directly, that means user applications could use one single instruction to get a fine-grained
timestamp (nanosecond-level) in a much faster way than `vsyscalls`.

However, the TSC mechanism can be sometimes unreliable. Sen makes use of the TSC mechanism only if
it detects an "Invariant TSC", otherwise it will fall back to `vsyscalls`. "Invariant TSC" means
that the TSC will run at a constant rate in all ACPI P-, C-, and T-states (In Intel processors,
invariant TSC only appears on Nehalem-and-later).

Most recent SMP systems supporting “Invariant TSC” ensure that TSC is synced among multiple CPUs. At
boot time, all CPUs connected with same RESET signal are reset and the TSCs are increased at same
rate.

If the TSC sync test is passed during the Linux kernel boot, the following sysfs file would export
"tsc" as current clock source:

```shell
$ cat /sys/devices/system/clocksource/clocksource0/current_clocksource
tsc
```

If you see some other value (such as "kvm-clock") then your system is not supporting an invariant
TSC and Sen will not use it.

However, the usage of the TSC mechanism can suffer from other pitfalls such as firmware problems,
hardware Erratas, emulation on hypervisors, etc. It can also happen that the Linux kernel is
incorrectly reporting an invariant TSC.

If you want Sen to avoid the usage of the TSC mechanism, set the `SEN_AVOID_TSC` environment
variable.

![Screenshot](../assets/images/work_in_progress_light.svg#only-light){: style="width:500px"}
![Screenshot](../assets/images/work_in_progress_dark.svg#only-dark){: style="width:500px"}

## CPU shielding

TODO

## Component CPU affinity pining

TODO

## Component thread priority setting

TODO
