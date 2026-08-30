# Configuring component sleeping between updates

Two different sleep policies can be configured for a Sen component. The `SleepPolicy` type is shown
below:

```rust
--8<-- "libs/kernel/stl/sen/kernel/basic_types.stl:sleep_policy"
```

They differ in what the thread does while it waits.

## System Sleep

It is the usual sleep of the component's thread until the next update is due. This policy can be
configured by the user for each of the components launched in a process independently, or for the
whole process using the `SEN_KERNEL_DISABLE_PRECISION_SLEEP` environment variable. The system sleep
removes load from the CPU by removing the active waiting before the next update but at the same time
sacrifices real time update precision.

## Precision Sleep

In this sleep policy, when a component finishes its computations before its next update is due, a
`PrecisionSleeper` takes over the wait. It sleeps repeatedly for short periods rather than once for
the whole interval, which wakes the thread closer to the time the update is due.

### Three different sleep times

In order to make the precision sleeper more efficient, it sleeps the thread repeatedly using three
different sleep times, each of them smaller as we approach the wake-up time.

The `PrecisionSleeper` starts functioning when all the computations of the previous update have been
finished and there is time remaining until the next scheduled update. The sleeping process is
divided as follows:

- First, when the remaining time is bigger than `veryCoarseGrainSleepTime`, the
  **veryCoarseGrainSleep** puts the thread to sleep until the time remaining is equal to it. It has
  a default of 7 milliseconds and is set through the sleep policy in the component YAML
  configuration. If `KERNEL_SLEEP_THRESHOLD_MS` is set in the environment, that value overrides the
  one configured in the YAML.

- Once the remaining time is smaller than the previous threshold, the **coarseGrainSleep** puts
  the thread to sleep repeatedly, measuring each sleep as it goes. The operating system does not
  sleep for exactly as long as it is asked, so the sleeper keeps a running estimate of what a
  sleep really costs (the mean of those measurements plus one standard deviation), and stops once
  the remaining time falls below it. The estimate starts at 5 milliseconds and moves in either
  direction as the measurements accumulate. `coarseGrainSleepTime` is set to 1 millisecond by
  default and can also be configured by the user using the component YAML configuration.

- Once the remaining time is smaller than that estimated threshold, an empty loop keeps the thread
  awake and ready to perform the next update on time.

### Performance issues

While this `PrecisionSleeper` provides sufficiently precise wake-up times, it comes with a tradeoff
in CPU performance, becoming more concerning as the frequency of the component increases. This
sleeping and waking-up process is executed prior to each thread update, increasing the stress on the
CPU as the frequency of the component increases.

That behavior has proven to saturate cores when running components at frequencies close to 100Hz
using the default `veryCoarseGrainSleepTime` of 7 milliseconds. In case the component needs to be
cycled at high frequencies, we recommend setting the `KERNEL_SLEEP_THRESHOLD_MS` environment
variable to a smaller value, thus decreasing the period in which the `PrecisionSleeper` is active.

Decreasing this threshold results on a significant reduction of the stress in the CPU core, but at
the same time it affects the wake-up precision negatively.

### How to configure the sleep policy in a Sen component

The YAML below shows how to configure the sleep policy for all types of components in a process
(kernel, loaded components and pipeline components):

```yaml
# configuring the sleep policy for the kernel component (the one used by the kernel to
# interact with other components).
kernel:
  sleepPolicy:
    type: SystemSleep

# configuring the sleep policy in the shell component
load:
  - name: shell
    group: 2
    freqHz: 1
    sleepPolicy:
      type: SystemSleep

# configuring the sleep policy in the pipeline component
build:
  - name: myComponent
    group: 3
    freqHz: 1
    imports: [my_package]
    sleepPolicy:
      type: PrecisionSleep
      value:
        veryCoarseGrainSleepTime: 8 ms
        coarseGrainSleepTime: 2 ms
    objects:
      - class: my_package.MyClassImpl
        name: myClass
        bus: my.tutorial
        prop1: some value
```

### Default sleep policy of Sen components

The different Sen components have different sleep policies depending on how important is for each of
them to have precisely timed updates. The default values are the following:

- **Kernel component**: it is a low frequency component in charge of maintenance/monitoring work. It
  does not require high precision wake-up times, so the default sleep policy for it is
  `SystemSleep`.
- **Loaded components** (ether, explorer, influx, jsonrpc, logmaster, py, recorder, replayer, rest,
  shell, tracy, webexplorer): None of them requires a high precision for its updates, therefore
  their default sleep policy is `SystemSleep`.
- **Pipeline components**: Components built by Sen on behalf of users default to the
  `PrecisionSleep` policy.
