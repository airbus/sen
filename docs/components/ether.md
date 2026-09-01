![Screenshot](../assets/images/ether_light.svg#only-light){: style="width:250px; float: right;"}
![Screenshot](../assets/images/ether_dark.svg#only-dark){: style="width:250px; float: right;"}

# The ether transport

If you load the ether [component](../users_guide/glossary.md#component), your process will be able
to communicate with other processes.

```yaml
load:
  - name: ether
    group: 1
```

Sen processes discover each other using multicast. Normally, you don't need to change the default
discovery parameters, but here you can see the available options:

```rust title="Ether component configuration options"
--8<-- "snippets/ether_config.stl"
```

If you arrive from another distributed system, the word you are reaching for here is *peer*. Sen
calls it a **remote kernel**: the kernel in another process that this one has found, one per
process. That is the term used in this documentation. *Peer* is kept only for
the shape of the network: Sen's transport is peer-to-peer, with no central server or broker to
connect to.

## Queue sizes

If the components are pumping more data than the I/O stack can handle, your system will eventually
run out of memory. So cap the queues, and watch the warnings `warningLevel` raises under load.

## Isolating communication

If you are sharing a network with other Sen instances that are unrelated to your project, you can
set the discovery port to a value shared only by the Sen instances you want to keep together.
`discovery` is a variant, so it takes a `type` and a `value`:

```yaml
load:
  - name: ether
    discovery:
      type: MulticastDiscovery
      value:
        port: 60544
```

A flat `discovery.port:` is not an error and is not applied. The parser needs the `type` key to know
which alternative you mean, and without it the whole block is ignored and the default port stands.

If changing the YAML file is not possible or convenient, this parameter can be overwritten by using
the `SEN_ETHER_DISCOVERY_PORT` environment variable.

## Working with containers

Sen uses the host name to identify participants in the network. If running a container, remember to
set a host name so that Sen gets the right picture about where things are running (you can do it
with the `--hostname` option).

If you are using the TCP Discovery mechanism in a bridged network, remember to set `hubAddress.host`
in the discovery configuration to the host name or IP address of the container hosting the hub.

## Network interfaces

The ether component needs a working network interface enabled with IPv4, and multicast. If you don't
have any network interface, please ensure that you have the loop-back interface enabled and with
multicast support.

### Ensuring that you are using valid interfaces

If there are no network interfaces, the ether component will not start.

By default, the ether component ignores virtual interfaces (interfaces that have the NO_CARRIER
flag). This is useful when working with containers, but might be inconvenient when working with
virtual machines.

You can enable virtual interfaces with `allowVirtualInterfaces`, nested inside the discovery
variant as above, or by setting the `SEN_ETHER_ALLOW_VIRTUAL_INTERFACES` environment variable to
`true` (or `yes`, or a number != `0`). The environment variable is the simpler of the two.

### Ensuring that multicast is enabled

```bash
sudo ip link set lo multicast on
sudo ip route add 224.0.0.0/4 dev lo
sudo ip route add ff00::/8 dev lo
```

If you have other interfaces, use `eth0` (or your preferred interface) instead of `lo`.

Remember to also do it if you are inside a Docker container (use `--cap-add=NET_ADMIN` and `eth0`).

## Controlling multicast

Sen uses multicast to distribute information to multiple receivers with minimum overhead. The
multicast groups are generated based on an internal algorithm. In some cases, you might need to
deploy Sen applications in a context where multicast support is limited.

### Setting the network interface

You can set the `networkDevice` attribute to force Sen to use a particular network interface. If
set, the ether component will route all the traffic through it. To know which devices can be used,
you can execute the `ip a` command (in Linux). Typically, the names will be along the lines of `lo`,
`eth0`, or similar.

Take into account that:

- The interface must be UP.
- If you use multicast, multicast needs to be enabled.
- You may need to configure your OS IP routing.

### Setting multicast ranges

The `busConfig.multicastRange` configuration parameter defines the ranges for possible multicast
groups to be used by the buses. The default range of addresses is `239.192.0.0` to
`239.195.255.255`, which follows the Organization-Local scope defined in RFC 2365. In
order for this to work, you need to ensure that all the related Sen applications are using the same
range.

### Disabling multicast for bus traffic

You can force Sen to use TCP. The `busConfig.multicastDisabled` configuration parameter can be used
for this purpose. You can also set the `SEN_ETHER_DISABLE_BUS_MULTICAST` environment variable with
`true` (or `yes`, or a number != `0`). TCP is resilient to message drops or re-ordering, but it does
not scale in regard to the number of participants, as emitters will have to send dedicated messages
to all receivers.

### Disabling multicast entirely

By default, Sen relies on multicast for kernels to discover each other. If your infrastructure does
not allow multicast traffic at all, you will need to disable it for bus traffic (see previous
section) and enable the **TCP-based discovery hub**.

The TCP discovery hub is a process that Sen applications connect to in order to discover each other.
You only need one.

Tell the ether component to run one, and give it a port.

```yaml
load:
  - name: ether
    runDiscoveryHub: 64222     # if you set this, we will run a discovery hub at this port
```

If you want to fully disable multicast you would need something like this:

```yaml
load:
  - name: ether
    busConfig:
      multicastDisabled: true  # no multicast bus traffic
    runDiscoveryHub: 64222     # if you set this, we will run a discovery hub at this port
    discovery:
      type: TcpDiscovery
      value:
        hubAddress:
          host: localhost
          port: 64222           # has to match with the port where the hub will be running
```

Point the discovery configuration at it:

```yaml
load:
  - name: ether
    busConfig:
      multicastDisabled: true  # no multicast bus traffic
    discovery:
      type: TcpDiscovery
      value:
        hubAddress:
          host: theComputer  # this can also be an IPv4 address
          port: 64222        # has to match with the port where the hub will be running
```

### Configuring the discovery expiration time

Different ether instances find each other using a beamer that broadcasts beam messages periodically.
The period at which beams are sent can be configured in the `DiscoveryConfig` of the ether
configuration by modifying the `beamPeriod`. The default value of this period is 1 second.

The BeamTracker watches for those beams and uses `beamExpirationTime` to decide when one has stopped
arriving at the expected frequency, at which point it is assumed lost. It defaults to three times
`beamPeriod`, which is too tight once you set the period low, 100 ms say, so you can set it yourself
in either of these ways:

- Configuring the `beamExpirationTime` parameter of the `DiscoveryConfig` to the desired duration.
- Setting the `BEAM_TRACKER_EXPIRATION_TIME_MS` environment variable to the desired duration in
  milliseconds.

Until a beam expires, the peer's objects are still there, holding the values from the last update
that arrived, and nothing marks them stale. When it expires the peer is treated as gone and its
objects are removed like any other departure, as
[Main concepts](../users_guide/main_concepts.md) describes.

So `beamExpirationTime` is the window in which a model can read values from a machine that is no
longer there. Shortening it narrows the window and makes a brief stall more likely to be read as a
departure.

## UDP OS buffer sizes

Some operating systems, most notably Linux, place restrictive limits on socket buffer sizes. Raise
them to at least 8MB before pushing large amounts of UDP traffic through your instance. 8MB is a
starting point rather than a ceiling, and can go higher.

### Linux

Check the current UDP/IP receive buffer limit & default by typing the following commands:

```shell
sysctl net.core.rmem_max
sysctl net.core.rmem_default
```

If the values are less than 8388608 bytes you should add the following lines to the /etc/sysctl.conf
file:

```text
net.core.rmem_max=8388608
net.core.rmem_default=8388608
```

Changes to /etc/sysctl.conf do not take effect until reboot. To update the values immediately, type
the following commands as root:

```shell
sysctl -w net.core.rmem_max=8388608
sysctl -w net.core.rmem_default=8388608
```
