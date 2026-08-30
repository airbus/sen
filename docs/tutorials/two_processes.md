# Tutorial 3: Two processes talking

**What you will learn**

- How to put two Sen processes on the same bus
- What you have to change in your code to go from one process to two (nothing)
- How discovery works, and what "it behaves like a local object" does and does not mean

**Prerequisites:** [Tutorial 2: Two objects talking](two_objects.md). We reuse its calculator, so it
helps to have run it.

---

## What we're building

The same calculator class from Tutorial 2, but the object lives in one process and you call it from
another. This is the point of a distributed system, and it is the smallest possible demonstration of
it.

---

## Step 1: Run the calculator with a transport

Sen's [kernel](../users_guide/glossary.md#kernel) does not talk to the network on its own. The
`ether` component is what carries objects between processes, and adding it is a configuration
change, not a code change.

The calculators example ships exactly this config:

```yaml title="config/1_calculators/2_calculators_eth.yaml"
--8<-- "examples/config/1_calculators/2_calculators_eth.yaml"
```

That is the whole difference from the single-process config: it includes the same
`1_calculators.yaml` you already ran, plus `ether`.

```yaml title="config/base/ether.yaml"
--8<-- "examples/config/base/ether.yaml"
```

Run it, and leave it running:

```shell
sen run config/1_calculators/2_calculators_eth.yaml
```

---

## Step 2: Join from a second process

In another terminal:

```shell
sen shell
```

`sen shell` is more than a prompt. It is a complete Sen kernel with two components loaded, `ether`
and `shell`. That is why it can see anything at all: it is another kernel on the same network, not a
client connecting to the first process.

It starts with only `local.kernel` open, so tell it which bus you want:

```text
open my.tutorial
```

Now list what the shell can see. The objects from the *other* process are there: `goodCalc` and
`badCalc`, the two calculators the first config instantiated:

```text
ls
```

Those are the calculators example's names, not Tutorial 2's `calc1`. `goodCalc` is the same
`CasioCalculator` class behind `calc1`; `badCalc` is a `FaultyCalculator`, there to fail on purpose.

Call one, and read a property:

```text
my.tutorial.goodCalc.add 10, 5
my.tutorial.goodCalc.getCurrent
```

---

## What just happened

**Nothing in the calculator's code changed.** `casio_calculator.cpp` is byte-for-byte the same file
Tutorial 2 used. It does not know whether the caller is in its own process, in another process, or
on another machine. That is the property the whole design is built around, and it is why [design
considerations](../howto_guides/considerations.md) argues you should defer the "how many processes?"
question instead of answering it early.

**The two kernels found each other without being told about each other.** Neither config contains
the other's address. Sen is broker-less: `ether` announces what a kernel has and listens for what
others have, and interests are matched against those announcements. Nothing had to start first.

**The bus name is what connected them.** `my.tutorial` is a namespace, and both processes named the
same one. A bus is not a security boundary or a channel someone has to create in advance. It is an
agreed string, and agreeing on it is the whole handshake.

---

## What "behaves like a local object" does not mean

The calculator behaves identically from your code's point of view. Underneath it does not, and the
differences below reach you in practice.

**It is not faster than the cycle.** A remote call still lands in the callee's next drain, so the
result arrives a cycle or two later, the same shape you already saw in Tutorial 2. The network adds
its own transit time on top of that, which on a local network is small next to a cycle and over a
longer link may not be. The asynchronous handling you learned does not change; what changes is how
much slack is left inside a cycle.

**The network has to allow it.** `ether` discovers over UDP multicast by default. On a network that
drops multicast between hosts you will see nothing and get no error, because a kernel that never
announces is indistinguishable from one that does not exist. [The networking
FAQ](../users_guide/faq.md) covers that case and the TCP discovery-hub alternative for when
multicast is unavailable.

**Objects vanish with their owner.** Stop the first process and watch the shell: `goodCalc` and
`badCalc` disappear within a few seconds. Sen removes every object owned by a component that goes
away, so a subscriber sees them leave and does not watch them freeze. If you hold a reference across
cycles, check it is still there.

---

## Next

- [Design considerations](../howto_guides/considerations.md): when splitting into processes is worth
  it, and what it costs.
- [Ether](../components/ether.md): discovery, multicast ranges, interface selection, and the TCP
  discovery hub.
- [The examples](../examples/index.md): `4_school` runs the same idea with two classrooms across two
  processes, and `11_shapes` adds interest filtering so a subscriber sees only part of what is
  published.
