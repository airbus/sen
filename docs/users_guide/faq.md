# FAQ

## Networking

---

> Why does Sen use multicast?

: When you have n participants, multicast allows for much better scaling. When producers send
messages to multiple consumers, multicast relies on the router hardware to do it fast and
efficiently. The producer does not have to allocate n buffers and send n messages, and the network
does not get loaded with n messages that contain the same information. It differs from broadcast in
the sense that it also prevents unwanted traffic from reaching networks where there are no
consumers. Multicast also serves as a way to isolate and segregate data flows that have to be
logically or physically separated in some contexts.

> Can I force the usage of TCP?

: Yes. You can force bus communication to be done over TCP by configuring the ether component in
your yaml file, or by setting the `SEN_ETHER_DISABLE_BUS_MULTICAST` environment variable to `true`.
Remember to do so in all the involved Sen instances.

> What if the infrastructure does not allow me to use Multicast?

: Apart from forcing the usage of TCP for bus communication, you need to change the way in which Sen
instances discover each other. You can do that by setting the `discovery` configuration field of the
`ether` component to `TcpDiscovery`. This mechanism works by instantiating a "discovery hub" that
Sen instances connect to in order to know about each other's existence. Have a look at the
documentation of the ether component for more information and some examples.

> Can messages be lost when using a non-confirmed quality of service?

: In theory, yes. In practice, it is unlikely in most cases. UDP (multicast or unicast) does
not offer any guarantee in terms of confirmation of reception and order of delivery. That said, when
working in Local Area Networks (which is the typical case for Sen applications), it is very hard to
lose a UDP datagram. You would need to have a very loaded network, a faulty firewall or some sort of
infrastructural problem that would cause it.

> Do I need a network to run my Sen-based application?

: If you don't load the ether component, all the sessions and buses are local.

## WSL2

---

> Can I use Sen with WSL2?

: Yes, with some networking limitations. Configure WSL2 to use mirrored networking, ensure the
selected interface and firewalls allow multicast, and raise the open-file limit. Communication has
been verified with remote Linux and Windows hosts, but not between WSL2 and the Windows host running
it. See [Networking with WSL2 and virtual machines](../howto_guides/networking_wsl2_virtual_machines.md)
for the complete setup and the TCP-only fallback.
