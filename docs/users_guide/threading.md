# Threading and object lifetime

Sen runs your components on threads it owns. This page states which thread runs your code, what you
may call from where, and who owns an object once you have published it.

## Which thread runs your code

**Everything the framework calls on your behalf runs on the runner thread that owns the thing
involved.**

For an object's own hooks, `registered`, `update`, `unregistered`, `preDrain` and `preCommit`, that
is the runner of the component the object lives in. For a method implementation, it is the runner
that owns the target object. For a method's result, a property-changed callback, an event callback
or a discovery callback, it is the runner that made the call or the subscription.

It is never a transport thread, never the kernel's own thread, and never a thread you created.

`preDrain` and `preCommit` are opt-in. They do nothing at all unless the object returns `true` from
`needsPreDrainOrPreCommit()`, which is the usual reason for overriding them and seeing nothing
happen. A callback also does not necessarily run inside the call that produced it. A notification
crossing components waits for the receiving runner's next drain, so expect up to a cycle between the
change and the callback reporting it.

## What you may call, and from where

**Every call into Sen, including calls on objects, belongs on your component's runner thread.** That
is the thread your hooks and callbacks already run on, so code written inside them is on the right
thread by default. A thread you started yourself is not.

What varies is how much of that rule the framework enforces.

**Some calls are checked and abort.** Making a remote method call from a thread that is not a
runner, creating a source on a component that is already running from another thread, and
subscribing from a foreign thread. Each of these stops the process.

**Some are checked and only warn.** Calling `add`, `remove` or `publish` from a thread that is not
the owning runner logs a warning naming your component and the call, and the call still goes
through. The warning means the code is wrong and will stop working. These calls were once documented
as thread-safe, so they keep working for anything that relied on that, and the hard stop is
scheduled.

**Reads are not checked at all.** Reading a property or an object from another thread breaks the
same rule as everything else here, and nothing will tell you. The reason is cost. Read paths have
far more call sites than write paths and no single place to put a check, so guarding them would be
paid for across the whole system. A fire-and-forget method call is unchecked for a different reason:
the state that made it hazardous was removed.

**One case is undefined behavior**: reading `local.kernel` from a test thread. Use
[`fetchStats()`](../howto_guides/unit_tests.md) instead, which is safe to call at any moment.

## How long a reference lasts

`RunApi&` and `RegistrationApi&` belong to your component's runner, and every hook receives the same
ones. Use them within the life of your component, do not put one anywhere it could outlive the
component, and never use one from another thread.

## Publishing an object

`add` hands the kernel a reference that it keeps. The object stays alive for as long as the kernel
holds that reference, whatever happens to your own pointer, and `remove` gives the reference back.
You are not required to call `remove` before dropping your last reference. The kernel's own
reference keeps the object alive, and the framework releases what it holds at shutdown.

**Set the publication-rejection callback before you add.** `add` answers immediately when the name
is already in use by the same source. It logs that, skips the instance and returns `false`. A
collision with another component in this process, or with another kernel across the network, is
settled once `add` has already returned, and reaches you through that callback. It runs on the
object's owning thread, like everything else here.

## When your code throws

How Sen treats an exception depends on how your code was reached.

**Where the framework delivers something to you, a throw becomes an error somebody receives.** A
method implementation is the clear case. If it throws, the exception is captured and handed to the
caller as the call's failure, so the process keeps running and the caller finds out. This is the one
place in Sen where throwing is a supported way to fail, and a method that cannot do its job may use
it instead of inventing its own error channel. Deferred work is the same: each item runs guarded, a
throwing one is counted and reported, and the items queued behind it still run.

**Where the runner calls you on its own cycle, a throw ends the process.** That covers `run`,
`update`, `registered`, `unregistered`, `preDrain`, `preCommit`, and also property-changed and event
callbacks. The exception leaves the component's thread and the process terminates. Sen's crash
reporter takes over at that point and records what happened: the exception's message, its kind, and
a stack trace.

So, for a system that must keep running: **do not throw out of a hook.** Return an error, or handle
it where it happens. The exception is a method implementation, where throwing is handled by design.

A hook that *returns* an error is treated separately during startup. The kernel logs the component
and the explanation, dumps a backtrace, and stops the process. A failure while the kernel is coming
up is handled differently again: what already started is torn down, and the original error is
reported to whatever asked for the kernel, so a failed start does not leave half a system running.

### What survives a process boundary

The exception itself cannot cross a process boundary. What travels is a category and a message, and
the receiving side builds a new exception from them: a `std::logic_error`, `std::invalid_argument`
or `std::runtime_error` carrying your message. Anything outside those categories arrives as a bare
`std::exception` with no message. So a remote caller can act on the failure and usually read its
text, and cannot switch on your exception type.

## Every call gets an answer

A call that carries a callback is always answered, exactly once. If the call runs, it answers. If
whatever was carrying it is destroyed first, the destructor answers. The obligation belongs to the
object holding the call and moves with it, so there is no path where the call quietly disappears.

A call with no callback is a silent no-op, since there is nobody to answer. Otherwise the guarantee
covers local and remote calls alike.

When the answer is a failure it arrives as a `MethodCallError`, and the reason says which case you
met:

| Reason | What happened |
| ------ | ------------- |
| `componentStopped` | The target had stopped accepting work. |
| `objectNoLongerExists` | The target object was already gone when the call was sent. |
| `connectionLost` | The transport or connection died under a call already in flight. |
| `methodCallTimeout` | The peer is alive but did not answer a best-effort call. |
| `callDropped` | The item died while the target was still accepting work. |

Inside a single process you are unlikely to meet `callDropped`. The inbound mailboxes do not drop,
so nothing falls to a queue bound, and `componentStopped` is the case that fires.

**Every call in flight toward a lost connection is answered once**, whether the peer died, the
session was torn down or the call was cancelled. Those answers are exempt from queue bounds, so an
answer cannot be lost on its way home at a caller queue that happens to be full. Your own teardown
answers too: dropping a subscription or a source while a call is in flight fires the callback once
with `connectionLost`.

### When a callback stops being called

A callback stops when you cancel it, or when the object that owns it is destroyed. That owner is the
one named in the registration token, the `this` in `{this, callback}`. The callback holds a weak
reference to it, so registering a callback does not keep your object alive.

Sen checks the owner is still there and delivers under the same lock, so a callback cannot pass the
check and then fire into an object that has gone in the meantime.

This is the one exception to the guarantee above. **If the caller itself is gone, nothing fires**,
because the answering path makes the same check before it posts. There is nobody left to tell.

## Shutdown

At shutdown the framework releases every component's objects and proxies **before any component's
destructor runs**. Before you write a destructor or an unload hook, know that you may read your own
objects there and you may not read a peer's, because a peer is being released in the same phase.
Breaking this is diagnosable, because what you meet is a released guard.

The same ordering is why you should release the sources and subscriptions you hold before the kernel
goes away, instead of relying on your own destruction order to do it.
