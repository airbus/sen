# Integration testing

[Unit testing](unit_tests.md) drives one kernel from a test binary. Integration testing drives
several. You can run them inside one process or in containers, and the choice changes both what you
exercise and what it costs.

Running the kernels in one process is fast and repeatable, and it can stage failures that are hard
to arrange for real. Running them in containers is slower, but the messages go over a real
transport, so it is the one that tests the whole chain.

## Several kernels in one process

A cluster stands up several kernels inside the test binary, connected by one shared in-process
network. There are no processes to spawn and no ports to allocate, and everything happens on one
machine under one debugger.

### Two clusters, depending on the mode

`setRunMode` chooses the mode, and it changes what the tool is. Set it before adding any kernel: it
applies to kernels built after it, and one already added keeps what it was built with.

In **virtual time**, the default, the components advance only when you step them. That makes their
execution deterministic and takes wall-clock waiting out of the logic you are testing. It does not
stop the kernel's own threads: discovery, transport io and the kernel server keep running whether or
not you step. So a virtual-time test is repeatable in what the components do, and still has to wait
for the kernel by polling state rather than by counting steps.

In **real time**, the kernels free-run on their own threads. Runs differ from each other, and that
is the point: this is the mode for interleavings and races, which stepping cannot produce because it
does not let two components run at once.

Stepping belongs to virtual time. `stepAll` and `stepAllUntil` drive the kernels' `step`, which
aborts on a real-time kernel, so the section below applies to the default mode only.

### The shape of a test

```c++
TEST(LocalCluster, ObjectCrossesBetweenKernels)
{
  // Declaration order matters: the handles are released while the kernels are still alive,
  // and the kernels call into the components while shutting down, so the components must
  // outlive the cluster.
  TestComponent publisher;
  TestComponent subscriber;
  LocalCluster cluster;
  std::shared_ptr<ObjectSource> publisherSource;
  std::shared_ptr<PingImpl> ping;
  std::atomic<bool> saw {false};
  std::shared_ptr<Subscription<PingInterface>> subscription;

  publisher.onInit([&](InitApi&& api) -> PassResult {
    publisherSource = api.getSource("cluster.demo");
    ping = std::make_shared<PingImpl>("ping", VarMap {});
    publisherSource->add(ping);
    return done();
  });
  publisher.onRun([](RunApi& api) { return api.execLoop(std::chrono::seconds(1), [] {}); });

  subscriber.onInit([&](InitApi&& api) -> PassResult {
    api.getTypes().add(PingInterface::meta());
    subscription = api.selectAllFrom<PingInterface>("cluster.demo",
      [&saw](const auto& added) { if (added.begin() != added.end()) { saw = true; } });
    return done();
  });
  subscriber.onRun([](RunApi& api) { return api.execLoop(std::chrono::seconds(1), [] {}); });

  cluster.addKernel(&publisher);
  cluster.addKernel(&subscriber);

  cluster.stepAllUntil([&] { return saw.load(); }, 2000);
  EXPECT_TRUE(saw.load());
}
```

That comment is not decoration. Components must outlive the cluster because the kernels call into
them while shutting down, and subscription and source handles must be released before the cluster
goes. Declaration order is how you get both, so it is the first thing to write and the easiest
thing to get wrong.

Kernels are added with `addKernel` and addressed afterwards by their index in that order, so the
first kernel added is 0. They join named sessions and can share more than one, with objects crossing
on each. A kernel's configuration must not install a transport of its own, because the cluster
injects one.

### Asking what a kernel can see

The network is inspectable through `network()`, which is what lets a test assert about visibility
rather than infer it from behaviour. `processIdOf` gives a kernel its address. From there `peersOf`
answers which peers a kernel can reach and `getVisibleProcesses` which processes it can see at all.
Those two differ on purpose: the second includes processes this kernel would refuse to talk to,
which is how you tell "cannot see it" apart from "sees it and rejects it". `hasDiscovered` asks
whether one kernel has found another, and `hasTrackTo` and `currentTrackOf` inspect the contact it
currently holds.

### Stepping, and the two budgets

`stepAll()` advances every kernel. `stepAllUntil()` steps them each round until your predicate
holds, and throws when its budget runs out so a test that never converges fails instead of hanging.

The budget is either a step count or elapsed wall-clock time, and the choice is not cosmetic. A
**step count** is right for a predicate that only depends on stepping. **Elapsed wall-clock time**
is right for one that waits on threads which run whether or not you step: discovery, transport io,
the kernel server. A loaded machine slows those threads and a wall-clock budget together, so the
allowance keeps its meaning; a step count would silently become a much shorter deadline exactly when
the machine is busy.

Because discovery is one of those free-running things, a test that needs cross-kernel traffic asks
whether the kernels have found each other first, then measures what it is actually about. Otherwise
slow discovery is charged to whatever the test was trying to time.

### Staging failures

This is what the cluster buys you that a real deployment cannot: conditions you can produce on
demand, in one process, repeatably.

**Blocking a kernel.** `blockKernel` stops it consuming what it is sent while it stays attached and
discoverable, and `unblockKernel` releases it. This stages a peer that is stuck, not one that is
gone.
What it was sent is held, not lost, and handed over when you release it, because a reliable
transport does not drop messages just because the far side is busy. That distinction matters: a peer
that is truly gone is a disconnection and takes an entirely different path through the kernel. There
is no other honest way to stage silence, since a genuinely hung process would hang your test with
it.

**Hiding from discovery.** `hideFromDiscovery` keeps one process out of what another can find while
leaving the traffic between them untouched. That combination is not artificial: it is the real
start-up window, because a session's transport delivers from the moment it exists and the kernel
server is told about the remote kernel separately and later. `showInDiscovery` closes the window
again.

**Forging a membership event.** The network can deliver a participant-joined announcement carrying
field values of your choosing, as though a kernel had announced itself. That turns adversarial input
into something you can write a test about, rather than only the absence of input. This is the least
settled part of the surface, so expect the call itself to be reshaped before it ships.

**Cutting a peer off.** `severProcess` drops every session attachment a process holds at once, with
no goodbye traffic, so the others watch it vanish the way a crash looks. Given a session as well it
cuts only that one, and deliberately one-sidedly: the severed kernel is not told, still sees the
session and can still send into it, and its frames then arrive under an ended contact and are
refused by the kernel's own fences. Any other session it holds carries on. That is a half-open
connection, which is otherwise hard to arrange on purpose.

**Injecting frames.** `injectBroadcastFrame` puts raw bytes into a session as though a kernel had
sent them, including bytes no healthy sender would produce, and `injectDirectFrame` does the same on
the reliable channel, arriving through the door control frames ride. That is how a malformed frame
reaches real deserialization instead of being discarded earlier as unroutable.

**Reporting a loss that did not happen.** `injectProcessLost` delivers a transport-level loss report
to one observer while both kernels stay attached, which is how you stage a discovery flap.

**Watching what a kernel says.** `observeFrames` takes a callback that receives every control frame
offered into a session, with its raw bytes. Asserting on what a kernel says is sturdier than
asserting on what it logs: renaming a log line leaves an assertion that can no longer fail, and this
suite has been caught by that.

**Bounding asynchrony.** `pendingInboundOf`, `inboundCountOf` and `armedTimerCountOf` let a wait
finish on a count rather than a timeout. The last one separates a cancelled timer from an abandoned
one, which behave identically until the abandoned one fires.

**Mismatching versions.** Pass a different protocol version or schema fingerprint to
`addKernel`, which is what that kernel's transport then announces. Kernels that disagree never
appear to each other as reachable, and `refusalCount` gives you a number to assert on. See
[Versioning and compatibility](../users_guide/versioning.md).

One thing the network will not do is lose a message on the reliable channel. That is a decision
rather than a gap: a reliable transport does not lose messages, so a dropped one would model
nothing that can happen. Loss that can happen belongs to the best-effort plane.

### What this makes testable

The scenarios below are all things Sen tests with this harness, which is a stronger claim than
saying they are possible.

An object and a method call crossing between kernels. A subscriber that was already watching when
the object appeared, against one that arrives later. A peer that goes silent mid-exchange, so a type
request ends loudly and then recovers, a call times out, and a late answer is correctly ignored,
including when a per-call timeout overrides the kernel's default. A join that reaches a kernel
before its link exists, waits, and is still delivered. Two kernels that refuse each other over a
version or a schema fingerprint. A name collision across kernels, where the older incumbent wins and
an exact tie still picks exactly one loser. A kernel that is lost and rebuilt while a bystander
kernel carries on untouched. One connection of two dying while the survivor keeps serving. Kernels
co-hosted in one process keeping distinct identities across a restart. A malformed frame dropped
without stopping the traffic that follows. A broadcast for a bus nobody opened, dropped. A peer that
vanishes the way a crash looks. A half-open connection where one side still believes it is attached.
A loss report delivered to one observer while both kernels are still there.

What the cluster cannot tell you is whether any of this survives a real network. Everything above
happens in one process over an in-process transport, so serialization, sockets, discovery over the
wire and the behaviour of separate operating-system processes are all out of scope. That is what the
container harness is for.

Sen's own tests for these live in `libs/kernel/test/support/loopback/test/`.
`local_cluster_test.cpp` holds the short ones worth reading first, including the object and
method-call crossings and the protocol-version and schema refusals. `local_cluster_strict_test.cpp`
holds the longer ones: the unanswered call that times out while a late answer is ignored, the
per-call timeout override, and the join that arrives before its link and is still delivered.

## Several processes in containers

The container harness runs the real thing: separate processes, each in its own container, on a
Docker network, talking over the actual transport. It is the slower option and the one that proves
the chain end to end.

Sen's own integration suite is built this way and is worth reading before you write your own. It
lives in `libs/kernel/test/integration/`, and the cases there cover object synchronization, interest
filtering, transport behaviour, type clashes, runtime compatibility, crash reporting and stress.
Each case is an ordinary Sen package: an `.stl` file, its C++, configuration templates and a readme
explaining the scenario.

The driver is `run.py`, which uses `testcontainers` to bring up a network and a container per
participant, and streams their logs. Three environment variables control it.
`SEN_INTEGRATION_TEST_IMAGE` names the image and has no default, because the harness runs binaries
that are already built and the image therefore has to match the system they were built on;
`tools/ci/runtime.Dockerfile` builds a suitable one. `SEN_INTEGRATION_TEST_TIMEOUT` bounds a run,
defaulting to thirty seconds. `SEN_INTEGRATION_TEST_MOUNT` is where the repository appears inside
the container, defaulting to `/home/builder/sen`.

The part worth copying is how the assertions are written. The test driver is itself a Sen component:
the configuration loads `ether` for the transport and `py` running a test module, so the code making
the assertions is inside a kernel, subscribing to objects and calling methods like any other
participant, rather than poking at the system from outside it.
