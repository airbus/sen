# Observing a running system

The kernel measures its own load continuously, and this page is about reading that.

Be clear about what the numbers are. They name the kernel's own machinery: its queues, its lanes,
its threads, the work it does reclaiming memory. The vocabulary is the kernel's rather than yours,
and none of it describes your model. That makes this an advanced surface, and a useful one for
exactly that reason: when a system is behaving strangely and the cause is not in your own code, this
is where the answer is, and there is a lot of it.

The page covers how you read the measurements, which of them mean something on their own, and what
they will not tell you.

## Tracer events

A tracer receives events as they happen, and it is the surface a running system actually exposes.
You install a factory once, before components start, with `PreloadApi::installTracerFactory`. Each
kernel thread then gets its own tracer, so a tracer needs no locking of its own. The factory does:
it is called from each thread as that thread starts, so an implementation that sets up a backend
lazily has to make that setup thread-safe.

An event is a name and a list of typed key-value fields. One contract matters and is easy to miss:
**the keys and any string values are borrowed views, valid only for the duration of the call.** A
tracer that stores an event must copy first.

| Event | Fires when |
|---|---|
| `ksQueueHighWaterMarkExceeded` | the kernel server's queue depth crosses its threshold |
| `sessionWorkerStrandHighWaterMarkExceeded` | a session worker's queue crosses its threshold |
| `pendingClassSpecsHighWaterMarkExceeded` | pending class specifications cross their threshold |
| `commitFanOutSendCountExceeded` | one commit fans out to more sends than its threshold |
| `transportOutboundHighWaterMarkExceeded` | the queue toward a remote kernel crosses its threshold |
| `cascadeReclamationBacklogExceeded` | the reclamation backlog crosses its threshold |
| `oversizedSendRefused` | a value or event was refused for being too large to send |
| `deferredWorkCapExceeded` | deferred work passed its cap |
| `ksShutdownCompleted` | the kernel server finished shutting down |

[Tracy](../components/tracy.md) implements the interface and renders these into its message channel,
so loading the `tracy` component shows them without your writing anything.

## Warning thresholds

The first six events fire because you asked for them. `KernelWarningThresholds`, set through
`KernelConfig`, holds the marks they watch: the kernel server's queue depth, a session worker's
queue, pending class specifications, a commit's fan-out, the outbound queue toward a remote kernel,
and the reclamation backlog. A seventh, the per-step cascade count, is accepted and stored but has
nothing to raise yet, so setting it currently does nothing.

**Zero means off, and zero is the default.** Nothing fires until an operator opts in, so a system
that has never been tuned is silent by design.

**They are signal only.** Crossing a mark reports it and changes nothing. Nothing is throttled,
dropped or slowed because a threshold was crossed.

**Each latches, per subject.** After a crossing it re-arms only once the value falls below half the
mark, so a system sitting just over the line reports once per episode rather than continuously. The
subject is the thing being measured, not the kernel: one report per lane, per remote kernel, per
producer. A hundred busy lanes can report a hundred times.

**Only the queue crossing also writes to the log.** That one logs a warning naming the rolling p99,
the mark, the current depth and the heaviest kind of work. The other five are reported to the tracer
and nowhere else, so an operator watching only the log sees one of the six conditions and is blind
to the other five. Installing a tracer is not a convenience here; for five of these it is the only
way to see them at all.

## What the numbers mean

Four carry a signal on their own.

**Arrivals minus completions** is the leading indicator, and the one to watch first. Sustained
positive means work is arriving faster than it drains, and it says so before any queue reaches its
mark. It also separates a burst from a runaway: a burst spikes it and falls back to
about zero, a runaway holds it positive. The mailbox equivalent, work pushed climbing without work
drained following it, means one runner has stopped draining, which is what a wedged component looks
like.

**Queue depth and queue bytes answer different questions.** Depth counts what is waiting; bytes
estimate the memory that is waiting. Depth rising means the server is falling behind the rate of
work. Bytes rising while depth stays flat means fewer but larger operations, a bulk publish or a big
batch, which is a memory problem rather than a rate one. Bytes are the honest signal for an
unbounded queue, because counting entries misses the case of a few enormous ones.

**The server thread's busy fraction** is the capacity number. Sustained above one half means half
the single thread's budget is gone and it is time to plan for the ceiling rather than wait for it.
At one it is saturated, and the queue behind it can only grow.

**Per-lane depth, read with the yield count beside it,** answers whether a session is busy or slow.
Many yields means the fairness quantum is working and the lane is busy but bounded. Few yields with
long per-item times means the work itself is slow. The symptom looks the same; the fix does not.

Three more matter once something is already wrong. **Latency by kind of work** is the follow-up to a
queue warning: it names which work is slow, which is the difference between an alert and a
diagnosis. **Outbound depth toward one remote kernel** is the gray-peer signal, where heartbeats
look fine but the peer drains slowly; the queue toward it is unbounded and nothing is shed, so a
number climbing for one peer and not others names the one that cannot keep up. **Reclamation
backlog, read with the interference delay,** is the restart-storm pair: a growing backlog under
repeated peer loss means cleanup is being outrun, and the delay beside it says how long ordinary
work waited behind that cleanup, which is what decides whether the backlog matters. **Type-handshake
counts per remote kernel** distinguish two peer problems that look alike: pending work rising
alongside timeouts means a peer is not answering, while pending rising alongside rejections means a
peer is refusing.

## Where the numbers live

`KernelServerStats`, declared in `libs/kernel/stl/sen/kernel/kernel_objects.stl`, is the
field-by-field reference for everything the kernel measures. Each field carries a comment saying
what it counts.

Reading it is another matter. In a running system the tracer events above are the surface, and they
carry the relevant readings in their payloads. In a test, `TestKernel::fetchStats()` returns the
whole structure and is safe to call from the test thread at any moment, which lets a test assert on
a number instead of waiting and hoping.

## What this does not tell you

All of this is the kernel's own load: what is queued, how long it takes, how busy its threads are.
None of it says whether your components are keeping their schedules.

For that, see [When a component runs out of time](../users_guide/execution_model.md), which covers
overruns and missed frames and where each is reported.
