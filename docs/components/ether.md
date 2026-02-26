![Screenshot](../assets/images/ether_light.svg#only-light){: style="width:250px; float: right;"}
![Screenshot](../assets/images/ether_dark.svg#only-dark){: style="width:250px; float: right;"}

# The Ether Transport

If you load the ether component, your process will be able to communicate with other processes.

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

## Queue sizes

If the components are pumping more data than the I/O stack can handle, your system will eventually
run out of memory. Therefore, it is advised to set some maximum size to the queues, and pay
attention to warning raised by the warningLevel parameter when the system is under heavy load.

## Isolating communication

If you are sharing a network with other Sen instances that are unrelated to your project, you can
set the `discovery.port` parameter to some predefined value used by the Sen instances that you want
to keep isolated from the rest.

If changing the YAML file is not possible or convenient, this parameter can be overwritten by using
the `SEN_ETHER_DISCOVERY_PORT` environment variable.

## Working with containers

Sen uses the process PID and host name to identify participants in the network. If running a
container, remember to:

- Mount your host `/etc/hostname` into the container's `/etc/hostname`
- Use the `--pid=host` command line flag (or the `pid: "host"` in your Docker Compose configuration)
- Consider using the `--network=host` command line flag (or `network: host` in your Docker Compose
  configuration)

## Network interfaces

The ether component needs a working network interface enabled with IPv4, and multicast. If you don't
have any network interface, please ensure that you have the loop-back interface enabled and with
multicast support.

### Ensuring that you are using valid interfaces

If there are no network interfaces, the ether component will not start.

By default, the ether component ignores virtual interfaces (interfaces that have the NO_CARRIER
flag). This is useful when working with containers, but might be inconvenient when working with
virtual machines.

You can enable virtual interfaces by setting the `discovery.allowVirtualInterfaces` parameter to
`true`, or by setting the `SEN_ETHER_ALLOW_VIRTUAL_INTERFACES` environment variable to `true` (or
`yes`, or a number != `0`).

### Ensuring that multicast is enabled

```bash
sudo ip link set lo multicast on
sudo ip route add 224.0.0.0/4 dev lo
sudo ip route add ff00::/8 dev lo
```

If you have other interfaces, use `eth0` (or your preferred interface) instead of `lo`.

Remember to also do it if you are inside a Docker container (use `--cap-add=NET_ADMIN` and `eth0`).

## Controlling Multicast

Sen uses multicast to distribute information to multiple receivers with minimum overhead. The
multicast groups are generated based on an internal algorithm. In some cases, you might need to
deploy Sen applications in a context where multicast support is limited.

### Setting the Network Interface

You can set the `networkDevice` attribute to force Sen to use a particular network interface. If
set, the Ether component will route all the traffic through it. To know which devices can be used,
you can execute the `ip a` command (in Linux). Typically, the names will be along the lines of `lo`,
`eth0`, or similar.

The Take into account that:

- The interface must be UP.
- If you use multicast, multicast needs to be enabled.
- You may need to configure your OS IP routing.

### Setting multicast ranges

The `busConfig.multicastRange` configuration parameter defines the ranges for possible multicast
groups to be used by the buses. The default range of addresses is 224.0.0.0 to 239.255.255.255. In
order for this to work, you need to ensure that all the related Sen applications are using the same
range.

### Disabling multicast for bus traffic

You can force Sen to use TCP. The `busConfig.multicastDisabled` configuration parameter can be used
for this purpose. You can also set the `SEN_ETHER_DISABLE_BUS_MULTICAST` environment variable with
`true` (or `yes`, or a number != `0`). TCP is resilient to message drops or re-ordering, but it does
not scale in regard to the number of participants, as emitters will have to send dedicated messages
to all receivers.

### Disabling multicast entirely

By default, Sen relies on multicast for peers to discover each other. If your infrastructure does
not allow multicast traffic at all, you will need to disable it for bus traffic (see previous
section) and enable the **TCP-based discovery hub**.

The TCP discovery hub is a process that Sen applications connect to in order to discover each other.
You only need one.

Starting a hub is easy. You just need to tell the ether service to start it on a given port.

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

Connecting to a hub is also easy. You need to set it in the discovery configuration:

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

The BeamTracker is then responsible for the detection of beams, allowing different processes to
discover themselves. The BeamTracker uses a parameter called `beamExpirationTime` to determine when
a beam is no longer being received at the expected frequency, at which point it is assumed lost. By
default, this parameter is set to 3 times the value of the `beamPeriod`, and this could be
problematic in cases where the `beamPeriod` is configured to small values (e.g. 100 ms). Therefore,
the `beamExpirationTime` can be configured by the user in the following ways:

- Configuring the `beamExpirationTime` parameter of the `DiscoveryConfig` to the desired duration.
- Setting the `BEAM_TRACKER_EXPIRATION_TIME_MS` environment variable to the desired duration in
  milliseconds.

## UDP OS Buffer Sizes

Some OSes (most notably, Linux) place very restrictive limits on the performance of UDP protocols.
It is highly recommended that you increase these OS limits to at least 8MB before trying to run
large amounts of UDP traffic to your instance. 8MB is just a recommendation, and can be adjusted
higher.

### Linux

Check the current UDP/IP receive buffer limit & default by typing the following commands:

```
sysctl net.core.rmem_max
sysctl net.core.rmem_default
```

If the values are less than 8388608 bytes you should add the following lines to the /etc/sysctl.conf
file:

```
net.core.rmem_max=8388608
net.core.rmem_default=8388608
```

Changes to /etc/sysctl.conf do not take effect until reboot. To update the values immediately, type
the following commands as root:

```
sysctl -w net.core.rmem_max=8388608
sysctl -w net.core.rmem_default=8388608
```

### BSD/Darwin

On BSD/Darwin systems you need to add about a 15% padding to the kernel limit socket buffer. Meaning
if you want an 8MB buffer (8388608 bytes) you need to set the kernel limit to 8388608\*1.15 =
9646900\. This is not documented anywhere but happens in the kernel here.

Check the current UDP/IP buffer limit by typing the following command:

```
sysctl kern.ipc.maxsockbuf
```

If the value is less than 9646900 bytes you should add the following lines to the /etc/sysctl.conf
file (create it if necessary):

```
kern.ipc.maxsockbuf=9646900
```

Changes to /etc/sysctl.conf do not take effect until reboot. To update the values immediately, type
the following command as root:

```
sysctl -w kern.ipc.maxsockbuf=9646900
```
