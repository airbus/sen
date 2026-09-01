# Configuring component sleeping between updates

Two different sleep policies can be configured for a Sen component. The `SleepPolicy` type is shown
below:

```rust
// Use the native system sleep
struct SystemSleep;

// Be more precise at the expense of some CPU cycles.
struct PrecisionSleep
{
  veryCoarseGrainSleepTime : Duration,  // First set of sleeps. If 0 defaults to 7ms
  coarseGrainSleepTime     : Duration   // Second set of sleeps. If 0 defaults to 1ms
}

// Component sleep policy
variant SleepPolicy
{
  PrecisionSleep,
  SystemSleep
}
```

The two sleep policies are the following:

## System Sleep

It is the usual sleep of the component's thread until the next update is due. This policy can be
configured by the user for each of the components launched in a process independently, or for the
whole process using the `SEN_KERNEL_DISABLE_PRECISION_SLEEP` environment variable. The system sleep
removes load from the CPU by removing the active waiting before the next update but at the same time
sacrifices real time update precision.

## Precision Sleep

In this sleep policy, if the component finishes all computations before the time in which the next
update of the component is scheduled, a `PrecisionSleeper` is used to provide a more precise wake-up
time by repeatedly sleeping for short periods of time before the next update is due.

### Sleeping in steps

In order to make the precision sleeper more efficient, it sleeps the thread repeatedly using sleep
times that get smaller as the wake-up time approaches.

The `PrecisionSleeper` starts functioning when all the computations of the previous update have been
finished and there is time remaining until the next scheduled update. From there:

- First, when the remaining time is bigger than `veryCoarseGrainSleepTime`, the
  **veryCoarseGrainSleep** puts the thread to sleep until the time remaining is equal to it. It
  defaults to 7 milliseconds, and can be configured using the sleep policy in the component YAML
  configuration. Additionally, if `KERNEL_SLEEP_THRESHOLD_MS` is set on the environment, that value
  will override the one previously configured in the YAML.

- Once the remaining time is smaller than that threshold, the thread is put to sleep repeatedly for
  periods of **coarseGrainSleep** time, one millisecond by default and configurable in the same
  place. Each of those sleeps is measured, and the step stops once the remaining time falls below
  what the next one is estimated to cost.

- Once no further sleep fits, an empty loop keeps the thread awake and ready to perform the next
  update on time.

### How the sleeper estimates what a sleep costs

A sleep costs what it asks for plus the operating system's own overhead in getting the thread back
on a core. That overhead is roughly the same whatever size of sleep is requested, so the sleeper
measures the overhead itself rather than the cost of any particular sleep.

It keeps the largest overhead it has seen rather than an average. An average sits below half of its
own measurements, and every one of those is a sleep that was taken but could not be afforded, which
is a sleep that runs past the wake-up instant. It starts out pessimistic for the same reason:
believing sleeps are free until proved otherwise means taking the first few before finding out.

Because it holds the largest, it also eases down a little on every cycle. Nothing else would ever
bring it back: on a machine that has become slow the estimate can grow past the time a cycle leaves
idle, at which point no sleep fits, no sleep is taken, and nothing is left to measure. Easing it
down means a machine that has recovered is found out within about a second, and being wrong costs
one sleep, because the first step that runs measures the truth and puts it straight back.

### Performance issues

While this `PrecisionSleeper` provides sufficiently precise wake-up times, it comes with a tradeoff
in CPU performance, becoming more concerning as the frequency of the component increases. This
sleeping and waking-up process is executed prior to each thread update, increasing the stress on the
CPU as the frequency of the component increases.

That behavior has proven to saturate cores when running components at frequencies close to 100Hz
using the default `veryCoarseGrainSleepTime` of 7 milliseconds. The worst of that came from the
estimate settling above the time a cycle leaves idle and staying there, which left the component
spinning for its whole idle interval; it no longer can. What remains is the ordinary cost of the
busy loop. In case the component needs to be cycled at high frequencies, you can set the
`KERNEL_SLEEP_THRESHOLD_MS` environment variable to a smaller value, thus decreasing the period in
which the `PrecisionSleeper` is active.

Note that this threshold is also the margin against the operating system waking the thread late, so
lowering it trades that protection for CPU. Measure the wake-up accuracy you actually get before
choosing a value.

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

- **Kernel Component**: it is a low frequency component in charge of maintenance/monitoring work. It
  does not require high precision wake-up times, so the default sleep policy for it is
  `SystemSleep`.
- **Loaded Components** (ether, explorer, influx, logmaster, py, recorder, replayer, rest, shell,
  term, tracy): None of them requires a high precision for its updates, so their default sleep
  policy is `SystemSleep`.
- **Built Components**: Components build by Sen on behalf of users default to the `PrecisionSleep`
  policy.
