# Networking FAQ

______________________________________________________________________

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
Sen instances connect to in order to know about each other existence. Have a look at the
documentation of the ether component for more information and some examples.

> Can messages be lost when using a non-confirmed quality of service?

: In theory, yes. In practice, it is unlikely in most of the cases. UDP (multicast or unicast) does
not offer any guarantee in terms of confirmation of reception and order of delivery. That said, when
working in Local Area Networks (which is the typical case for Sen applications), it is very hard to
lose a UDP datagram. You would need to have a very loaded network, a faulty firewall or some sort of
infrastructural problem that would cause it.

> Do I need a network to run my Sen-based application?

: If you don't load the ether component, all the sessions and buses are local.

# WSL2 FAQ

______________________________________________________________________

> Can I use Sen with WSL2?

: Yes, you can use it but WSL2 imposes restrictive user hard limits that cannot be changed with
`ulimit`. Only workaround for now is to run from your shell `su [your_username]` before starting
Sen. You can find more info about this in this
[WSL issue](https://github.com/microsoft/WSL/issues/6564)
