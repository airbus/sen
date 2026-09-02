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

## How long a reference lasts

`RunApi&` and `RegistrationApi&` belong to your component's runner, and every hook receives the same
ones. Use them within the life of your component, do not put one anywhere it could outlive the
component, and never use one from another thread.

## Publishing an object

`add` hands the kernel a reference that it keeps. The object stays alive for as long as the kernel
holds that reference, whatever happens to your own pointer, and `remove` gives the reference back.
You are not required to call `remove` before dropping your last reference. The kernel's own
reference keeps the object alive, and the framework releases what it holds at shutdown.

## When your code throws

How Sen treats an exception depends on how your code was reached.

**Where the framework delivers something to you, a throw becomes an error somebody receives.** A
method implementation is the clear case. If it throws, the exception is captured and handed to the
caller as the call's failure, so the process keeps running and the caller finds out. This is the one
place in Sen where throwing is a supported way to fail, and a method that cannot do its job may use
it instead of inventing its own error channel.

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

## When a callback stops being called

A callback stops when you cancel it, or when the object that owns it is destroyed. That owner is the
one named in the registration token, the `this` in `{this, callback}`. The callback holds a weak
reference to it, so registering a callback does not keep your object alive.

Sen checks the owner is still there and delivers under the same lock, so a callback cannot pass the
check and then fire into an object that has gone in the meantime.

**If the caller itself is gone, nothing fires**, because the answering path makes the same check
before it posts. There is nobody left to tell.
