# Tutorials

Worked examples, in order. Each builds something small and runs it, and each assumes you have
already been through [Install](../getting_started/install.md).

**[Tutorial 1: Hello Sen](hello_sen.md)** builds one object that counts, publishes it, and lets you
watch it change from the shell. It is the shortest path from an empty directory to a running Sen
system, and it is the page to send someone who has never seen Sen before.

**[Tutorial 2: Two objects talking](two_objects.md)** adds a second object and has the two of them
call each other. This is where the asynchronous behavior becomes visible: you call a method, the
result arrives a cycle or two later, and the callback is how you receive it.

**[Tutorial 3: Two processes talking](two_processes.md)** takes the same calculator and puts the
caller in a separate process. Your code does not change at all, because the transport is a line of
configuration. This is where discovery, and what happens when a process goes away, become
visible.

If a term is unfamiliar, the [glossary](../users_guide/glossary.md) is the fastest way to settle it.

[Create your first package](../getting_started/first_package.md) covers the same ground as
reference: `sen package init` generates the skeleton, and the page walks through what each generated
file is for.

After these, the [examples](../examples/index.md) are a graded set of working packages you can read
and run, and the [how-to guides](../howto_guides/index.md) answer specific questions.
