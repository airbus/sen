#import "style.typ": sen-table, facts, declared, kw, lit, summary, chip, section, prose, index-of, hierarchy, packages, enum-columns, mono, COL-NAME, COL-TYPE, COL-FLAGS

= Model overview

#prose("335 types in 13 packages. 13 classes in 11 independent hierarchies.")

*Packages*

#packages(8,
  ([*Package*], [*Types*], [*Object classes*], [*Fixed records*], [*Enumerations*], [*Variant records*], [*Arrays*], [*Optionals*]),
  ([#h(0pt)#text(fill: luma(120))[#"components"]], [—], [—], [—], [—], [—], [—], [—]),
  ([#h(10pt)#"ether"], [31], [—], [20], [1], [3], [4], [3]),
  ([#h(10pt)#"explorer"], [1], [—], [1], [—], [—], [—], [—]),
  ([#h(10pt)#"influx"], [3], [—], [1], [1], [—], [1], [—]),
  ([#h(10pt)#"jsonrpc"], [41], [2], [17], [—], [1], [8], [13]),
  ([#h(10pt)#"logmaster"], [3], [2], [1], [—], [—], [—], [—]),
  ([#h(10pt)#"py"], [3], [1], [1], [—], [—], [1], [—]),
  ([#h(10pt)#"recorder"], [6], [1], [2], [1], [—], [2], [—]),
  ([#h(10pt)#"replayer"], [4], [2], [1], [1], [—], [—], [—]),
  ([#h(10pt)#"rest"], [37], [—], [20], [5], [—], [9], [3]),
  ([#h(10pt)#"shell"], [27], [1], [20], [3], [1], [2], [—]),
  ([#h(0pt)#"db"], [3], [—], [3], [—], [—], [—], [—]),
  ([#h(0pt)#"kernel"], [160], [4], [80], [23], [8], [37], [8]),
  ([#h(10pt)#"log"], [16], [—], [11], [1], [1], [3], [—]),
)

*Class hierarchy across all packages*

#hierarchy(
  ("", link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"components.​jsonrpc.​Json​Rpc​Server"], <t-sen-components-jsonrpc-JsonRpcServer>),
  ("", link(<t-sen-components-jsonrpc-StaticFileServer>)[#"components.​jsonrpc.​Static​File​Server"], <t-sen-components-jsonrpc-StaticFileServer>),
  ("", link(<t-sen-components-logmaster-LogMaster>)[#"components.​logmaster.​Log​Master"], <t-sen-components-logmaster-LogMaster>),
  ("", link(<t-sen-components-logmaster-Logger>)[#"components.​logmaster.​Logger"], <t-sen-components-logmaster-Logger>),
  ("", link(<t-sen-components-py-PythonInterpreter>)[#"components.​py.​Python​Interpreter"], <t-sen-components-py-PythonInterpreter>),
  ("", link(<t-sen-components-recorder-Recorder>)[#"components.​recorder.​Recorder"], <t-sen-components-recorder-Recorder>),
  ("", link(<t-sen-components-replayer-Replay>)[#"components.​replayer.​Replay"], <t-sen-components-replayer-Replay>),
  ("", link(<t-sen-components-replayer-Replayer>)[#"components.​replayer.​Replayer"], <t-sen-components-replayer-Replayer>),
  ("", link(<t-sen-components-shell-Shell>)[#"components.​shell.​Shell"], <t-sen-components-shell-Shell>),
  ("", link(<t-sen-kernel-KernelApi>)[#"kernel.​Kernel​Api"], <t-sen-kernel-KernelApi>),
  ("", link(<t-sen-kernel-VirtualClock>)[#"kernel.​Virtual​Clock"], <t-sen-kernel-VirtualClock>),
  ("├── ", link(<t-sen-kernel-VirtualKernelClock>)[#"kernel.​Virtual​Kernel​Clock"], <t-sen-kernel-VirtualKernelClock>),
  ("└── ", link(<t-sen-kernel-VirtualMasterClock>)[#"kernel.​Virtual​Master​Clock"], <t-sen-kernel-VirtualMasterClock>),
)

= Reading the tables

#prose[Every property carries three flags, in this order.]

#sen-table(
  columns: (COL-FLAGS, 1fr),
  table.header([*Flag*], [*Meaning*]),
  [#mono[RO]], [Read-only: the value is published by whoever owns it and cannot be set from outside.],
  [#mono[RW]], [Read-write: the value can be set as well as read.],
  [#mono[D]], [Dynamic: the value changes over the life of an instance.],
  [#mono[S]], [Static: the value is fixed once an instance exists.],
  [#mono[C]], [Confirmed: delivery is acknowledged, and the update is retried until it arrives.],
  [#mono[BE]], [Best effort: the update is sent once and may be lost.],
)

= #"components"

== #"ether"

#prose[31 types.]

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-ether-BusConfig>)[#"Bus​Config"], "Remember to set these to the same value in all related Sen instances", <t-sen-components-ether-BusConfig>),
  (link(<t-sen-components-ether-BusJoined>)[#"Bus​Joined"], "", <t-sen-components-ether-BusJoined>),
  (link(<t-sen-components-ether-BusLeft>)[#"Bus​Left"], "", <t-sen-components-ether-BusLeft>),
  (link(<t-sen-components-ether-ByteRange>)[#"Byte​Range"], "Range of values of a u8", <t-sen-components-ether-ByteRange>),
  (link(<t-sen-components-ether-Configuration>)[#"Configuration"], "Overall component configuration", <t-sen-components-ether-Configuration>),
  (link(<t-sen-components-ether-DiscoveryHubAddress>)[#"Discovery​Hub​Address"], "TCP-based discovery hub parameters", <t-sen-components-ether-DiscoveryHubAddress>),
  (link(<t-sen-components-ether-Endpoint>)[#"Endpoint"], "", <t-sen-components-ether-Endpoint>),
  (link(<t-sen-components-ether-Ephemeral>)[#"Ephemeral"], "Let the operating system select an ephemeral port", <t-sen-components-ether-Ephemeral>),
  (link(<t-sen-components-ether-Hello>)[#"Hello"], "", <t-sen-components-ether-Hello>),
  (link(<t-sen-components-ether-MulticastAddressRange>)[#"Multicast​Address​Range"], "Inclusive IPv4 multicast address range", <t-sen-components-ether-MulticastAddressRange>),
  (link(<t-sen-components-ether-MulticastDiscovery>)[#"Multicast​Discovery"], "Multicast-based discovery parameters", <t-sen-components-ether-MulticastDiscovery>),
  (link(<t-sen-components-ether-PinnedPort>)[#"Pinned​Port"], "Bind exactly this port, fail closed if taken", <t-sen-components-ether-PinnedPort>),
  (link(<t-sen-components-ether-PortConfig>)[#"Port​Config"], "This configuration is per process and does not need to match across Sen instances. Exclusions apply to a pinned port, not to an ephemeral one: an ephemeral bind asks the OS for any free port and does not learn which it was given. udpUnicast cannot be pinned. Its socket is per peer and UDP has no four-tuple to demultiplex on, so a second peer either fails to bind or silently takes the first peer's traffic.", <t-sen-components-ether-PortConfig>),
  (link(<t-sen-components-ether-PortRange>)[#"Port​Range"], "Inclusive port range", <t-sen-components-ether-PortRange>),
  (link(<t-sen-components-ether-ProbePortRange>)[#"Probe​Port​Range"], "Bind within [min,max], random start, walk forward", <t-sen-components-ether-ProbePortRange>),
  (link(<t-sen-components-ether-ProtocolVersion>)[#"Protocol​Version"], "", <t-sen-components-ether-ProtocolVersion>),
  (link(<t-sen-components-ether-QueueConfig>)[#"Queue​Config"], "", <t-sen-components-ether-QueueConfig>),
  (link(<t-sen-components-ether-Ready>)[#"Ready"], "", <t-sen-components-ether-Ready>),
  (link(<t-sen-components-ether-SessionPresenceBeam>)[#"Session​Presence​Beam"], "", <t-sen-components-ether-SessionPresenceBeam>),
  (link(<t-sen-components-ether-TcpDiscovery>)[#"Tcp​Discovery"], "TCP-based discovery parameters", <t-sen-components-ether-TcpDiscovery>),
)

==== #"BusConfig" #chip("structures", "structure") <t-sen-components-ether-BusConfig>
#prose("Remember to set these to the same value in all related Sen instances")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("multicast​Range")], link(<t-sen-components-ether-MulticastRange>)[#"Multicast​Range"], "Defaults to the Organization Local scope (RFC 2365): [239], [192-195], [0-255], [0-255] Remember to keep a range wide enough for avoiding collisions if you have many buses.",
  [#mono("multicast​Exclusions")], link(<t-sen-components-ether-MulticastAddressExclusions>)[#"Multicast​Address​Exclusions"], "Address ranges that must not be allocated",
  [#mono("multicast​Port")], link(<t-u16>)[#"u16"], "Defaults to 50985 Change this in case you have limited control over the infrastructure.",
  [#mono("multicast​Disabled")], link(<t-bool>)[#"bool"], "If true, no multicast traffic is performed and all the bus settings above become meaningless. Defaults to false.",
)
#facts[Named by #link(<t-sen-components-ether-Configuration>)[#"Configuration"].]

==== #"BusJoined" #chip("structures", "structure") <t-sen-components-ether-BusJoined>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("participant​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("bus​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("bus​Name")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-components-ether-ControlMessage>)[#"Control​Message"].]

==== #"BusLeft" #chip("structures", "structure") <t-sen-components-ether-BusLeft>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("participant​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("bus​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("bus​Name")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-components-ether-ControlMessage>)[#"Control​Message"].]

==== #"ByteRange" #chip("structures", "structure") <t-sen-components-ether-ByteRange>
#prose("Range of values of a u8")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("min")], link(<t-u8>)[#"u8"], "minimum range",
  [#mono("max")], link(<t-u8>)[#"u8"], "needs to be >= min",
)
#facts[Named by #link(<t-sen-components-ether-MulticastRange>)[#"Multicast​Range"].]

==== #"Configuration" #chip("structures", "structure") <t-sen-components-ether-Configuration>
#prose("Overall component configuration")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("discovery")], link(<t-sen-components-ether-DiscoveryConfig>)[#"Discovery​Config"], "Discovery-related parameters",
  [#mono("bus​Out​Queue")], link(<t-sen-components-ether-QueueConfig>)[#"Queue​Config"], "Queue for broadcasting to buses",
  [#mono("process​Udp​Out​Queue")], link(<t-sen-components-ether-QueueConfig>)[#"Queue​Config"], "Queue for communicating with processes over UDP",
  [#mono("process​Tcp​Out​Queue")], link(<t-sen-components-ether-QueueConfig>)[#"Queue​Config"], "Queue for communicating with processes over TCP",
  [#mono("bus​Config")], link(<t-sen-components-ether-BusConfig>)[#"Bus​Config"], "Bus transport configuration",
  [#mono("port​Exclusions")], link(<t-sen-components-ether-PortExclusions>)[#"Port​Exclusions"], "Port ranges that this process must not use",
  [#mono("port​Config")], link(<t-sen-components-ether-PortConfig>)[#"Port​Config"], "If not present, all port bindings are Ephemeral",
  [#mono("run​Discovery​Hub")], link(<t-u16>)[#"u16"], "If present, run the TCP discovery hub on this port",
  [#mono("network​Device")], link(<t-string>)[#"string"], "If present, the network device to use",
)

==== #"DiscoveryHubAddress" #chip("structures", "structure") <t-sen-components-ether-DiscoveryHubAddress>
#prose("TCP-based discovery hub parameters")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("port")], link(<t-u16>)[#"u16"], "Port where the discovery hub is running",
  [#mono("host")], link(<t-string>)[#"string"], "name or IP of the host where the discovery hub is running",
)
#facts[Named by #link(<t-sen-components-ether-TcpDiscovery>)[#"Tcp​Discovery"].]

==== #"Endpoint" #chip("structures", "structure") <t-sen-components-ether-Endpoint>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("ip")], link(<t-u32>)[#"u32"], "",
  [#mono("port")], link(<t-u16>)[#"u16"], "",
)
#facts[Named by #link(<t-sen-components-ether-EnpointList>)[#"Enpoint​List"].]

==== #"Ephemeral" #chip("structures", "structure") <t-sen-components-ether-Ephemeral>
#prose("Let the operating system select an ephemeral port")
#facts[Named by #link(<t-sen-components-ether-PortBinding>)[#"Port​Binding"].]

==== #"Hello" #chip("structures", "structure") <t-sen-components-ether-Hello>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("info")], link(<t-sen-kernel-ProcessInfo>)[#"sen.​kernel.​Process​Info"], "",
  [#mono("udp​Port")], link(<t-u16>)[#"u16"], "",
  [#mono("version")], link(<t-sen-components-ether-ProtocolVersion>)[#"Protocol​Version"], "",
)
#facts[Named by #link(<t-sen-components-ether-ControlMessage>)[#"Control​Message"].]

==== #"MulticastAddressRange" #chip("structures", "structure") <t-sen-components-ether-MulticastAddressRange>
#prose("Inclusive IPv4 multicast address range")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("min")], link(<t-string>)[#"string"], "",
  [#mono("max")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-components-ether-MulticastAddressExclusions>)[#"Multicast​Address​Exclusions"].]

==== #"MulticastDiscovery" #chip("structures", "structure") <t-sen-components-ether-MulticastDiscovery>
#prose("Multicast-based discovery parameters")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("beam​Period")], link(<t-sen-Duration>)[#"sen.​Duration"], "How often are we beaming our presence",
  [#mono("beam​Expiration​Time")], link(<t-sen-Duration>)[#"sen.​Duration"], "Expiry time used by the beam tracker",
  [#mono("mcast​Group")], link(<t-string>)[#"string"], "Defaults to '239.255.0.44'",
  [#mono("port")], link(<t-u16>)[#"u16"], "Defaults to 60543",
  [#mono("device")], link(<t-string>)[#"string"], "Device to use for the network communication",
  [#mono("allow​Virtual​Interfaces")], link(<t-bool>)[#"bool"], "If true, virtual (NO_CARRIER) interfaces are also considered",
)
#facts[Named by #link(<t-sen-components-ether-DiscoveryConfig>)[#"Discovery​Config"].]

==== #"PinnedPort" #chip("structures", "structure") <t-sen-components-ether-PinnedPort>
#prose("Bind exactly this port, fail closed if taken")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("port")], link(<t-u16>)[#"u16"], "",
)
#facts[Named by #link(<t-sen-components-ether-PortBinding>)[#"Port​Binding"].]

==== #"PortConfig" #chip("structures", "structure") <t-sen-components-ether-PortConfig>
#prose("This configuration is per process and does not need to match across Sen instances. Exclusions apply to a pinned port, not to an ephemeral one: an ephemeral bind asks the OS for any free port and does not learn which it was given. udpUnicast cannot be pinned. Its socket is per peer and UDP has no four-tuple to demultiplex on, so a second peer either fails to bind or silently takes the first peer's traffic.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("tcp​Acceptor")], link(<t-sen-components-ether-PortBinding>)[#"Port​Binding"], "inbound TCP listener",
  [#mono("udp​Unicast")], link(<t-sen-components-ether-PortBinding>)[#"Port​Binding"], "per-peer UDP unicast; must be Ephemeral",
  [#mono("tcp​Source")], link(<t-sen-components-ether-PortBinding>)[#"Port​Binding"], "outbound TCP source port",
)
#facts[Named by #link(<t-sen-components-ether-Configuration>)[#"Configuration"].]

==== #"PortRange" #chip("structures", "structure") <t-sen-components-ether-PortRange>
#prose("Inclusive port range")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("min")], link(<t-u16>)[#"u16"], "",
  [#mono("max")], link(<t-u16>)[#"u16"], "",
)
#facts[Named by #link(<t-sen-components-ether-PortExclusions>)[#"Port​Exclusions"].]

==== #"ProbePortRange" #chip("structures", "structure") <t-sen-components-ether-ProbePortRange>
#prose("Bind within [min,max], random start, walk forward")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("min")], link(<t-u16>)[#"u16"], "",
  [#mono("max")], link(<t-u16>)[#"u16"], "",
)
#facts[Named by #link(<t-sen-components-ether-PortBinding>)[#"Port​Binding"].]

==== #"ProtocolVersion" #chip("structures", "structure") <t-sen-components-ether-ProtocolVersion>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("kernel")], link(<t-u32>)[#"u32"], "",
  [#mono("ether")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-components-ether-Hello>)[#"Hello"].]

==== #"QueueConfig" #chip("structures", "structure") <t-sen-components-ether-QueueConfig>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("eviction​Policy")], link(<t-sen-components-ether-QueueEvictionPolicy>)[#"Queue​Eviction​Policy"], "What to do when a queue is full",
  [#mono("max​Size")], link(<t-u64>)[#"u64"], "Maximum element count, 0 means unbounded",
  [#mono("warning​Level")], link(<t-u64>)[#"u64"], "Element count where warning will be issued, 0 means none",
)
#facts[Named by #link(<t-sen-components-ether-Configuration>)[#"Configuration"].]

==== #"Ready" #chip("structures", "structure") <t-sen-components-ether-Ready>
#facts[Named by #link(<t-sen-components-ether-ControlMessage>)[#"Control​Message"].]

==== #"SessionPresenceBeam" #chip("structures", "structure") <t-sen-components-ether-SessionPresenceBeam>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("protocol​Version")], link(<t-u16>)[#"u16"], "",
  [#mono("info")], link(<t-sen-kernel-ProcessInfo>)[#"sen.​kernel.​Process​Info"], "",
  [#mono("beam​Period")], link(<t-sen-Duration>)[#"sen.​Duration"], "",
  [#mono("endpoints")], link(<t-sen-components-ether-EnpointList>)[#"Enpoint​List"], "",
)

==== #"TcpDiscovery" #chip("structures", "structure") <t-sen-components-ether-TcpDiscovery>
#prose("TCP-based discovery parameters")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("beam​Period")], link(<t-sen-Duration>)[#"sen.​Duration"], "How often are we beaming our presence",
  [#mono("beam​Expiration​Time")], link(<t-sen-Duration>)[#"sen.​Duration"], "Expiry time used by the beam tracker",
  [#mono("hub​Address")], link(<t-sen-components-ether-DiscoveryHubAddress>)[#"Discovery​Hub​Address"], "Where to find the discovery hub",
  [#mono("allow​Virtual​Interfaces")], link(<t-bool>)[#"bool"], "If true, virtual (NO_CARRIER) interfaces are also considered",
)
#facts[Named by #link(<t-sen-components-ether-DiscoveryConfig>)[#"Discovery​Config"].]

#section("Enumerations", "enumerations")

#summary(
  (link(<t-sen-components-ether-QueueEvictionPolicy>)[#"Queue​Eviction​Policy"], "What to do when a queue is full", <t-sen-components-ether-QueueEvictionPolicy>),
)

==== #"QueueEvictionPolicy" #chip("enumerations", "enumeration") <t-sen-components-ether-QueueEvictionPolicy>
#prose("What to do when a queue is full")
#facts[Held as #mono("u32").]
#enum-columns(3,
  ("drop​Oldest", [0]),
  ("drop​Newest", [1]),
)
#facts[Named by #link(<t-sen-components-ether-QueueConfig>)[#"Queue​Config"].]

#section("Variant records", "variants")

#summary(
  (link(<t-sen-components-ether-ControlMessage>)[#"Control​Message"], "", <t-sen-components-ether-ControlMessage>),
  (link(<t-sen-components-ether-DiscoveryConfig>)[#"Discovery​Config"], "Configuration parameters related to the discovery protocol", <t-sen-components-ether-DiscoveryConfig>),
  (link(<t-sen-components-ether-PortBinding>)[#"Port​Binding"], "", <t-sen-components-ether-PortBinding>),
)

==== #"ControlMessage" #chip("variants", "variant") <t-sen-components-ether-ControlMessage>
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-components-ether-Hello>)[#"Hello"], "",
  link(<t-sen-components-ether-Ready>)[#"Ready"], "",
  link(<t-sen-components-ether-BusJoined>)[#"Bus​Joined"], "",
  link(<t-sen-components-ether-BusLeft>)[#"Bus​Left"], "",
)

==== #"DiscoveryConfig" #chip("variants", "variant") <t-sen-components-ether-DiscoveryConfig>
#prose("Configuration parameters related to the discovery protocol")
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-components-ether-MulticastDiscovery>)[#"Multicast​Discovery"], "",
  link(<t-sen-components-ether-TcpDiscovery>)[#"Tcp​Discovery"], "",
)
#facts[Named by #link(<t-sen-components-ether-Configuration>)[#"Configuration"].]

==== #"PortBinding" #chip("variants", "variant") <t-sen-components-ether-PortBinding>
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-components-ether-Ephemeral>)[#"Ephemeral"], "",
  link(<t-sen-components-ether-PinnedPort>)[#"Pinned​Port"], "",
  link(<t-sen-components-ether-ProbePortRange>)[#"Probe​Port​Range"], "",
)
#facts[Named by #link(<t-sen-components-ether-PortConfig>)[#"Port​Config"].]

#section("Arrays", "sequences")

#summary(
  (link(<t-sen-components-ether-EnpointList>)[#"Enpoint​List"], "", <t-sen-components-ether-EnpointList>),
  (link(<t-sen-components-ether-MulticastAddressExclusions>)[#"Multicast​Address​Exclusions"], "", <t-sen-components-ether-MulticastAddressExclusions>),
  (link(<t-sen-components-ether-MulticastRange>)[#"Multicast​Range"], "Range of values of a IPv4 multicast address", <t-sen-components-ether-MulticastRange>),
  (link(<t-sen-components-ether-PortExclusions>)[#"Port​Exclusions"], "", <t-sen-components-ether-PortExclusions>),
)

==== #"EnpointList" #chip("sequences", "sequence") <t-sen-components-ether-EnpointList>
#declared[#kw[sequence]\<#link(<t-sen-components-ether-Endpoint>)[#"Endpoint"], #lit[27]\> #"Enpoint​List"#";"]
#facts[Named by #link(<t-sen-components-ether-SessionPresenceBeam>)[#"Session​Presence​Beam"].]

==== #"MulticastAddressExclusions" #chip("sequences", "sequence") <t-sen-components-ether-MulticastAddressExclusions>
#declared[#kw[sequence]\<#link(<t-sen-components-ether-MulticastAddressRange>)[#"Multicast​Address​Range"]\> #"Multicast​Address​Exclusions"#";"]
#facts[Named by #link(<t-sen-components-ether-BusConfig>)[#"Bus​Config"].]

==== #"MulticastRange" #chip("sequences", "sequence") <t-sen-components-ether-MulticastRange>
#prose("Range of values of a IPv4 multicast address")
#declared[#kw[array]\<#link(<t-sen-components-ether-ByteRange>)[#"Byte​Range"], #lit[4]\> #"Multicast​Range"#";"]
#facts[Named by #link(<t-sen-components-ether-BusConfig>)[#"Bus​Config"].]

==== #"PortExclusions" #chip("sequences", "sequence") <t-sen-components-ether-PortExclusions>
#declared[#kw[sequence]\<#link(<t-sen-components-ether-PortRange>)[#"Port​Range"]\> #"Port​Exclusions"#";"]
#facts[Named by #link(<t-sen-components-ether-Configuration>)[#"Configuration"].]

#section("Optionals", "optionals")

#summary(
  (link(<t-sen-components-ether-MaybeDeviceName>)[#"Maybe​Device​Name"], "", <t-sen-components-ether-MaybeDeviceName>),
  (link(<t-sen-components-ether-MaybeDiscoveryHubPort>)[#"Maybe​Discovery​Hub​Port"], "", <t-sen-components-ether-MaybeDiscoveryHubPort>),
  (link(<t-sen-components-ether-MaybePortConfig>)[#"Maybe​Port​Config"], "", <t-sen-components-ether-MaybePortConfig>),
)

==== #"MaybeDeviceName" #chip("optionals", "optional") <t-sen-components-ether-MaybeDeviceName>
#declared[#kw[optional]\<#link(<t-string>)[#"string"]\> #"Maybe​Device​Name"#";"]

==== #"MaybeDiscoveryHubPort" #chip("optionals", "optional") <t-sen-components-ether-MaybeDiscoveryHubPort>
#declared[#kw[optional]\<#link(<t-u16>)[#"u16"]\> #"Maybe​Discovery​Hub​Port"#";"]

==== #"MaybePortConfig" #chip("optionals", "optional") <t-sen-components-ether-MaybePortConfig>
#declared[#kw[optional]\<#link(<t-sen-components-ether-PortConfig>)[#"Port​Config"]\> #"Maybe​Port​Config"#";"]

== #"explorer"

#prose[1 types.]

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-explorer-Configuration>)[#"Configuration"], "Overall component configuration", <t-sen-components-explorer-Configuration>),
)

==== #"Configuration" #chip("structures", "structure") <t-sen-components-explorer-Configuration>
#prose("Overall component configuration")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("layout​File")], link(<t-string>)[#"string"], "the imgui layout file. Defaults to \"\"",
)

== #"influx"

#prose[3 types.]

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-influx-Configuration>)[#"Configuration"], "Overall component configuration", <t-sen-components-influx-Configuration>),
)

==== #"Configuration" #chip("structures", "structure") <t-sen-components-influx-Configuration>
#prose("Overall component configuration")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("sampling​Period")], link(<t-sen-Duration>)[#"sen.​Duration"], "sampling period for the recordings",
  [#mono("selections")], link(<t-sen-components-influx-SelectionList>)[#"Selection​List"], "objects to be monitored (using sql)",
  [#mono("telegraf​Address")], link(<t-string>)[#"string"], "ip or name of the Telegraf server",
  [#mono("telegraf​Port")], link(<t-u16>)[#"u16"], "port used by the Telegraf server",
  [#mono("protocol")], link(<t-sen-components-influx-Protocol>)[#"Protocol"], "protocol used",
  [#mono("batch​Size")], link(<t-u32>)[#"u32"], "points to batch before emission (0 means no batching)",
)

#section("Enumerations", "enumerations")

#summary(
  (link(<t-sen-components-influx-Protocol>)[#"Protocol"], "", <t-sen-components-influx-Protocol>),
)

==== #"Protocol" #chip("enumerations", "enumeration") <t-sen-components-influx-Protocol>
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("udp", [0]),
  ("tcp", [1]),
)
#facts[Named by #link(<t-sen-components-influx-Configuration>)[#"Configuration"].]

#section("Arrays", "sequences")

#summary(
  (link(<t-sen-components-influx-SelectionList>)[#"Selection​List"], "", <t-sen-components-influx-SelectionList>),
)

==== #"SelectionList" #chip("sequences", "sequence") <t-sen-components-influx-SelectionList>
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"Selection​List"#";"]
#facts[Named by #link(<t-sen-components-influx-Configuration>)[#"Configuration"].]

== #"jsonrpc"

#prose[41 types.]

#section("Object classes", "classes")

#summary(
  (link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"Json​Rpc​Server"], "JsonRpcServer -- the JSON-RPC over WebSocket external API, expressed as a Sen class. One instance per connected client lives on `local.jsonrpc`. Methods are the RPC surface; events are the server-pushed notifications. Wire framing (envelope, id, error mapping) is the dispatcher's job, not this class's. Wire-shape convention: JSON-RPC named params (`\"params\": { ... }`) use the same field names as the STL method's argument list. The dispatcher treats every method's params object as `{ <argName>: <value>, ... }` and walks `method->getArgs()` to build the VarList in declaration order. That lets the inbound dispatch path go through Sen's meta machinery with no per-method translation.", <t-sen-components-jsonrpc-JsonRpcServer>),
  (link(<t-sen-components-jsonrpc-StaticFileServer>)[#"Static​File​Server"], "Server-wide registry of in-memory static-file bundles, served alongside the WebSocket+JSON-RPC surface by the same uWS app. Internal-only: lives on the configurable control bus (defaults to `local.jsonrpc_control`), not on the data-plane bus that hosts authenticated per-connection JsonRpcServer instances. The component publishes exactly one instance.", <t-sen-components-jsonrpc-StaticFileServer>),
)

==== #"JsonRpcServer" #chip("classes", "class") <t-sen-components-jsonrpc-JsonRpcServer>
#prose("JsonRpcServer -- the JSON-RPC over WebSocket external API, expressed as a Sen class. One instance per connected client lives on `local.jsonrpc`. Methods are the RPC surface; events are the server-pushed notifications. Wire framing (envelope, id, error mapping) is the dispatcher's job, not this class's. Wire-shape convention: JSON-RPC named params (`\"params\": { ... }`) use the same field names as the STL method's argument list. The dispatcher treats every method's params object as `{ <argName>: <value>, ... }` and walks `method->getArgs()` to build the VarList in declaration order. That lets the inbound dispatch path go through Sen's meta machinery with no per-method translation.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("identity")], link(<t-sen-components-jsonrpc-Identity>)[#"Identity"], [RO S BE], "Identity of the connected client.",
)
#sen-table(
  columns: (COL-NAME, 1.2fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("ping")], [#mono[() \u{2192} #link(<t-string>)[#"string"]]], "Liveness probe.",
  [#mono("list​Topology")], [#mono[() \u{2192} #link(<t-sen-components-jsonrpc-SessionInfoList>)[#"Session​Info​List"]]], "Snapshot of the currently-detected sessions, each with the buses currently detected inside it. The kernel's discovery surface is asynchronous; this returns whatever was visible at the moment of the call. Use `subscribeTopology` for live updates.",
  [#mono("subscribe​Topology")], [#mono[()]], "Sticky subscription to topology changes. Idempotent per connection. Wires a `topologyChanged` push immediately with the current full snapshot, then again on every session or bus add/remove. Cancel with `unsubscribeTopology`; the subscription is also torn down automatically when the connection closes.",
  [#mono("unsubscribe​Topology")], [#mono[()]], "Cancel the topology subscription created by `subscribeTopology`. Idempotent.",
  [#mono("get​Types")], [#mono[() \u{2192} #link(<t-sen-kernel-StringList>)[#"sen.​kernel.​String​List"]]], "Returns the qualified names of every custom type registered with the kernel. Catalog only -- pair with `getType` to fetch a chosen spec. The bundled-with-objects path on `interestUpdate.types` remains the primary way clients learn about types they observe.",
  [#mono("get​Type")], [#mono[(#"qualified​Name": #link(<t-string>)[#"string"], #"with​Schema": #link(<t-bool>)[#"bool"]) \u{2192} #link(<t-sen-components-jsonrpc-TypeLookupResult>)[#"Type​Lookup​Result"]]], "Look up one type by qualified name. Returns the CustomTypeSpec; when `withSchema` is true, also returns the JSON-Schema fragment for the type (empty string otherwise). Dependency chasing is the client's job: each schema fragment carries `$ref` strings keyed by qualified name, resolved against whatever schema cache the client maintains.",
  [#mono("create​Interest")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"query": #link(<t-string>)[#"string"], #"subscribe": #link(<t-sen-components-jsonrpc-SubscribeBlock>)[#"Subscribe​Block"], #"with​Schemas": #link(<t-bool>)[#"bool"])]], "Register a sticky interest defined by an STL query. Optional `subscribe` block pre-subscribes to properties/events on every matched object, including ones that arrive later. When `withSchemas` is true, every subsequent `interestUpdate` for this interest carries the JSON-Schema fragment for each previously-unseen type alongside the existing `CustomTypeSpec`. Re-registering the same name = invalidParams. Wire-ordering contract: the initial `interestUpdate.added` for the current match set is emitted *before* this RPC's response lands on the wire. Clients must accept pushes for an interest whose response has not yet been observed.",
  [#mono("release​Interest")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"])]], "Drop an interest and every subscription living under it.",
  [#mono("list​Objects")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"]) \u{2192} #link(<t-sen-components-jsonrpc-ObjectInfos>)[#"Object​Infos"]]], "List the names and classes of objects currently in an interest's match set.",
  [#mono("get​Property")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"property​Name": #link(<t-string>)[#"string"]) \u{2192} #link(<t-string>)[#"string"]]], "Read a property's current value, synchronously.",
  [#mono("get​Objects​Batch​State")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Names": #link(<t-sen-kernel-StringList>)[#"sen.​kernel.​String​List"], #"property​Names": #link(<t-sen-kernel-StringList>)[#"sen.​kernel.​String​List"]) \u{2192} #link(<t-sen-components-jsonrpc-ObjectStateList>)[#"Object​State​List"]]], "Batched per-object snapshot. Walks the interest's match set in one pass instead of forcing the caller to issue N*M `getProperty` round-trips. Omitting `objectNames` reads every object currently matched; omitting `propertyNames` reads every property of each object (the class hierarchy is walked, so inherited members are included). Names from `objectNames` that aren't currently in the match set are silently skipped -- the caller compares its requested list against the returned `objectName`s to detect omissions. Names from `propertyNames` that don't exist on a given object's class are recorded per-object under `errors`, so a heterogeneous batch still returns useful partial results. Bounded: the call rejects with `invalidParams` when the resolved object set exceeds 512 entries (either via an explicit `objectNames` list of that length, or via an implicit walk over a match set that has grown that large). Page the request by passing `objectNames`.",
  [#mono("set​Property")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"property​Name": #link(<t-string>)[#"string"], #"value": #link(<t-string>)[#"string"])]], "Write a property's value (sugar over invoking the generated setter). Hand-routed in `Dispatcher::handleSetProperty`: the dispatcher issues `invokeUntyped` against the property's setter and wires the response through the kernel callback in a single tick. The typed `setPropertyImpl` override in the C++ `Server` exists only to satisfy the codegen-generated abstract base and is unreachable at runtime. See `architecture.md` \"Method dispatch\" for why this method (and `invoke`) bypass the generic meta path.",
  [#mono("subscribe​Property")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"property​Name": #link(<t-string>)[#"string"], #"max​Rate​Hz": #link(<t-f64>)[#"f64"])]], "Subscribe to propertyChanged notifications for one (interest, object, property). Sticky: persists across object churn; auto-rewires when the named object (re)enters the match set. Snapshot of the current value is delivered immediately after wiring. The throttle is per-object, shared across every property/event subscribed for the same object. An explicit `maxRateHz` overrides the current rate for the object; omitting it preserves whatever rate is already in effect (so adding a per-property sub doesn't accidentally retune existing subs back to unlimited).",
  [#mono("unsubscribe​Property")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"property​Name": #link(<t-string>)[#"string"])]], "Cancel a property subscription previously created with subscribeProperty.",
  [#mono("subscribe​Event")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"event​Name": #link(<t-string>)[#"string"])]], "Subscribe to eventTriggered notifications for one (interest, object, event).",
  [#mono("unsubscribe​Event")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"event​Name": #link(<t-string>)[#"string"])]], "Cancel an event subscription previously created with subscribeEvent.",
  [#mono("subscribe​All")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"max​Rate​Hz": #link(<t-f64>)[#"f64"])]], "Subscribe to every property and every event of one named object. Wildcard expansion against the object's actual class (including parent classes). Object must currently be in the match set; absent objects are invalidParams.",
  [#mono("unsubscribe​All")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"])]], "Cancel an all-members subscription previously created with subscribeAll.",
  [#mono("invoke")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"method​Name": #link(<t-string>)[#"string"], #"args​Json": #link(<t-string>)[#"string"]) \u{2192} #link(<t-string>)[#"string"]]], "Call a method on a kernel object and await its return value. Hand-routed in `Dispatcher::handleInvoke`: validates the args, issues `invokeUntyped`, and wires the response through the kernel callback in a single tick. The typed `invokeImpl` override in the C++ `Server` exists only to satisfy the codegen-generated abstract base and is unreachable at runtime. See `architecture.md` \"Method dispatch\".",
)
#sen-table(
  columns: (COL-NAME, 1fr, 1fr),
  table.header([*Event*], [*Payload*], [*Description*]),
  [#mono("property​Changed")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"values": #link(<t-sen-components-jsonrpc-PropertyValueList>)[#"Property​Value​List"], #"timestamp": #link(<t-sen-TimeStamp>)[#"sen.​Time​Stamp"])]], "Bundled per-object snapshot of changed properties. Per-object bundling, not per-property: any property change on one object lands in a single envelope.",
  [#mono("event​Triggered")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"object​Name": #link(<t-string>)[#"string"], #"event​Name": #link(<t-string>)[#"string"], #"args": #link(<t-string>)[#"string"], #"timestamp": #link(<t-sen-TimeStamp>)[#"sen.​Time​Stamp"])]], "An event fired on one object in one interest. The event signature is decided by the source object's class, not by JSON-RPC.",
  [#mono("interest​Update")], [#mono[(#"interest​Name": #link(<t-string>)[#"string"], #"added": #link(<t-sen-components-jsonrpc-AddedObjectList>)[#"Added​Object​List"], #"removed": #link(<t-sen-kernel-StringList>)[#"sen.​kernel.​String​List"], #"types": #link(<t-sen-kernel-CustomTypeSpecList>)[#"sen.​kernel.​Custom​Type​Spec​List"], #"type​Schemas": #link(<t-string>)[#"string"])]], "Match-set delta for one interest. The dispatcher tracks knownTypes / knownSchemas per connection and only re-sends specs / schemas new to the client.",
  [#mono("topology​Changed")], [#mono[(#"sessions": #link(<t-sen-components-jsonrpc-SessionInfoList>)[#"Session​Info​List"])]], "Full snapshot of the topology after a session or bus add/remove. Pushed once on `subscribeTopology` (with the current state) and again on every change. Snapshot form rather than deltas: topology changes are rare, and the snapshot keeps client logic trivial and idempotent.",
  [#mono("notifications​Dropped")], [#mono[(#"count": #link(<t-u64>)[#"u64"])]], "Sent once when outbound backpressure clears on this connection, reporting how many unreliable notifications were dropped while the buffer was above the high watermark. Emitted reliably on purpose: the notice about dropped messages must not be droppable. A client that receives this has an incomplete picture and cannot tell which updates it missed -- only that it missed some. The correct response is to re-read what it displays (getObjectsBatchState over the affected interests) rather than to wait for the next change, which may never come for a property that has stopped moving.",
)

==== #"StaticFileServer" #chip("classes", "class") <t-sen-components-jsonrpc-StaticFileServer>
#prose("Server-wide registry of in-memory static-file bundles, served alongside the WebSocket+JSON-RPC surface by the same uWS app. Internal-only: lives on the configurable control bus (defaults to `local.jsonrpc_control`), not on the data-plane bus that hosts authenticated per-connection JsonRpcServer instances. The component publishes exactly one instance.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("registered​Bundles")], link(<t-sen-components-jsonrpc-RegisteredBundleInfoList>)[#"Registered​Bundle​Info​List"], [RO D C], "Currently-registered bundles. Externally read-only; the component mutates this whenever a bundle is registered or unregistered. Subscribe to receive notifications on every change.",
)
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("register​Static​Bundle")], [#mono[(#"bundle": #link(<t-sen-components-jsonrpc-StaticBundle>)[#"Static​Bundle"]) \u{2192} #link(<t-u64>)[#"u64"]]], "Register a bundle. The bundle is copied into the registry; the caller may release the argument afterwards. Routes are wired up asynchronously on the uWS thread; the returned bundleId is stable and may be used immediately with `unregisterStaticBundle`.",
  [#mono("unregister​Static​Bundle")], [#mono[(#"bundle​Id": #link(<t-u64>)[#"u64"])]], "Stop serving the bundle. The slot in the registry is nulled; the underlying uWS route remains registered (uWS does not expose a clean route-removal API) but becomes inert and returns 404. No-op if `bundleId` is unknown.",
)

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-jsonrpc-AddedObjectEntry>)[#"Added​Object​Entry"], "One object newly entering an interest's match set.", <t-sen-components-jsonrpc-AddedObjectEntry>),
  (link(<t-sen-components-jsonrpc-Configuration>)[#"Configuration"], "JSON-RPC over WebSocket component configuration. Only `address` and `port` are required; every other field falls back to a documented default.", <t-sen-components-jsonrpc-Configuration>),
  (link(<t-sen-components-jsonrpc-ConnectionLimits>)[#"Connection​Limits"], "Per-connection buffer / timeout knobs consulted on the uWS thread for the lifetime of the server. Every field is optional; unset fields fall back to the defaults resolved inside ws_server.cpp at the use site.", <t-sen-components-jsonrpc-ConnectionLimits>),
  (link(<t-sen-components-jsonrpc-Identity>)[#"Identity"], "Identity of the connected client. Set once on connect by the dispatcher from the WebSocket upgrade authenticator. Read-only externally. Other components / admin tools can SELECT * FROM local.jsonrpc to enumerate currently-connected clients.", <t-sen-components-jsonrpc-Identity>),
  (link(<t-sen-components-jsonrpc-NamedSelection>)[#"Named​Selection"], "Named arm of MemberSelector: matches only the listed members. Wire shape: `{type: \"NamedSelection\", value: {memberNames: [...]}}`.", <t-sen-components-jsonrpc-NamedSelection>),
  (link(<t-sen-components-jsonrpc-ObjectInfo>)[#"Object​Info"], "One entry of listObjects' result.", <t-sen-components-jsonrpc-ObjectInfo>),
  (link(<t-sen-components-jsonrpc-ObjectState>)[#"Object​State"], "Per-object snapshot returned by `getObjectsBatchState`. `properties` carries successful reads (one entry per property name resolved against the object's class hierarchy); `errors` is empty when nothing failed for this object.", <t-sen-components-jsonrpc-ObjectState>),
  (link(<t-sen-components-jsonrpc-PropertyError>)[#"Property​Error"], "Per-property read error. Used when the caller-supplied `propertyNames` includes a name that doesn't exist on a given object's class, or when the accessor itself throws. Carried in a separate list from successful reads so a property literally named `error` can never collide with the failure envelope.", <t-sen-components-jsonrpc-PropertyError>),
  (link(<t-sen-components-jsonrpc-PropertyValuePair>)[#"Property​Value​Pair"], "Per-property changed value. The value type is decided by the addressed kernel object, not by JSON-RPC, so STL can't statically describe it.", <t-sen-components-jsonrpc-PropertyValuePair>),
  (link(<t-sen-components-jsonrpc-RegisteredBundleInfo>)[#"Registered​Bundle​Info"], "Summary record returned by `listRegisteredBundles`. Excludes the file bytes; just enough to answer \"what is currently being served, and where\".", <t-sen-components-jsonrpc-RegisteredBundleInfo>),
  (link(<t-sen-components-jsonrpc-SessionInfo>)[#"Session​Info"], "One discovered session and the buses currently detected inside it. Names are opaque strings from the kernel's session-discovery surface.", <t-sen-components-jsonrpc-SessionInfo>),
  (link(<t-sen-components-jsonrpc-StaticBundle>)[#"Static​Bundle"], "One bundle of in-memory files served on a configurable URL prefix. The same bundle's files are served at `<urlPrefix>/<file.path>`. `indexFileName` is the SPA fallback served whenever a request under `urlPrefix` does not match any file (lets the client-side router handle paths the server does not know about).", <t-sen-components-jsonrpc-StaticBundle>),
  (link(<t-sen-components-jsonrpc-StaticFile>)[#"Static​File"], "One file inside a StaticBundle. Bytes are owned by the bundle; `path` is relative to the bundle's `urlPrefix` and uses forward slashes regardless of the producer's OS. `contentType` is the MIME type the server will return verbatim; the bundle producer decides it (no extension-based inference happens server-side).", <t-sen-components-jsonrpc-StaticFile>),
  (link(<t-sen-components-jsonrpc-SubscribeBlock>)[#"Subscribe​Block"], "Pre-subscription block bundled into createInterest. Wires the listed property / event subscriptions on every matched object -- including ones that arrive later. Wire shape mirrors the typed STL representation exactly: each `MaybeMemberSelector` is either omitted (no auto-subscribe), `null`, or one of the typed variant arms (`{type: \"WildcardSelection\", value: {}}` or `{type: \"NamedSelection\", value: {memberNames: [...]}}`); `maxRateHz` is an optional positive number. Strict alignment with the typed surface lets the dispatcher's uniform meta-dispatch path consume the wire directly via `fromJson`, without a per-method translation step.", <t-sen-components-jsonrpc-SubscribeBlock>),
  (link(<t-sen-components-jsonrpc-TlsConfig>)[#"Tls​Config"], "TLS materials. The schema is reserved so configs stay forward-compatible, but the component fails to start if `tls` is set; full TLS support is not yet wired through the uWebSockets SSL backend.", <t-sen-components-jsonrpc-TlsConfig>),
  (link(<t-sen-components-jsonrpc-TypeLookupResult>)[#"Type​Lookup​Result"], "Bundled return of getType: always the CustomTypeSpec, plus the JSON-Schema fragment when the caller opted in. `schema` is empty when not requested.", <t-sen-components-jsonrpc-TypeLookupResult>),
  (link(<t-sen-components-jsonrpc-WildcardSelection>)[#"Wildcard​Selection"], "Wildcard arm of MemberSelector: matches every member of the relevant kind. Wire shape: `{type: \"WildcardSelection\", value: {}}`.", <t-sen-components-jsonrpc-WildcardSelection>),
)

==== #"AddedObjectEntry" #chip("structures", "structure") <t-sen-components-jsonrpc-AddedObjectEntry>
#prose("One object newly entering an interest's match set.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("object​Name")], link(<t-string>)[#"string"], "name of the newly-matched object",
  [#mono("qualified​Class​Name")], link(<t-string>)[#"string"], "package-qualified class name of the object",
  [#mono("current​Values")], link(<t-sen-components-jsonrpc-PropertyValueList>)[#"Property​Value​List"], "initial values of pre-subscribed properties (empty if none wired)",
)
#facts[Named by #link(<t-sen-components-jsonrpc-AddedObjectList>)[#"Added​Object​List"].]

==== #"Configuration" #chip("structures", "structure") <t-sen-components-jsonrpc-Configuration>
#prose("JSON-RPC over WebSocket component configuration. Only `address` and `port` are required; every other field falls back to a documented default.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("address")], link(<t-string>)[#"string"], "The IP address or hostname the server will bind to",
  [#mono("port")], link(<t-u16>)[#"u16"], "The port number the server will listen on",
  [#mono("update​Freq​Hz")], link(<t-f64>)[#"f64"], "Run-loop frequency in Hz; defaults to 60",
  [#mono("connection​Limits")], link(<t-sen-components-jsonrpc-ConnectionLimits>)[#"Connection​Limits"], "Per-connection uWS buffer / timeout knobs",
  [#mono("tls")], link(<t-sen-components-jsonrpc-TlsConfig>)[#"Tls​Config"], "schema placeholder; setting this fails component start",
  [#mono("control​Bus​Name")], link(<t-string>)[#"string"], "Control-plane bus on `local`; defaults to \"jsonrpc_control\"",
)

==== #"ConnectionLimits" #chip("structures", "structure") <t-sen-components-jsonrpc-ConnectionLimits>
#prose("Per-connection buffer / timeout knobs consulted on the uWS thread for the lifetime of the server. Every field is optional; unset fields fall back to the defaults resolved inside ws_server.cpp at the use site.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("idle​Timeout​Seconds")], link(<t-u16>)[#"u16"], "uWS idle close timeout; defaults to 120",
  [#mono("max​Payload​Bytes")], link(<t-u32>)[#"u32"], "Max inbound frame size; defaults to 16 KiB",
  [#mono("max​Backpressure​Bytes")], link(<t-u32>)[#"u32"], "uWS hard outbound buffer ceiling; defaults to 64 KiB",
  [#mono("high​Backpressure​Bytes")], link(<t-u32>)[#"u32"], "Soft trigger for the BackpressureUpdate signal; defaults to 32 KiB",
)
#facts[Named by #link(<t-sen-components-jsonrpc-Configuration>)[#"Configuration"].]

==== #"Identity" #chip("structures", "structure") <t-sen-components-jsonrpc-Identity>
#prose("Identity of the connected client. Set once on connect by the dispatcher from the WebSocket upgrade authenticator. Read-only externally. Other components / admin tools can SELECT * FROM local.jsonrpc to enumerate currently-connected clients.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("subject")], link(<t-string>)[#"string"], "authenticated subject identifier (e.g. user / service principal)",
)
#facts[Named by #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"Json​Rpc​Server"].]

==== #"NamedSelection" #chip("structures", "structure") <t-sen-components-jsonrpc-NamedSelection>
#prose("Named arm of MemberSelector: matches only the listed members. Wire shape: `{type: \"NamedSelection\", value: {memberNames: [...]}}`.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("member​Names")], link(<t-sen-kernel-StringList>)[#"sen.​kernel.​String​List"], "member names to match",
)
#facts[Named by #link(<t-sen-components-jsonrpc-MemberSelector>)[#"Member​Selector"].]

==== #"ObjectInfo" #chip("structures", "structure") <t-sen-components-jsonrpc-ObjectInfo>
#prose("One entry of listObjects' result.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("object​Name")], link(<t-string>)[#"string"], "object name within the interest's match set",
  [#mono("qualified​Class​Name")], link(<t-string>)[#"string"], "package-qualified class name of the object",
)
#facts[Named by #link(<t-sen-components-jsonrpc-ObjectInfos>)[#"Object​Infos"].]

==== #"ObjectState" #chip("structures", "structure") <t-sen-components-jsonrpc-ObjectState>
#prose("Per-object snapshot returned by `getObjectsBatchState`. `properties` carries successful reads (one entry per property name resolved against the object's class hierarchy); `errors` is empty when nothing failed for this object.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("object​Name")], link(<t-string>)[#"string"], "object name within the interest's match set",
  [#mono("qualified​Class​Name")], link(<t-string>)[#"string"], "package-qualified class name at read time",
  [#mono("properties")], link(<t-sen-components-jsonrpc-PropertyValueList>)[#"Property​Value​List"], "resolved property values; empty when caller's filter matched nothing",
  [#mono("errors")], link(<t-sen-components-jsonrpc-PropertyErrorList>)[#"Property​Error​List"], "per-property failures; empty when none occurred",
)
#facts[Named by #link(<t-sen-components-jsonrpc-ObjectStateList>)[#"Object​State​List"].]

==== #"PropertyError" #chip("structures", "structure") <t-sen-components-jsonrpc-PropertyError>
#prose("Per-property read error. Used when the caller-supplied `propertyNames` includes a name that doesn't exist on a given object's class, or when the accessor itself throws. Carried in a separate list from successful reads so a property literally named `error` can never collide with the failure envelope.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("property​Name")], link(<t-string>)[#"string"], "name from the caller's `propertyNames` list that could not be read",
  [#mono("error")], link(<t-string>)[#"string"], "human-readable reason (e.g. \"unknown property\")",
)
#facts[Named by #link(<t-sen-components-jsonrpc-PropertyErrorList>)[#"Property​Error​List"].]

==== #"PropertyValuePair" #chip("structures", "structure") <t-sen-components-jsonrpc-PropertyValuePair>
#prose("Per-property changed value. The value type is decided by the addressed kernel object, not by JSON-RPC, so STL can't statically describe it.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("property​Name")], link(<t-string>)[#"string"], "property member name",
  [#mono("value")], link(<t-string>)[#"string"], "JSON-encoded SenValue",
)
#facts[Named by #link(<t-sen-components-jsonrpc-PropertyValueList>)[#"Property​Value​List"].]

==== #"RegisteredBundleInfo" #chip("structures", "structure") <t-sen-components-jsonrpc-RegisteredBundleInfo>
#prose("Summary record returned by `listRegisteredBundles`. Excludes the file bytes; just enough to answer \"what is currently being served, and where\".")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("bundle​Id")], link(<t-u64>)[#"u64"], "opaque id assigned by the server, stable for the bundle's lifetime",
  [#mono("url​Prefix")], link(<t-string>)[#"string"], "bundle's URL prefix",
  [#mono("file​Count")], link(<t-u64>)[#"u64"], "number of files in the bundle",
)
#facts[Named by #link(<t-sen-components-jsonrpc-RegisteredBundleInfoList>)[#"Registered​Bundle​Info​List"].]

==== #"SessionInfo" #chip("structures", "structure") <t-sen-components-jsonrpc-SessionInfo>
#prose("One discovered session and the buses currently detected inside it. Names are opaque strings from the kernel's session-discovery surface.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "session name",
  [#mono("buses")], link(<t-sen-kernel-StringList>)[#"sen.​kernel.​String​List"], "bus names currently detected in this session",
)
#facts[Named by #link(<t-sen-components-jsonrpc-SessionInfoList>)[#"Session​Info​List"].]

==== #"StaticBundle" #chip("structures", "structure") <t-sen-components-jsonrpc-StaticBundle>
#prose("One bundle of in-memory files served on a configurable URL prefix. The same bundle's files are served at `<urlPrefix>/<file.path>`. `indexFileName` is the SPA fallback served whenever a request under `urlPrefix` does not match any file (lets the client-side router handle paths the server does not know about).")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("url​Prefix")], link(<t-string>)[#"string"], "URL prefix, e.g. \"/explorer\"; must start with \"/\"",
  [#mono("index​File​Name")], link(<t-string>)[#"string"], "SPA fallback, e.g. \"index.html\"; must match a `path` in `files`",
  [#mono("files")], link(<t-sen-components-jsonrpc-StaticFileList>)[#"Static​File​List"], "file set",
)
#facts[Named by #link(<t-sen-components-jsonrpc-StaticFileServer>)[#"Static​File​Server"].]

==== #"StaticFile" #chip("structures", "structure") <t-sen-components-jsonrpc-StaticFile>
#prose("One file inside a StaticBundle. Bytes are owned by the bundle; `path` is relative to the bundle's `urlPrefix` and uses forward slashes regardless of the producer's OS. `contentType` is the MIME type the server will return verbatim; the bundle producer decides it (no extension-based inference happens server-side).")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("path")], link(<t-string>)[#"string"], "path within the bundle, e.g. \"assets/main.js\"",
  [#mono("content​Type")], link(<t-string>)[#"string"], "MIME type, e.g. \"text/html\"",
  [#mono("contents")], link(<t-sen-kernel-Buffer>)[#"sen.​kernel.​Buffer"], "raw file bytes",
)
#facts[Named by #link(<t-sen-components-jsonrpc-StaticFileList>)[#"Static​File​List"].]

==== #"SubscribeBlock" #chip("structures", "structure") <t-sen-components-jsonrpc-SubscribeBlock>
#prose("Pre-subscription block bundled into createInterest. Wires the listed property / event subscriptions on every matched object -- including ones that arrive later. Wire shape mirrors the typed STL representation exactly: each `MaybeMemberSelector` is either omitted (no auto-subscribe), `null`, or one of the typed variant arms (`{type: \"WildcardSelection\", value: {}}` or `{type: \"NamedSelection\", value: {memberNames: [...]}}`); `maxRateHz` is an optional positive number. Strict alignment with the typed surface lets the dispatcher's uniform meta-dispatch path consume the wire directly via `fromJson`, without a per-method translation step.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("properties")], link(<t-sen-components-jsonrpc-MemberSelector>)[#"Member​Selector"], "which properties to auto-subscribe to (or none)",
  [#mono("events")], link(<t-sen-components-jsonrpc-MemberSelector>)[#"Member​Selector"], "which events to auto-subscribe to (or none)",
  [#mono("max​Rate​Hz")], link(<t-f64>)[#"f64"], "optional throttle cap shared across the wired subscriptions",
)
#facts[Named by #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"Json​Rpc​Server"].]

==== #"TlsConfig" #chip("structures", "structure") <t-sen-components-jsonrpc-TlsConfig>
#prose("TLS materials. The schema is reserved so configs stay forward-compatible, but the component fails to start if `tls` is set; full TLS support is not yet wired through the uWebSockets SSL backend.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("cert​Path")], link(<t-string>)[#"string"], "PEM-encoded server certificate",
  [#mono("key​Path")], link(<t-string>)[#"string"], "PEM-encoded server private key",
)
#facts[Named by #link(<t-sen-components-jsonrpc-Configuration>)[#"Configuration"].]

==== #"TypeLookupResult" #chip("structures", "structure") <t-sen-components-jsonrpc-TypeLookupResult>
#prose("Bundled return of getType: always the CustomTypeSpec, plus the JSON-Schema fragment when the caller opted in. `schema` is empty when not requested.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("spec")], link(<t-sen-kernel-CustomTypeSpec>)[#"sen.​kernel.​Custom​Type​Spec"], "",
  [#mono("schema")], link(<t-string>)[#"string"], "JSON-encoded schema fragment; \"\" when withSchema=false",
)
#facts[Named by #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"Json​Rpc​Server"].]

==== #"WildcardSelection" #chip("structures", "structure") <t-sen-components-jsonrpc-WildcardSelection>
#prose("Wildcard arm of MemberSelector: matches every member of the relevant kind. Wire shape: `{type: \"WildcardSelection\", value: {}}`.")
#facts[Named by #link(<t-sen-components-jsonrpc-MemberSelector>)[#"Member​Selector"].]

#section("Variant records", "variants")

#summary(
  (link(<t-sen-components-jsonrpc-MemberSelector>)[#"Member​Selector"], "Selector for properties or events in a SubscribeBlock. Absent = no auto-subscribe; WildcardSelection = \"*\"; NamedSelection = explicit list.", <t-sen-components-jsonrpc-MemberSelector>),
)

==== #"MemberSelector" #chip("variants", "variant") <t-sen-components-jsonrpc-MemberSelector>
#prose("Selector for properties or events in a SubscribeBlock. Absent = no auto-subscribe; WildcardSelection = \"*\"; NamedSelection = explicit list.")
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-components-jsonrpc-WildcardSelection>)[#"Wildcard​Selection"], "",
  link(<t-sen-components-jsonrpc-NamedSelection>)[#"Named​Selection"], "",
)
#facts[Named by #link(<t-sen-components-jsonrpc-SubscribeBlock>)[#"Subscribe​Block"].]

#section("Arrays", "sequences")

#summary(
  (link(<t-sen-components-jsonrpc-AddedObjectList>)[#"Added​Object​List"], "", <t-sen-components-jsonrpc-AddedObjectList>),
  (link(<t-sen-components-jsonrpc-ObjectInfos>)[#"Object​Infos"], "", <t-sen-components-jsonrpc-ObjectInfos>),
  (link(<t-sen-components-jsonrpc-ObjectStateList>)[#"Object​State​List"], "", <t-sen-components-jsonrpc-ObjectStateList>),
  (link(<t-sen-components-jsonrpc-PropertyErrorList>)[#"Property​Error​List"], "", <t-sen-components-jsonrpc-PropertyErrorList>),
  (link(<t-sen-components-jsonrpc-PropertyValueList>)[#"Property​Value​List"], "", <t-sen-components-jsonrpc-PropertyValueList>),
  (link(<t-sen-components-jsonrpc-RegisteredBundleInfoList>)[#"Registered​Bundle​Info​List"], "", <t-sen-components-jsonrpc-RegisteredBundleInfoList>),
  (link(<t-sen-components-jsonrpc-SessionInfoList>)[#"Session​Info​List"], "", <t-sen-components-jsonrpc-SessionInfoList>),
  (link(<t-sen-components-jsonrpc-StaticFileList>)[#"Static​File​List"], "", <t-sen-components-jsonrpc-StaticFileList>),
)

==== #"AddedObjectList" #chip("sequences", "sequence") <t-sen-components-jsonrpc-AddedObjectList>
#declared[#kw[sequence]\<#link(<t-sen-components-jsonrpc-AddedObjectEntry>)[#"Added​Object​Entry"]\> #"Added​Object​List"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"Json​Rpc​Server"].]

==== #"ObjectInfos" #chip("sequences", "sequence") <t-sen-components-jsonrpc-ObjectInfos>
#declared[#kw[sequence]\<#link(<t-sen-components-jsonrpc-ObjectInfo>)[#"Object​Info"]\> #"Object​Infos"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"Json​Rpc​Server"].]

==== #"ObjectStateList" #chip("sequences", "sequence") <t-sen-components-jsonrpc-ObjectStateList>
#declared[#kw[sequence]\<#link(<t-sen-components-jsonrpc-ObjectState>)[#"Object​State"]\> #"Object​State​List"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"Json​Rpc​Server"].]

==== #"PropertyErrorList" #chip("sequences", "sequence") <t-sen-components-jsonrpc-PropertyErrorList>
#declared[#kw[sequence]\<#link(<t-sen-components-jsonrpc-PropertyError>)[#"Property​Error"]\> #"Property​Error​List"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-ObjectState>)[#"Object​State"].]

==== #"PropertyValueList" #chip("sequences", "sequence") <t-sen-components-jsonrpc-PropertyValueList>
#declared[#kw[sequence]\<#link(<t-sen-components-jsonrpc-PropertyValuePair>)[#"Property​Value​Pair"]\> #"Property​Value​List"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-AddedObjectEntry>)[#"Added​Object​Entry"], #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"Json​Rpc​Server"], #link(<t-sen-components-jsonrpc-ObjectState>)[#"Object​State"].]

==== #"RegisteredBundleInfoList" #chip("sequences", "sequence") <t-sen-components-jsonrpc-RegisteredBundleInfoList>
#declared[#kw[sequence]\<#link(<t-sen-components-jsonrpc-RegisteredBundleInfo>)[#"Registered​Bundle​Info"]\> #"Registered​Bundle​Info​List"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-StaticFileServer>)[#"Static​File​Server"].]

==== #"SessionInfoList" #chip("sequences", "sequence") <t-sen-components-jsonrpc-SessionInfoList>
#declared[#kw[sequence]\<#link(<t-sen-components-jsonrpc-SessionInfo>)[#"Session​Info"]\> #"Session​Info​List"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"Json​Rpc​Server"].]

==== #"StaticFileList" #chip("sequences", "sequence") <t-sen-components-jsonrpc-StaticFileList>
#declared[#kw[sequence]\<#link(<t-sen-components-jsonrpc-StaticFile>)[#"Static​File"]\> #"Static​File​List"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-StaticBundle>)[#"Static​Bundle"].]

#section("Optionals", "optionals")

#summary(
  (link(<t-sen-components-jsonrpc-MaybeConnectionLimits>)[#"Maybe​Connection​Limits"], "", <t-sen-components-jsonrpc-MaybeConnectionLimits>),
  (link(<t-sen-components-jsonrpc-MaybeControlBusName>)[#"Maybe​Control​Bus​Name"], "", <t-sen-components-jsonrpc-MaybeControlBusName>),
  (link(<t-sen-components-jsonrpc-MaybeFlag>)[#"Maybe​Flag"], "", <t-sen-components-jsonrpc-MaybeFlag>),
  (link(<t-sen-components-jsonrpc-MaybeHighBackpressureBytes>)[#"Maybe​High​Backpressure​Bytes"], "", <t-sen-components-jsonrpc-MaybeHighBackpressureBytes>),
  (link(<t-sen-components-jsonrpc-MaybeIdleTimeoutSeconds>)[#"Maybe​Idle​Timeout​Seconds"], "", <t-sen-components-jsonrpc-MaybeIdleTimeoutSeconds>),
  (link(<t-sen-components-jsonrpc-MaybeMaxBackpressureBytes>)[#"Maybe​Max​Backpressure​Bytes"], "", <t-sen-components-jsonrpc-MaybeMaxBackpressureBytes>),
  (link(<t-sen-components-jsonrpc-MaybeMaxPayloadBytes>)[#"Maybe​Max​Payload​Bytes"], "", <t-sen-components-jsonrpc-MaybeMaxPayloadBytes>),
  (link(<t-sen-components-jsonrpc-MaybeMemberSelector>)[#"Maybe​Member​Selector"], "", <t-sen-components-jsonrpc-MaybeMemberSelector>),
  (link(<t-sen-components-jsonrpc-MaybeRateHz>)[#"Maybe​Rate​Hz"], "", <t-sen-components-jsonrpc-MaybeRateHz>),
  (link(<t-sen-components-jsonrpc-MaybeStringList>)[#"Maybe​String​List"], "Optional list of object names. Absent on the wire (or `null`) selects every object currently in the addressed interest's match set; present limits the read to the listed names.", <t-sen-components-jsonrpc-MaybeStringList>),
  (link(<t-sen-components-jsonrpc-MaybeSubscribeBlock>)[#"Maybe​Subscribe​Block"], "", <t-sen-components-jsonrpc-MaybeSubscribeBlock>),
  (link(<t-sen-components-jsonrpc-MaybeTlsConfig>)[#"Maybe​Tls​Config"], "", <t-sen-components-jsonrpc-MaybeTlsConfig>),
  (link(<t-sen-components-jsonrpc-MaybeUpdateFreqHz>)[#"Maybe​Update​Freq​Hz"], "", <t-sen-components-jsonrpc-MaybeUpdateFreqHz>),
)

==== #"MaybeConnectionLimits" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeConnectionLimits>
#declared[#kw[optional]\<#link(<t-sen-components-jsonrpc-ConnectionLimits>)[#"Connection​Limits"]\> #"Maybe​Connection​Limits"#";"]

==== #"MaybeControlBusName" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeControlBusName>
#declared[#kw[optional]\<#link(<t-string>)[#"string"]\> #"Maybe​Control​Bus​Name"#";"]

==== #"MaybeFlag" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeFlag>
#declared[#kw[optional]\<#link(<t-bool>)[#"bool"]\> #"Maybe​Flag"#";"]

==== #"MaybeHighBackpressureBytes" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeHighBackpressureBytes>
#declared[#kw[optional]\<#link(<t-u32>)[#"u32"]\> #"Maybe​High​Backpressure​Bytes"#";"]

==== #"MaybeIdleTimeoutSeconds" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeIdleTimeoutSeconds>
#declared[#kw[optional]\<#link(<t-u16>)[#"u16"]\> #"Maybe​Idle​Timeout​Seconds"#";"]

==== #"MaybeMaxBackpressureBytes" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeMaxBackpressureBytes>
#declared[#kw[optional]\<#link(<t-u32>)[#"u32"]\> #"Maybe​Max​Backpressure​Bytes"#";"]

==== #"MaybeMaxPayloadBytes" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeMaxPayloadBytes>
#declared[#kw[optional]\<#link(<t-u32>)[#"u32"]\> #"Maybe​Max​Payload​Bytes"#";"]

==== #"MaybeMemberSelector" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeMemberSelector>
#declared[#kw[optional]\<#link(<t-sen-components-jsonrpc-MemberSelector>)[#"Member​Selector"]\> #"Maybe​Member​Selector"#";"]

==== #"MaybeRateHz" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeRateHz>
#declared[#kw[optional]\<#link(<t-f64>)[#"f64"]\> #"Maybe​Rate​Hz"#";"]

==== #"MaybeStringList" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeStringList>
#prose("Optional list of object names. Absent on the wire (or `null`) selects every object currently in the addressed interest's match set; present limits the read to the listed names.")
#declared[#kw[optional]\<#link(<t-sen-kernel-StringList>)[#"sen.​kernel.​String​List"]\> #"Maybe​String​List"#";"]

==== #"MaybeSubscribeBlock" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeSubscribeBlock>
#declared[#kw[optional]\<#link(<t-sen-components-jsonrpc-SubscribeBlock>)[#"Subscribe​Block"]\> #"Maybe​Subscribe​Block"#";"]

==== #"MaybeTlsConfig" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeTlsConfig>
#declared[#kw[optional]\<#link(<t-sen-components-jsonrpc-TlsConfig>)[#"Tls​Config"]\> #"Maybe​Tls​Config"#";"]

==== #"MaybeUpdateFreqHz" #chip("optionals", "optional") <t-sen-components-jsonrpc-MaybeUpdateFreqHz>
#declared[#kw[optional]\<#link(<t-f64>)[#"f64"]\> #"Maybe​Update​Freq​Hz"#";"]

== #"logmaster"

#prose[3 types.]

#section("Object classes", "classes")

#summary(
  (link(<t-sen-components-logmaster-LogMaster>)[#"Log​Master"], "allows controlling the loggers", <t-sen-components-logmaster-LogMaster>),
  (link(<t-sen-components-logmaster-Logger>)[#"Logger"], "represents a logger", <t-sen-components-logmaster-Logger>),
)

==== #"LogMaster" #chip("classes", "class") <t-sen-components-logmaster-LogMaster>
#prose("allows controlling the loggers")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("target​Bus")], link(<t-string>)[#"string"], [RO S BE], "",
)
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("mute​All")], [#mono[()]], "mute all the loggers",
  [#mono("unmute​All")], [#mono[()]], "un-mutes all the muted loggers",
  [#mono("toggle​Mute​All")], [#mono[()]], "toggles the muting of all loggers",
  [#mono("set​Level")], [#mono[(#"level": #link(<t-sen-kernel-log-LogLevel>)[#"sen.​kernel.​log.​Log​Level"])]], "sets all the loggers to a given level",
)

==== #"Logger" #chip("classes", "class") <t-sen-components-logmaster-Logger>
#prose("represents a logger")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("level")], link(<t-sen-kernel-log-LogLevel>)[#"sen.​kernel.​log.​Log​Level"], [RO D BE], "the current log level of this logger",
  [#mono("pattern")], link(<t-string>)[#"string"], [RO D C], "the pattern used by this logger",
  [#mono("muted")], link(<t-bool>)[#"bool"], [RO D BE], "true if muted",
)
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("mute")], [#mono[()]], "mute this logger",
  [#mono("unmute")], [#mono[()]], "unmute this logger",
  [#mono("set​Level")], [#mono[(#"level": #link(<t-sen-kernel-log-LogLevel>)[#"sen.​kernel.​log.​Log​Level"])]], "unmute this logger",
  [#mono("set​Pattern")], [#mono[(#"pattern": #link(<t-string>)[#"string"])]], "sets the pattern for this logger",
)

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-logmaster-Config>)[#"Config"], "component configuration", <t-sen-components-logmaster-Config>),
)

==== #"Config" #chip("structures", "structure") <t-sen-components-logmaster-Config>
#prose("component configuration")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("target​Bus")], link(<t-string>)[#"string"], "defaults to \"local.log\"",
  [#mono("period")], link(<t-sen-Duration>)[#"sen.​Duration"], "defaults to 500 milliseconds",
)

== #"py"

#prose[3 types.]

#section("Object classes", "classes")

#summary(
  (link(<t-sen-components-py-PythonInterpreter>)[#"Python​Interpreter"], "represents the embedded Python interpreter", <t-sen-components-py-PythonInterpreter>),
)

==== #"PythonInterpreter" #chip("classes", "class") <t-sen-components-py-PythonInterpreter>
#prose("represents the embedded Python interpreter")
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("eval")], [#mono[(#"expr": #link(<t-string>)[#"string"]) \u{2192} #link(<t-string>)[#"string"]]], "evaluates a Python expression",
  [#mono("exec")], [#mono[(#"code": #link(<t-string>)[#"string"])]], "executes a Python code block",
  [#mono("exec​File")], [#mono[(#"file": #link(<t-string>)[#"string"])]], "executes a Python file",
)

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-py-Configuration>)[#"Configuration"], "Overall component configuration", <t-sen-components-py-Configuration>),
)

==== #"Configuration" #chip("structures", "structure") <t-sen-components-py-Configuration>
#prose("Overall component configuration")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("freq​Hz")], link(<t-f32>)[#"f32"], "update frequency in Hertz",
  [#mono("module")], link(<t-string>)[#"string"], "python module to use (name of a python script, without the .py)",
  [#mono("imports")], link(<t-sen-components-py-StringList>)[#"String​List"], "sen packages to import",
  [#mono("bus")], link(<t-string>)[#"string"], "bus where to publish the interpreter object (none by default)",
)

#section("Arrays", "sequences")

#summary(
  (link(<t-sen-components-py-StringList>)[#"String​List"], "", <t-sen-components-py-StringList>),
)

==== #"StringList" #chip("sequences", "sequence") <t-sen-components-py-StringList>
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"String​List"#";"]
#facts[Named by #link(<t-sen-components-py-Configuration>)[#"Configuration"].]

== #"recorder"

#prose[6 types.]

#section("Object classes", "classes")

#summary(
  (link(<t-sen-components-recorder-Recorder>)[#"Recorder"], "represents an recording session", <t-sen-components-recorder-Recorder>),
)

==== #"Recorder" #chip("classes", "class") <t-sen-components-recorder-Recorder>
#prose("represents an recording session")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("settings")], link(<t-sen-components-recorder-RecordingSettings>)[#"Recording​Settings"], [RO S BE], "",
  [#mono("state")], link(<t-sen-components-recorder-RecorderState>)[#"Recorder​State"], [RO D C], "",
)
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("start")], [#mono[()]], "starts (or resumes) recording",
  [#mono("stop")], [#mono[()]], "stops recording",
  [#mono("fetch​Stats")], [#mono[() \u{2192} #link(<t-sen-db-OutStats>)[#"sen.​db.​Out​Stats"]]], "sample the output statistics",
)

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-recorder-Configuration>)[#"Configuration"], "Overall component configuration", <t-sen-components-recorder-Configuration>),
  (link(<t-sen-components-recorder-RecordingSettings>)[#"Recording​Settings"], "Configuration of a predefined recording", <t-sen-components-recorder-RecordingSettings>),
)

==== #"Configuration" #chip("structures", "structure") <t-sen-components-recorder-Configuration>
#prose("Overall component configuration")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("sampling​Period")], link(<t-sen-Duration>)[#"sen.​Duration"], "sampling period for the recordings",
  [#mono("recordings")], link(<t-sen-components-recorder-RecordingSettingsList>)[#"Recording​Settings​List"], "predefined recordings",
  [#mono("bus")], link(<t-string>)[#"string"], "bus to publish the object (local.rec by default)",
)

==== #"RecordingSettings" #chip("structures", "structure") <t-sen-components-recorder-RecordingSettings>
#prose("Configuration of a predefined recording")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the recording (cannot be empty)",
  [#mono("folder")], link(<t-string>)[#"string"], "storage folder",
  [#mono("selections")], link(<t-sen-components-recorder-SelectionList>)[#"Selection​List"], "which objects shall be recorded",
  [#mono("keyframe​Period")], link(<t-sen-Duration>)[#"sen.​Duration"], "period between keyframes (0 means no keyframes)",
  [#mono("index​Keyframes")], link(<t-bool>)[#"bool"], "should we create indexes for keyframes",
  [#mono("index​Objects")], link(<t-bool>)[#"bool"], "should we create indexes for objects",
  [#mono("auto​Start")], link(<t-bool>)[#"bool"], "should we start recording automatically",
)
#facts[Named by #link(<t-sen-components-recorder-Recorder>)[#"Recorder"], #link(<t-sen-components-recorder-RecordingSettingsList>)[#"Recording​Settings​List"].]

#section("Enumerations", "enumerations")

#summary(
  (link(<t-sen-components-recorder-RecorderState>)[#"Recorder​State"], "the state of a recording session", <t-sen-components-recorder-RecorderState>),
)

==== #"RecorderState" #chip("enumerations", "enumeration") <t-sen-components-recorder-RecorderState>
#prose("the state of a recording session")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("stopped", [0]),
  ("recording", [1]),
  ("error", [2]),
)
#facts[Named by #link(<t-sen-components-recorder-Recorder>)[#"Recorder"].]

#section("Arrays", "sequences")

#summary(
  (link(<t-sen-components-recorder-RecordingSettingsList>)[#"Recording​Settings​List"], "List of predefined recordings", <t-sen-components-recorder-RecordingSettingsList>),
  (link(<t-sen-components-recorder-SelectionList>)[#"Selection​List"], "List of selection queries", <t-sen-components-recorder-SelectionList>),
)

==== #"RecordingSettingsList" #chip("sequences", "sequence") <t-sen-components-recorder-RecordingSettingsList>
#prose("List of predefined recordings")
#declared[#kw[sequence]\<#link(<t-sen-components-recorder-RecordingSettings>)[#"Recording​Settings"]\> #"Recording​Settings​List"#";"]
#facts[Named by #link(<t-sen-components-recorder-Configuration>)[#"Configuration"].]

==== #"SelectionList" #chip("sequences", "sequence") <t-sen-components-recorder-SelectionList>
#prose("List of selection queries")
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"Selection​List"#";"]
#facts[Named by #link(<t-sen-components-recorder-RecordingSettings>)[#"Recording​Settings"].]

== #"replayer"

#prose[4 types.]

#section("Object classes", "classes")

#summary(
  (link(<t-sen-components-replayer-Replay>)[#"Replay"], "controls the replay of a given archive", <t-sen-components-replayer-Replay>),
  (link(<t-sen-components-replayer-Replayer>)[#"Replayer"], "represents the replayer component and manages Replay objects.", <t-sen-components-replayer-Replayer>),
)

==== #"Replay" #chip("classes", "class") <t-sen-components-replayer-Replay>
#prose("controls the replay of a given archive")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("archive​Info")], link(<t-sen-db-Summary>)[#"sen.​db.​Summary"], [RO S BE], "information about the archive being replayed",
  [#mono("archive​Path")], link(<t-string>)[#"string"], [RO S BE], "file path to the archive",
  [#mono("status")], link(<t-sen-components-replayer-ReplayStatus>)[#"Replay​Status"], [RO D C], "playback status",
  [#mono("playback​Time")], link(<t-sen-TimeStamp>)[#"sen.​Time​Stamp"], [RO D BE], "playback time",
)
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("play")], [#mono[()]], "starts the playback from the current position",
  [#mono("pause")], [#mono[()]], "hold the playback position, if playing",
  [#mono("stop")], [#mono[()]], "goes to the start of the playback, erasing all objects",
  [#mono("seek")], [#mono[(#"time": #link(<t-sen-TimeStamp>)[#"sen.​Time​Stamp"])]], "restores the playback to the keyframe that's closest to a given time",
  [#mono("advance")], [#mono[(#"time": #link(<t-sen-Duration>)[#"sen.​Duration"])]], "if paused or stopped, advances the playback a given time delta",
)

==== #"Replayer" #chip("classes", "class") <t-sen-components-replayer-Replayer>
#prose("represents the replayer component and manages Replay objects.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("auto​Play")], link(<t-bool>)[#"bool"], [RO S BE], "if true, automatically calls play when opening a replay",
)
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("open")], [#mono[(#"name": #link(<t-string>)[#"string"], #"path": #link(<t-string>)[#"string"])]], "opens an archive in a given path, and creates a named Replay object.",
  [#mono("close")], [#mono[(#"name": #link(<t-string>)[#"string"])]], "closes the named Replay object, if any.",
  [#mono("close​All")], [#mono[()]], "closes all previously opened replays",
)

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-replayer-Configuration>)[#"Configuration"], "Overall component configuration", <t-sen-components-replayer-Configuration>),
)

==== #"Configuration" #chip("structures", "structure") <t-sen-components-replayer-Configuration>
#prose("Overall component configuration")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("bus")], link(<t-string>)[#"string"], "bus to publish the object (local.replay by default)",
  [#mono("sampling​Period")], link(<t-sen-Duration>)[#"sen.​Duration"], "max sampling period for flushing the replayed data",
  [#mono("auto​Open")], link(<t-string>)[#"string"], "optional archive path to auto open on start-up (empty by default)",
  [#mono("auto​Play")], link(<t-bool>)[#"bool"], "if autoOpen is not empty, try to play it on start-up (false by default)",
)

#section("Enumerations", "enumerations")

#summary(
  (link(<t-sen-components-replayer-ReplayStatus>)[#"Replay​Status"], "the state in which a Replay can be", <t-sen-components-replayer-ReplayStatus>),
)

==== #"ReplayStatus" #chip("enumerations", "enumeration") <t-sen-components-replayer-ReplayStatus>
#prose("the state in which a Replay can be")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("stopped", [0]),
  ("paused", [1]),
  ("playing", [2]),
)
#facts[Named by #link(<t-sen-components-replayer-Replay>)[#"Replay"].]

== #"rest"

#prose[37 types.]

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-rest-Argument>)[#"Argument"], "", <t-sen-components-rest-Argument>),
  (link(<t-sen-components-rest-Bus>)[#"Bus"], "", <t-sen-components-rest-Bus>),
  (link(<t-sen-components-rest-BusSummary>)[#"Bus​Summary"], "", <t-sen-components-rest-BusSummary>),
  (link(<t-sen-components-rest-ClientAuth>)[#"Client​Auth"], "", <t-sen-components-rest-ClientAuth>),
  (link(<t-sen-components-rest-Configuration>)[#"Configuration"], "", <t-sen-components-rest-Configuration>),
  (link(<t-sen-components-rest-Error>)[#"Error"], "", <t-sen-components-rest-Error>),
  (link(<t-sen-components-rest-Interest>)[#"Interest"], "", <t-sen-components-rest-Interest>),
  (link(<t-sen-components-rest-InterestSummary>)[#"Interest​Summary"], "", <t-sen-components-rest-InterestSummary>),
  (link(<t-sen-components-rest-Link>)[#"Link"], "", <t-sen-components-rest-Link>),
  (link(<t-sen-components-rest-Notification>)[#"Notification"], "", <t-sen-components-rest-Notification>),
  (link(<t-sen-components-rest-Object>)[#"Object"], "", <t-sen-components-rest-Object>),
  (link(<t-sen-components-rest-ObjectEvent>)[#"Object​Event"], "", <t-sen-components-rest-ObjectEvent>),
  (link(<t-sen-components-rest-ObjectMethod>)[#"Object​Method"], "", <t-sen-components-rest-ObjectMethod>),
  (link(<t-sen-components-rest-ObjectSummary>)[#"Object​Summary"], "", <t-sen-components-rest-ObjectSummary>),
  (link(<t-sen-components-rest-Session>)[#"Session"], "", <t-sen-components-rest-Session>),
  (link(<t-sen-components-rest-SessionSummary>)[#"Session​Summary"], "", <t-sen-components-rest-SessionSummary>),
  (link(<t-sen-components-rest-SubscriptionOptions>)[#"Subscription​Options"], "", <t-sen-components-rest-SubscriptionOptions>),
  (link(<t-sen-components-rest-Subscriptions>)[#"Subscriptions"], "", <t-sen-components-rest-Subscriptions>),
  (link(<t-sen-components-rest-Success>)[#"Success"], "", <t-sen-components-rest-Success>),
  (link(<t-sen-components-rest-Version>)[#"Version"], "", <t-sen-components-rest-Version>),
)

==== #"Argument" #chip("structures", "structure") <t-sen-components-rest-Argument>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "",
  [#mono("description")], link(<t-string>)[#"string"], "",
  [#mono("type")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-components-rest-Arguments>)[#"Arguments"].]

==== #"Bus" #chip("structures", "structure") <t-sen-components-rest-Bus>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("id")], link(<t-string>)[#"string"], "",
  [#mono("status")], link(<t-sen-components-rest-Status>)[#"Status"], "",
  [#mono("objects")], link(<t-sen-components-rest-ObjectsSummary>)[#"Objects​Summary"], "",
)

==== #"BusSummary" #chip("structures", "structure") <t-sen-components-rest-BusSummary>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("id")], link(<t-string>)[#"string"], "",
  [#mono("status")], link(<t-sen-components-rest-Status>)[#"Status"], "",
)
#facts[Named by #link(<t-sen-components-rest-Buses>)[#"Buses"].]

==== #"ClientAuth" #chip("structures", "structure") <t-sen-components-rest-ClientAuth>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("id")], link(<t-string>)[#"string"], "",
)

==== #"Configuration" #chip("structures", "structure") <t-sen-components-rest-Configuration>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("address")], link(<t-string>)[#"string"], "The IP address or hostname the server will bind to",
  [#mono("port")], link(<t-u16>)[#"u16"], "The port number the server will listen on",
  [#mono("freq​Hz")], link(<t-f32>)[#"f32"], "Update frequency in Hertz",
)

==== #"Error" #chip("structures", "structure") <t-sen-components-rest-Error>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("error")], link(<t-string>)[#"string"], "",
)

==== #"Interest" #chip("structures", "structure") <t-sen-components-rest-Interest>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("query")], link(<t-string>)[#"string"], "",
  [#mono("name")], link(<t-string>)[#"string"], "",
)

==== #"InterestSummary" #chip("structures", "structure") <t-sen-components-rest-InterestSummary>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "",
  [#mono("session")], link(<t-string>)[#"string"], "",
  [#mono("bus")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-components-rest-InterestsSummary>)[#"Interests​Summary"].]

==== #"Link" #chip("structures", "structure") <t-sen-components-rest-Link>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("rel")], link(<t-sen-components-rest-RelType>)[#"Rel​Type"], "",
  [#mono("href")], link(<t-string>)[#"string"], "",
  [#mono("method")], link(<t-sen-components-rest-HttpMethod>)[#"Http​Method"], "",
)
#facts[Named by #link(<t-sen-components-rest-Links>)[#"Links"], #link(<t-sen-components-rest-ObjectSummary>)[#"Object​Summary"].]

==== #"Notification" #chip("structures", "structure") <t-sen-components-rest-Notification>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("type")], link(<t-sen-components-rest-NotificationType>)[#"Notification​Type"], "",
  [#mono("interest")], link(<t-string>)[#"string"], "",
  [#mono("time")], link(<t-sen-TimeStamp>)[#"sen.​Time​Stamp"], "",
  [#mono("data")], link(<t-string>)[#"string"], "",
)

==== #"Object" #chip("structures", "structure") <t-sen-components-rest-Object>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("object​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("name")], link(<t-string>)[#"string"], "",
  [#mono("class​Name")], link(<t-string>)[#"string"], "",
  [#mono("local​Name")], link(<t-string>)[#"string"], "",
  [#mono("description")], link(<t-string>)[#"string"], "",
  [#mono("links")], link(<t-sen-components-rest-Links>)[#"Links"], "",
)

==== #"ObjectEvent" #chip("structures", "structure") <t-sen-components-rest-ObjectEvent>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "",
  [#mono("description")], link(<t-string>)[#"string"], "",
)

==== #"ObjectMethod" #chip("structures", "structure") <t-sen-components-rest-ObjectMethod>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "",
  [#mono("description")], link(<t-string>)[#"string"], "",
  [#mono("ret​Type")], link(<t-string>)[#"string"], "",
  [#mono("args")], link(<t-sen-components-rest-Arguments>)[#"Arguments"], "",
)

==== #"ObjectSummary" #chip("structures", "structure") <t-sen-components-rest-ObjectSummary>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("object​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("name")], link(<t-string>)[#"string"], "",
  [#mono("class​Name")], link(<t-string>)[#"string"], "",
  [#mono("local​Name")], link(<t-string>)[#"string"], "",
  [#mono("link")], link(<t-sen-components-rest-Link>)[#"Link"], "",
)
#facts[Named by #link(<t-sen-components-rest-ObjectsSummary>)[#"Objects​Summary"].]

==== #"Session" #chip("structures", "structure") <t-sen-components-rest-Session>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "",
)

==== #"SessionSummary" #chip("structures", "structure") <t-sen-components-rest-SessionSummary>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "",
  [#mono("buses")], link(<t-sen-components-rest-BusNames>)[#"Bus​Names"], "",
)

==== #"SubscriptionOptions" #chip("structures", "structure") <t-sen-components-rest-SubscriptionOptions>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("max​Update​Time")], link(<t-sen-Duration>)[#"sen.​Duration"], "Max. update time",
)

==== #"Subscriptions" #chip("structures", "structure") <t-sen-components-rest-Subscriptions>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("properties")], link(<t-sen-components-rest-Properties>)[#"Properties"], "",
  [#mono("events")], link(<t-sen-components-rest-Events>)[#"Events"], "",
)

==== #"Success" #chip("structures", "structure") <t-sen-components-rest-Success>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("msg")], link(<t-string>)[#"string"], "",
)

==== #"Version" #chip("structures", "structure") <t-sen-components-rest-Version>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("version")], link(<t-string>)[#"string"], "",
)

#section("Enumerations", "enumerations")

#summary(
  (link(<t-sen-components-rest-HttpMethod>)[#"Http​Method"], "", <t-sen-components-rest-HttpMethod>),
  (link(<t-sen-components-rest-InvokeStatus>)[#"Invoke​Status"], "", <t-sen-components-rest-InvokeStatus>),
  (link(<t-sen-components-rest-NotificationType>)[#"Notification​Type"], "", <t-sen-components-rest-NotificationType>),
  (link(<t-sen-components-rest-RelType>)[#"Rel​Type"], "", <t-sen-components-rest-RelType>),
  (link(<t-sen-components-rest-Status>)[#"Status"], "", <t-sen-components-rest-Status>),
)

==== #"HttpMethod" #chip("enumerations", "enumeration") <t-sen-components-rest-HttpMethod>
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("http​Get", [0]),
  ("http​Post", [1]),
  ("http​Put", [2]),
  ("http​Delete", [3]),
  ("http​Option", [4]),
)
#facts[Named by #link(<t-sen-components-rest-Link>)[#"Link"].]

==== #"InvokeStatus" #chip("enumerations", "enumeration") <t-sen-components-rest-InvokeStatus>
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("pending", [0]),
  ("finished", [1]),
  ("failed", [2]),
)

==== #"NotificationType" #chip("enumerations", "enumeration") <t-sen-components-rest-NotificationType>
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("evt", [0]),
  ("invoke", [1]),
  ("property", [2]),
  ("object​Added", [3]),
  ("object​Removed", [4]),
  ("count", [5]),
)
#facts[Named by #link(<t-sen-components-rest-Notification>)[#"Notification"].]

==== #"RelType" #chip("enumerations", "enumeration") <t-sen-components-rest-RelType>
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("method", [0]),
  ("setter", [1]),
  ("getter", [2]),
  ("evt", [3]),
  ("property", [4]),
  ("property​Subscribe", [5]),
  ("property​Unsubscribe", [6]),
  ("def", [7]),
)
#facts[Named by #link(<t-sen-components-rest-Link>)[#"Link"].]

==== #"Status" #chip("enumerations", "enumeration") <t-sen-components-rest-Status>
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("open", [0]),
  ("close", [1]),
)
#facts[Named by #link(<t-sen-components-rest-Bus>)[#"Bus"], #link(<t-sen-components-rest-BusSummary>)[#"Bus​Summary"].]

#section("Arrays", "sequences")

#summary(
  (link(<t-sen-components-rest-Arguments>)[#"Arguments"], "", <t-sen-components-rest-Arguments>),
  (link(<t-sen-components-rest-BusNames>)[#"Bus​Names"], "", <t-sen-components-rest-BusNames>),
  (link(<t-sen-components-rest-Buses>)[#"Buses"], "", <t-sen-components-rest-Buses>),
  (link(<t-sen-components-rest-Events>)[#"Events"], "", <t-sen-components-rest-Events>),
  (link(<t-sen-components-rest-InterestsSummary>)[#"Interests​Summary"], "", <t-sen-components-rest-InterestsSummary>),
  (link(<t-sen-components-rest-Links>)[#"Links"], "", <t-sen-components-rest-Links>),
  (link(<t-sen-components-rest-ObjectsSummary>)[#"Objects​Summary"], "", <t-sen-components-rest-ObjectsSummary>),
  (link(<t-sen-components-rest-Properties>)[#"Properties"], "", <t-sen-components-rest-Properties>),
  (link(<t-sen-components-rest-Sessions>)[#"Sessions"], "", <t-sen-components-rest-Sessions>),
)

==== #"Arguments" #chip("sequences", "sequence") <t-sen-components-rest-Arguments>
#declared[#kw[sequence]\<#link(<t-sen-components-rest-Argument>)[#"Argument"]\> #"Arguments"#";"]
#facts[Named by #link(<t-sen-components-rest-ObjectMethod>)[#"Object​Method"].]

==== #"BusNames" #chip("sequences", "sequence") <t-sen-components-rest-BusNames>
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"Bus​Names"#";"]
#facts[Named by #link(<t-sen-components-rest-SessionSummary>)[#"Session​Summary"].]

==== #"Buses" #chip("sequences", "sequence") <t-sen-components-rest-Buses>
#declared[#kw[sequence]\<#link(<t-sen-components-rest-BusSummary>)[#"Bus​Summary"]\> #"Buses"#";"]

==== #"Events" #chip("sequences", "sequence") <t-sen-components-rest-Events>
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"Events"#";"]
#facts[Named by #link(<t-sen-components-rest-Subscriptions>)[#"Subscriptions"].]

==== #"InterestsSummary" #chip("sequences", "sequence") <t-sen-components-rest-InterestsSummary>
#declared[#kw[sequence]\<#link(<t-sen-components-rest-InterestSummary>)[#"Interest​Summary"]\> #"Interests​Summary"#";"]

==== #"Links" #chip("sequences", "sequence") <t-sen-components-rest-Links>
#declared[#kw[sequence]\<#link(<t-sen-components-rest-Link>)[#"Link"]\> #"Links"#";"]
#facts[Named by #link(<t-sen-components-rest-Object>)[#"Object"].]

==== #"ObjectsSummary" #chip("sequences", "sequence") <t-sen-components-rest-ObjectsSummary>
#declared[#kw[sequence]\<#link(<t-sen-components-rest-ObjectSummary>)[#"Object​Summary"]\> #"Objects​Summary"#";"]
#facts[Named by #link(<t-sen-components-rest-Bus>)[#"Bus"].]

==== #"Properties" #chip("sequences", "sequence") <t-sen-components-rest-Properties>
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"Properties"#";"]
#facts[Named by #link(<t-sen-components-rest-Subscriptions>)[#"Subscriptions"].]

==== #"Sessions" #chip("sequences", "sequence") <t-sen-components-rest-Sessions>
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"Sessions"#";"]

#section("Optionals", "optionals")

#summary(
  (link(<t-sen-components-rest-ThreadPoolSize>)[#"Thread​Pool​Size"], "", <t-sen-components-rest-ThreadPoolSize>),
  (link(<t-sen-components-rest-UpdateFrequency>)[#"Update​Frequency"], "", <t-sen-components-rest-UpdateFrequency>),
  (link(<t-sen-components-rest-UpdateTime>)[#"Update​Time"], "", <t-sen-components-rest-UpdateTime>),
)

==== #"ThreadPoolSize" #chip("optionals", "optional") <t-sen-components-rest-ThreadPoolSize>
#declared[#kw[optional]\<#link(<t-u16>)[#"u16"]\> #"Thread​Pool​Size"#";"]

==== #"UpdateFrequency" #chip("optionals", "optional") <t-sen-components-rest-UpdateFrequency>
#declared[#kw[optional]\<#link(<t-f32>)[#"f32"]\> #"Update​Frequency"#";"]

==== #"UpdateTime" #chip("optionals", "optional") <t-sen-components-rest-UpdateTime>
#declared[#kw[optional]\<#link(<t-sen-Duration>)[#"sen.​Duration"]\> #"Update​Time"#";"]

== #"shell"

#prose[27 types.]

#section("Object classes", "classes")

#summary(
  (link(<t-sen-components-shell-Shell>)[#"Shell"], "Provides a text-based interface to interact with the a sen kernel", <t-sen-components-shell-Shell>),
)

==== #"Shell" #chip("classes", "class") <t-sen-components-shell-Shell>
#prose("Provides a text-based interface to interact with the a sen kernel")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("config")], link(<t-sen-components-shell-Configuration>)[#"Configuration"], [RO S BE], "service configuration",
)
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("clear")], [#mono[()]], "clears the screen",
  [#mono("help")], [#mono[()]], "basic usage help",
  [#mono("history")], [#mono[()]], "lists previous commands",
  [#mono("ls")], [#mono[()]], "lists all the existing objects and sources.",
  [#mono("open")], [#mono[(#"source": #link(<t-string>)[#"string"])]], "opens a source (bus or session) without any condition",
  [#mono("query")], [#mono[(#"name": #link(<t-string>)[#"string"], #"condition": #link(<t-string>)[#"string"])]], "opens a named source using a condition expression",
  [#mono("close")], [#mono[(#"source": #link(<t-string>)[#"string"])]], "closes a previously-opened source (session, bus or query)",
  [#mono("info")], [#mono[(#"term": #link(<t-string>)[#"string"])]], "prints information about a type or instance",
  [#mono("shutdown")], [#mono[()]], "requests the kernel to be shut-down",
  [#mono("src")], [#mono[()]], "lists the current sources",
)

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-components-shell-CPrint>)[#"CPrint"], "", <t-sen-components-shell-CPrint>),
  (link(<t-sen-components-shell-ClearCurrentLine>)[#"Clear​Current​Line"], "", <t-sen-components-shell-ClearCurrentLine>),
  (link(<t-sen-components-shell-ClearRemainingCurrentLine>)[#"Clear​Remaining​Current​Line"], "", <t-sen-components-shell-ClearRemainingCurrentLine>),
  (link(<t-sen-components-shell-ClearScreen>)[#"Clear​Screen"], "", <t-sen-components-shell-ClearScreen>),
  (link(<t-sen-components-shell-Configuration>)[#"Configuration"], "Configuration params for the shell", <t-sen-components-shell-Configuration>),
  (link(<t-sen-components-shell-HideCursor>)[#"Hide​Cursor"], "", <t-sen-components-shell-HideCursor>),
  (link(<t-sen-components-shell-MoveCursorAllLeft>)[#"Move​Cursor​All​Left"], "", <t-sen-components-shell-MoveCursorAllLeft>),
  (link(<t-sen-components-shell-MoveCursorDown>)[#"Move​Cursor​Down"], "", <t-sen-components-shell-MoveCursorDown>),
  (link(<t-sen-components-shell-MoveCursorLeft>)[#"Move​Cursor​Left"], "", <t-sen-components-shell-MoveCursorLeft>),
  (link(<t-sen-components-shell-MoveCursorRight>)[#"Move​Cursor​Right"], "", <t-sen-components-shell-MoveCursorRight>),
  (link(<t-sen-components-shell-MoveCursorUp>)[#"Move​Cursor​Up"], "", <t-sen-components-shell-MoveCursorUp>),
  (link(<t-sen-components-shell-NewLine>)[#"New​Line"], "", <t-sen-components-shell-NewLine>),
  (link(<t-sen-components-shell-Print>)[#"Print"], "", <t-sen-components-shell-Print>),
  (link(<t-sen-components-shell-Query>)[#"Query"], "Holds information about a named query", <t-sen-components-shell-Query>),
  (link(<t-sen-components-shell-RestoreCursorPosition>)[#"Restore​Cursor​Position"], "", <t-sen-components-shell-RestoreCursorPosition>),
  (link(<t-sen-components-shell-SaveCursorPosition>)[#"Save​Cursor​Position"], "", <t-sen-components-shell-SaveCursorPosition>),
  (link(<t-sen-components-shell-SetBgColor>)[#"Set​Bg​Color"], "", <t-sen-components-shell-SetBgColor>),
  (link(<t-sen-components-shell-SetFgColor>)[#"Set​Fg​Color"], "", <t-sen-components-shell-SetFgColor>),
  (link(<t-sen-components-shell-SetWindowTitle>)[#"Set​Window​Title"], "", <t-sen-components-shell-SetWindowTitle>),
  (link(<t-sen-components-shell-ShowCursor>)[#"Show​Cursor"], "", <t-sen-components-shell-ShowCursor>),
)

==== #"CPrint" #chip("structures", "structure") <t-sen-components-shell-CPrint>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("flags")], link(<t-u32>)[#"u32"], "",
  [#mono("color")], link(<t-u32>)[#"u32"], "",
  [#mono("text")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"ClearCurrentLine" #chip("structures", "structure") <t-sen-components-shell-ClearCurrentLine>
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"ClearRemainingCurrentLine" #chip("structures", "structure") <t-sen-components-shell-ClearRemainingCurrentLine>
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"ClearScreen" #chip("structures", "structure") <t-sen-components-shell-ClearScreen>
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"Configuration" #chip("structures", "structure") <t-sen-components-shell-Configuration>
#prose("Configuration params for the shell")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("prompt")], link(<t-string>)[#"string"], "defaults to 'sen> '",
  [#mono("time​Style")], link(<t-sen-components-shell-TimeStyle>)[#"Time​Style"], "how to display time",
  [#mono("buffer​Style")], link(<t-sen-components-shell-BufferStyle>)[#"Buffer​Style"], "how to display buffers",
  [#mono("server​Enabled")], link(<t-bool>)[#"bool"], "enables remote shell access (false by default)",
  [#mono("server​Port")], link(<t-u16>)[#"u16"], "0 means default",
  [#mono("no​Logo")], link(<t-bool>)[#"bool"], "true if you don't want a welcoming logo",
  [#mono("open")], link(<t-sen-components-shell-SourcesList>)[#"Sources​List"], "list of sources to open automatically",
  [#mono("query")], link(<t-sen-components-shell-QueryList>)[#"Query​List"], "list of queries to create automatically",
  [#mono("log​Bus")], link(<t-string>)[#"string"], "where to find the logmaster, if any (default: local.log)",
)
#facts[Named by #link(<t-sen-components-shell-Shell>)[#"Shell"].]

==== #"HideCursor" #chip("structures", "structure") <t-sen-components-shell-HideCursor>
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"MoveCursorAllLeft" #chip("structures", "structure") <t-sen-components-shell-MoveCursorAllLeft>
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"MoveCursorDown" #chip("structures", "structure") <t-sen-components-shell-MoveCursorDown>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("cells")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"MoveCursorLeft" #chip("structures", "structure") <t-sen-components-shell-MoveCursorLeft>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("cells")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"MoveCursorRight" #chip("structures", "structure") <t-sen-components-shell-MoveCursorRight>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("cells")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"MoveCursorUp" #chip("structures", "structure") <t-sen-components-shell-MoveCursorUp>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("cells")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"NewLine" #chip("structures", "structure") <t-sen-components-shell-NewLine>
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"Print" #chip("structures", "structure") <t-sen-components-shell-Print>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("text")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"Query" #chip("structures", "structure") <t-sen-components-shell-Query>
#prose("Holds information about a named query")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "the name of the query (cannot contain spaces or dots)",
  [#mono("selection")], link(<t-string>)[#"string"], "the query expression",
)
#facts[Named by #link(<t-sen-components-shell-QueryList>)[#"Query​List"].]

==== #"RestoreCursorPosition" #chip("structures", "structure") <t-sen-components-shell-RestoreCursorPosition>
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"SaveCursorPosition" #chip("structures", "structure") <t-sen-components-shell-SaveCursorPosition>
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"SetBgColor" #chip("structures", "structure") <t-sen-components-shell-SetBgColor>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("color")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"SetFgColor" #chip("structures", "structure") <t-sen-components-shell-SetFgColor>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("color")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"SetWindowTitle" #chip("structures", "structure") <t-sen-components-shell-SetWindowTitle>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("text")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

==== #"ShowCursor" #chip("structures", "structure") <t-sen-components-shell-ShowCursor>
#facts[Named by #link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"].]

#section("Enumerations", "enumerations")

#summary(
  (link(<t-sen-components-shell-BufferStyle>)[#"Buffer​Style"], "How buffers should be printed", <t-sen-components-shell-BufferStyle>),
  (link(<t-sen-components-shell-ListStyle>)[#"List​Style"], "How lists should be printed", <t-sen-components-shell-ListStyle>),
  (link(<t-sen-components-shell-TimeStyle>)[#"Time​Style"], "How timestamps should be printed", <t-sen-components-shell-TimeStyle>),
)

==== #"BufferStyle" #chip("enumerations", "enumeration") <t-sen-components-shell-BufferStyle>
#prose("How buffers should be printed")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("size", [0]),
  ("hexdump", [1]),
)
#facts[Named by #link(<t-sen-components-shell-Configuration>)[#"Configuration"].]

==== #"ListStyle" #chip("enumerations", "enumeration") <t-sen-components-shell-ListStyle>
#prose("How lists should be printed")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("tree​Style", [0]),
  ("plain​Style", [1]),
)

==== #"TimeStyle" #chip("enumerations", "enumeration") <t-sen-components-shell-TimeStyle>
#prose("How timestamps should be printed")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("timestamp​UTC", [0]),
  ("timestamp​Local", [1]),
)
#facts[Named by #link(<t-sen-components-shell-Configuration>)[#"Configuration"].]

#section("Variant records", "variants")

#summary(
  (link(<t-sen-components-shell-TerminalCmd>)[#"Terminal​Cmd"], "", <t-sen-components-shell-TerminalCmd>),
)

==== #"TerminalCmd" #chip("variants", "variant") <t-sen-components-shell-TerminalCmd>
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-components-shell-Print>)[#"Print"], "",
  link(<t-sen-components-shell-CPrint>)[#"CPrint"], "",
  link(<t-sen-components-shell-NewLine>)[#"New​Line"], "",
  link(<t-sen-components-shell-MoveCursorLeft>)[#"Move​Cursor​Left"], "",
  link(<t-sen-components-shell-MoveCursorRight>)[#"Move​Cursor​Right"], "",
  link(<t-sen-components-shell-MoveCursorUp>)[#"Move​Cursor​Up"], "",
  link(<t-sen-components-shell-MoveCursorDown>)[#"Move​Cursor​Down"], "",
  link(<t-sen-components-shell-MoveCursorAllLeft>)[#"Move​Cursor​All​Left"], "",
  link(<t-sen-components-shell-SetFgColor>)[#"Set​Fg​Color"], "",
  link(<t-sen-components-shell-SetBgColor>)[#"Set​Bg​Color"], "",
  link(<t-sen-components-shell-ClearCurrentLine>)[#"Clear​Current​Line"], "",
  link(<t-sen-components-shell-ClearRemainingCurrentLine>)[#"Clear​Remaining​Current​Line"], "",
  link(<t-sen-components-shell-HideCursor>)[#"Hide​Cursor"], "",
  link(<t-sen-components-shell-ShowCursor>)[#"Show​Cursor"], "",
  link(<t-sen-components-shell-SaveCursorPosition>)[#"Save​Cursor​Position"], "",
  link(<t-sen-components-shell-RestoreCursorPosition>)[#"Restore​Cursor​Position"], "",
  link(<t-sen-components-shell-ClearScreen>)[#"Clear​Screen"], "",
  link(<t-sen-components-shell-SetWindowTitle>)[#"Set​Window​Title"], "",
)

#section("Arrays", "sequences")

#summary(
  (link(<t-sen-components-shell-QueryList>)[#"Query​List"], "", <t-sen-components-shell-QueryList>),
  (link(<t-sen-components-shell-SourcesList>)[#"Sources​List"], "", <t-sen-components-shell-SourcesList>),
)

==== #"QueryList" #chip("sequences", "sequence") <t-sen-components-shell-QueryList>
#declared[#kw[sequence]\<#link(<t-sen-components-shell-Query>)[#"Query"]\> #"Query​List"#";"]
#facts[Named by #link(<t-sen-components-shell-Configuration>)[#"Configuration"].]

==== #"SourcesList" #chip("sequences", "sequence") <t-sen-components-shell-SourcesList>
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"Sources​List"#";"]
#facts[Named by #link(<t-sen-components-shell-Configuration>)[#"Configuration"].]

= #"db"

#prose[3 types.]

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-db-OutSettings>)[#"Out​Settings"], "Settings for outputs", <t-sen-db-OutSettings>),
  (link(<t-sen-db-OutStats>)[#"Out​Stats"], "Statistics of writing", <t-sen-db-OutStats>),
  (link(<t-sen-db-Summary>)[#"Summary"], "Summary of an archive", <t-sen-db-Summary>),
)

==== #"OutSettings" #chip("structures", "structure") <t-sen-db-OutSettings>
#prose("Settings for outputs")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the archive (cannot be empty)",
  [#mono("folder")], link(<t-string>)[#"string"], "storage folder",
  [#mono("index​Keyframes")], link(<t-bool>)[#"bool"], "should we create indexes for keyframes",
)

==== #"OutStats" #chip("structures", "structure") <t-sen-db-OutStats>
#prose("Statistics of writing")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("max​Queue​Size")], link(<t-u64>)[#"u64"], "in bytes",
)
#facts[Named by #link(<t-sen-components-recorder-Recorder>)[#"sen.​components.​recorder.​Recorder"].]

==== #"Summary" #chip("structures", "structure") <t-sen-db-Summary>
#prose("Summary of an archive")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("first​Time")], link(<t-sen-TimeStamp>)[#"sen.​Time​Stamp"], "the time of the first event",
  [#mono("last​Time")], link(<t-sen-TimeStamp>)[#"sen.​Time​Stamp"], "the time of the last event",
  [#mono("keyframe​Count")], link(<t-u32>)[#"u32"], "number of written keyframes",
  [#mono("object​Count")], link(<t-u32>)[#"u32"], "number of recorded objects",
  [#mono("type​Count")], link(<t-u32>)[#"u32"], "number of types written",
  [#mono("annotation​Count")], link(<t-u32>)[#"u32"], "number of annotations",
  [#mono("indexed​Object​Count")], link(<t-u32>)[#"u32"], "number of indexed objects",
)
#facts[Named by #link(<t-sen-components-replayer-Replay>)[#"sen.​components.​replayer.​Replay"].]

= #"kernel"

#prose[160 types.]

#section("Object classes", "classes")

#summary(
  (link(<t-sen-kernel-KernelApi>)[#"Kernel​Api"], "Represents the kernel", <t-sen-kernel-KernelApi>),
  (link(<t-sen-kernel-VirtualClock>)[#"Virtual​Clock"], "Allows inspecting and controlling the advance of virtual time and associated component iteration", <t-sen-kernel-VirtualClock>),
  (link(<t-sen-kernel-VirtualKernelClock>)[#"Virtual​Kernel​Clock"], "A clock for all the components loaded by the kernel", <t-sen-kernel-VirtualKernelClock>),
  (link(<t-sen-kernel-VirtualMasterClock>)[#"Virtual​Master​Clock"], "Controls all the VirtualKernelClock that it discovers in the bus where it's published. One step reaches the next component due to run: each clock reports how long until its own next one, and the soonest wins", <t-sen-kernel-VirtualMasterClock>),
)

==== #"KernelApi" #chip("classes", "class") <t-sen-kernel-KernelApi>
#prose("Represents the kernel")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("build​Info")], link(<t-sen-kernel-BuildInfo>)[#"Build​Info"], [RO S BE], "kernel build information",
)
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("shutdown")], [#mono[()]], "requests the kernel to shut down",
  [#mono("get​Units")], [#mono[() \u{2192} #link(<t-sen-kernel-UnitList>)[#"Unit​List"]]], "registered units",
  [#mono("get​Types")], [#mono[() \u{2192} #link(<t-sen-kernel-StringList>)[#"String​List"]]], "registered types, by name",
  [#mono("get​Config")], [#mono[() \u{2192} #link(<t-sen-kernel-KernelParams>)[#"Kernel​Params"]]], "configuration of the kernel execution",
)

==== #"VirtualClock" #chip("classes", "class") <t-sen-kernel-VirtualClock>
#prose("Allows inspecting and controlling the advance of virtual time and associated component iteration")
#sen-table(
  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),
  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),
  [#mono("time")], link(<t-sen-TimeStamp>)[#"sen.​Time​Stamp"], [RO D BE], "the current virtual time",
)

==== #"VirtualKernelClock" #chip("classes", "class") <t-sen-kernel-VirtualKernelClock>
#prose("A clock for all the components loaded by the kernel")
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("process​No​Flush")], [#mono[(#"delta": #link(<t-sen-Duration>)[#"sen.​Duration"]) \u{2192} #link(<t-sen-Duration>)[#"sen.​Duration"]]], "updates the time, drains the inputs, and cycles components - doesn't flush outputs",
  [#mono("flush​Outputs")], [#mono[() \u{2192} #link(<t-sen-Duration>)[#"sen.​Duration"]]], "makes component outputs visible to other components and returns the next step duration",
)

==== #"VirtualMasterClock" #chip("classes", "class") <t-sen-kernel-VirtualMasterClock>
#prose("Controls all the VirtualKernelClock that it discovers in the bus where it's published. One step reaches the next component due to run: each clock reports how long until its own next one, and the soonest wins")
#sen-table(
  columns: (COL-NAME, 0.55fr, 1fr),
  table.header([*Method*], [*Signature*], [*Description*]),
  [#mono("advance​Time")], [#mono[(#"duration": #link(<t-sen-Duration>)[#"sen.​Duration"]) \u{2192} #link(<t-sen-Duration>)[#"sen.​Duration"]]], "steps until the virtual time advanced reaches duration, rounding up to a whole step. Returns the wall time the call took",
  [#mono("step")], [#mono[() \u{2192} #link(<t-sen-Duration>)[#"sen.​Duration"]]], "single step forward",
  [#mono("steps")], [#mono[(#"count": #link(<t-u64>)[#"u64"]) \u{2192} #link(<t-sen-Duration>)[#"sen.​Duration"]]], "perform 'count' steps forward",
)

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-kernel-AliasTypeSpec>)[#"Alias​Type​Spec"], "data of an aliased type", <t-sen-kernel-AliasTypeSpec>),
  (link(<t-sen-kernel-AliasTypeSpecV4>)[#"Alias​Type​Spec​V4"], "", <t-sen-kernel-AliasTypeSpecV4>),
  (link(<t-sen-kernel-AliasTypeSpecV5>)[#"Alias​Type​Spec​V5"], "", <t-sen-kernel-AliasTypeSpecV5>),
  (link(<t-sen-kernel-ArgSpec>)[#"Arg​Spec"], "an argument of a callable (methods and events)", <t-sen-kernel-ArgSpec>),
  (link(<t-sen-kernel-ArgSpecV4>)[#"Arg​Spec​V4"], "", <t-sen-kernel-ArgSpecV4>),
  (link(<t-sen-kernel-ArgSpecV5>)[#"Arg​Spec​V5"], "", <t-sen-kernel-ArgSpecV5>),
  (link(<t-sen-kernel-BuildInfo>)[#"Build​Info"], "Build-related information", <t-sen-kernel-BuildInfo>),
  (link(<t-sen-kernel-BuiltComponentParams>)[#"Built​Component​Params"], "Parameters related to components build by the kernel", <t-sen-kernel-BuiltComponentParams>),
  (link(<t-sen-kernel-BusAddress>)[#"Bus​Address"], "Address to connect to a bus", <t-sen-kernel-BusAddress>),
  (link(<t-sen-kernel-ClassTypeSpec>)[#"Class​Type​Spec"], "spec of a class", <t-sen-kernel-ClassTypeSpec>),
  (link(<t-sen-kernel-ClassTypeSpecV4>)[#"Class​Type​Spec​V4"], "", <t-sen-kernel-ClassTypeSpecV4>),
  (link(<t-sen-kernel-ClassTypeSpecV5>)[#"Class​Type​Spec​V5"], "", <t-sen-kernel-ClassTypeSpecV5>),
  (link(<t-sen-kernel-ComponentConfig>)[#"Component​Config"], "Basic runtime configuration for a component", <t-sen-kernel-ComponentConfig>),
  (link(<t-sen-kernel-ComponentInfo>)[#"Component​Info"], "Basic information of a kernel component", <t-sen-kernel-ComponentInfo>),
  (link(<t-sen-kernel-CustomTypeSpec>)[#"Custom​Type​Spec"], "spec of a custom type", <t-sen-kernel-CustomTypeSpec>),
  (link(<t-sen-kernel-CustomTypeSpecV4>)[#"Custom​Type​Spec​V4"], "", <t-sen-kernel-CustomTypeSpecV4>),
  (link(<t-sen-kernel-CustomTypeSpecV5>)[#"Custom​Type​Spec​V5"], "", <t-sen-kernel-CustomTypeSpecV5>),
  (link(<t-sen-kernel-EnumTypeSpec>)[#"Enum​Type​Spec"], "spec of an enum type", <t-sen-kernel-EnumTypeSpec>),
  (link(<t-sen-kernel-EnumTypeSpecV4>)[#"Enum​Type​Spec​V4"], "", <t-sen-kernel-EnumTypeSpecV4>),
  (link(<t-sen-kernel-EnumTypeSpecV5>)[#"Enum​Type​Spec​V5"], "", <t-sen-kernel-EnumTypeSpecV5>),
  (link(<t-sen-kernel-EnumeratorSpec>)[#"Enumerator​Spec"], "an enumerator", <t-sen-kernel-EnumeratorSpec>),
  (link(<t-sen-kernel-EnumeratorSpecV4>)[#"Enumerator​Spec​V4"], "", <t-sen-kernel-EnumeratorSpecV4>),
  (link(<t-sen-kernel-EnumeratorSpecV5>)[#"Enumerator​Spec​V5"], "", <t-sen-kernel-EnumeratorSpecV5>),
  (link(<t-sen-kernel-EnvVar>)[#"Env​Var"], "Information about an environment variable", <t-sen-kernel-EnvVar>),
  (link(<t-sen-kernel-ErrorData>)[#"Error​Data"], "Information related to the error", <t-sen-kernel-ErrorData>),
  (link(<t-sen-kernel-ErrorReport>)[#"Error​Report"], "Information about a detected error", <t-sen-kernel-ErrorReport>),
  (link(<t-sen-kernel-EventSpec>)[#"Event​Spec"], "spec for an event", <t-sen-kernel-EventSpec>),
  (link(<t-sen-kernel-EventSpecV4>)[#"Event​Spec​V4"], "", <t-sen-kernel-EventSpecV4>),
  (link(<t-sen-kernel-EventSpecV5>)[#"Event​Spec​V5"], "", <t-sen-kernel-EventSpecV5>),
  (link(<t-sen-kernel-ExecError>)[#"Exec​Error"], "Holds details of an execution error", <t-sen-kernel-ExecError>),
  (link(<t-sen-kernel-KernelParams>)[#"Kernel​Params"], "Parameters to configure kernel execution", <t-sen-kernel-KernelParams>),
  (link(<t-sen-kernel-LoadedComponentParams>)[#"Loaded​Component​Params"], "Parameters related to a component that will be loaded by the kernel", <t-sen-kernel-LoadedComponentParams>),
  (link(<t-sen-kernel-MethodSpec>)[#"Method​Spec"], "spec for methods", <t-sen-kernel-MethodSpec>),
  (link(<t-sen-kernel-MethodSpecV4>)[#"Method​Spec​V4"], "", <t-sen-kernel-MethodSpecV4>),
  (link(<t-sen-kernel-MethodSpecV5>)[#"Method​Spec​V5"], "", <t-sen-kernel-MethodSpecV5>),
  (link(<t-sen-kernel-NetworkFootprint>)[#"Network​Footprint"], "Network endpoints predicted offline or observed at runtime for one process", <t-sen-kernel-NetworkFootprint>),
  (link(<t-sen-kernel-NetworkFootprintAddressRange>)[#"Network​Footprint​Address​Range"], "Inclusive IPv4 address range excluded from multicast allocation", <t-sen-kernel-NetworkFootprintAddressRange>),
  (link(<t-sen-kernel-NetworkFootprintBus>)[#"Network​Footprint​Bus"], "Multicast endpoint allocated to a bus", <t-sen-kernel-NetworkFootprintBus>),
  (link(<t-sen-kernel-NetworkFootprintBusIdentity>)[#"Network​Footprint​Bus​Identity"], "Names and IDs that identify a bus", <t-sen-kernel-NetworkFootprintBusIdentity>),
  (link(<t-sen-kernel-NetworkFootprintCollision>)[#"Network​Footprint​Collision"], "Multicast allocation capacity and collision information", <t-sen-kernel-NetworkFootprintCollision>),
  (link(<t-sen-kernel-NetworkFootprintMulticast>)[#"Network​Footprint​Multicast"], "Multicast information in the network footprint", <t-sen-kernel-NetworkFootprintMulticast>),
  (link(<t-sen-kernel-NetworkFootprintPort>)[#"Network​Footprint​Port"], "Socket type, port selection mode and port value", <t-sen-kernel-NetworkFootprintPort>),
  (link(<t-sen-kernel-NetworkFootprintPortExclusions>)[#"Network​Footprint​Port​Exclusions"], "Port ranges that cannot be used", <t-sen-kernel-NetworkFootprintPortExclusions>),
  (link(<t-sen-kernel-NetworkFootprintPortRange>)[#"Network​Footprint​Port​Range"], "Inclusive range from which a port may be selected", <t-sen-kernel-NetworkFootprintPortRange>),
  (link(<t-sen-kernel-NetworkFootprintSelfCollision>)[#"Network​Footprint​Self​Collision"], "Two buses allocated to the same multicast group", <t-sen-kernel-NetworkFootprintSelfCollision>),
  (link(<t-sen-kernel-OpFinished>)[#"Op​Finished"], "An operation is complete", <t-sen-kernel-OpFinished>),
  (link(<t-sen-kernel-OpNotFinished>)[#"Op​Not​Finished"], "An operation is not complete", <t-sen-kernel-OpNotFinished>),
  (link(<t-sen-kernel-OptionalTypeSpec>)[#"Optional​Type​Spec"], "data of an optional type", <t-sen-kernel-OptionalTypeSpec>),
  (link(<t-sen-kernel-OptionalTypeSpecV4>)[#"Optional​Type​Spec​V4"], "", <t-sen-kernel-OptionalTypeSpecV4>),
  (link(<t-sen-kernel-OptionalTypeSpecV5>)[#"Optional​Type​Spec​V5"], "", <t-sen-kernel-OptionalTypeSpecV5>),
  (link(<t-sen-kernel-PrecisionSleep>)[#"Precision​Sleep"], "Be more precise at the expense of some CPU cycles.", <t-sen-kernel-PrecisionSleep>),
  (link(<t-sen-kernel-ProcessData>)[#"Process​Data"], "Information about the process", <t-sen-kernel-ProcessData>),
  (link(<t-sen-kernel-ProcessInfo>)[#"Process​Info"], "Basic information about a process", <t-sen-kernel-ProcessInfo>),
  (link(<t-sen-kernel-PropertySpec>)[#"Property​Spec"], "spec of a property", <t-sen-kernel-PropertySpec>),
  (link(<t-sen-kernel-PropertySpecV4>)[#"Property​Spec​V4"], "", <t-sen-kernel-PropertySpecV4>),
  (link(<t-sen-kernel-PropertySpecV5>)[#"Property​Spec​V5"], "", <t-sen-kernel-PropertySpecV5>),
  (link(<t-sen-kernel-QuantityTypeSpec>)[#"Quantity​Type​Spec"], "spec of a quantity type", <t-sen-kernel-QuantityTypeSpec>),
  (link(<t-sen-kernel-QuantityTypeSpecV4>)[#"Quantity​Type​Spec​V4"], "", <t-sen-kernel-QuantityTypeSpecV4>),
  (link(<t-sen-kernel-QuantityTypeSpecV5>)[#"Quantity​Type​Spec​V5"], "", <t-sen-kernel-QuantityTypeSpecV5>),
  (link(<t-sen-kernel-QueueConfig>)[#"Queue​Config"], "", <t-sen-kernel-QueueConfig>),
  (link(<t-sen-kernel-SenData>)[#"Sen​Data"], "Information about Sen", <t-sen-kernel-SenData>),
  (link(<t-sen-kernel-SequenceTypeSpec>)[#"Sequence​Type​Spec"], "spec of a sequence type", <t-sen-kernel-SequenceTypeSpec>),
  (link(<t-sen-kernel-SequenceTypeSpecV4>)[#"Sequence​Type​Spec​V4"], "", <t-sen-kernel-SequenceTypeSpecV4>),
  (link(<t-sen-kernel-SequenceTypeSpecV5>)[#"Sequence​Type​Spec​V5"], "", <t-sen-kernel-SequenceTypeSpecV5>),
  (link(<t-sen-kernel-SignalData>)[#"Signal​Data"], "Information about an unhandled signal", <t-sen-kernel-SignalData>),
  (link(<t-sen-kernel-StructTypeFieldSpec>)[#"Struct​Type​Field​Spec"], "a field of a struct", <t-sen-kernel-StructTypeFieldSpec>),
  (link(<t-sen-kernel-StructTypeFieldSpecV4>)[#"Struct​Type​Field​Spec​V4"], "", <t-sen-kernel-StructTypeFieldSpecV4>),
  (link(<t-sen-kernel-StructTypeFieldSpecV5>)[#"Struct​Type​Field​Spec​V5"], "", <t-sen-kernel-StructTypeFieldSpecV5>),
  (link(<t-sen-kernel-StructTypeSpec>)[#"Struct​Type​Spec"], "spec of an struct type", <t-sen-kernel-StructTypeSpec>),
  (link(<t-sen-kernel-StructTypeSpecV4>)[#"Struct​Type​Spec​V4"], "", <t-sen-kernel-StructTypeSpecV4>),
  (link(<t-sen-kernel-StructTypeSpecV5>)[#"Struct​Type​Spec​V5"], "", <t-sen-kernel-StructTypeSpecV5>),
  (link(<t-sen-kernel-SystemSleep>)[#"System​Sleep"], "Use the native system sleep", <t-sen-kernel-SystemSleep>),
  (link(<t-sen-kernel-UncaughtException>)[#"Uncaught​Exception"], "Information about an unhandled exception", <t-sen-kernel-UncaughtException>),
  (link(<t-sen-kernel-UnitInfo>)[#"Unit​Info"], "Describes a unit", <t-sen-kernel-UnitInfo>),
  (link(<t-sen-kernel-VariantTypeFieldSpec>)[#"Variant​Type​Field​Spec"], "a field of a variant", <t-sen-kernel-VariantTypeFieldSpec>),
  (link(<t-sen-kernel-VariantTypeFieldSpecV4>)[#"Variant​Type​Field​Spec​V4"], "", <t-sen-kernel-VariantTypeFieldSpecV4>),
  (link(<t-sen-kernel-VariantTypeFieldSpecV5>)[#"Variant​Type​Field​Spec​V5"], "", <t-sen-kernel-VariantTypeFieldSpecV5>),
  (link(<t-sen-kernel-VariantTypeSpec>)[#"Variant​Type​Spec"], "spec of an struct type", <t-sen-kernel-VariantTypeSpec>),
  (link(<t-sen-kernel-VariantTypeSpecV4>)[#"Variant​Type​Spec​V4"], "", <t-sen-kernel-VariantTypeSpecV4>),
  (link(<t-sen-kernel-VariantTypeSpecV5>)[#"Variant​Type​Spec​V5"], "", <t-sen-kernel-VariantTypeSpecV5>),
)

==== #"AliasTypeSpec" #chip("structures", "structure") <t-sen-kernel-AliasTypeSpec>
#prose("data of an aliased type")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("aliased​Type")], link(<t-string>)[#"string"], "qualified type name being aliased",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"].]

==== #"AliasTypeSpecV4" #chip("structures", "structure") <t-sen-kernel-AliasTypeSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("aliased​Type")], link(<t-string>)[#"string"], "qualified type name being aliased",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"].]

==== #"AliasTypeSpecV5" #chip("structures", "structure") <t-sen-kernel-AliasTypeSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("aliased​Type")], link(<t-string>)[#"string"], "qualified type name being aliased",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"].]

==== #"ArgSpec" #chip("structures", "structure") <t-sen-kernel-ArgSpec>
#prose("an argument of a callable (methods and events)")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "argument name",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name of the argument",
)
#facts[Named by #link(<t-sen-kernel-ArgSpecList>)[#"Arg​Spec​List"].]

==== #"ArgSpecV4" #chip("structures", "structure") <t-sen-kernel-ArgSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "argument name",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name of the argument",
)
#facts[Named by #link(<t-sen-kernel-ArgSpecListV4>)[#"Arg​Spec​List​V4"].]

==== #"ArgSpecV5" #chip("structures", "structure") <t-sen-kernel-ArgSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "argument name",
  [#mono("name​Hash")], link(<t-u32>)[#"u32"], "hash computed with the argument name",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name of the argument",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-ArgSpecListV5>)[#"Arg​Spec​List​V5"].]

==== #"BuildInfo" #chip("structures", "structure") <t-sen-kernel-BuildInfo>
#prose("Build-related information")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("maintainer")], link(<t-string>)[#"string"], "principal maintainer of the software",
  [#mono("version")], link(<t-string>)[#"string"], "version string (format-agnostic)",
  [#mono("compiler")], link(<t-string>)[#"string"], "vendor-specific compiler string",
  [#mono("debug​Mode")], link(<t-bool>)[#"bool"], "compiled in debug mode or not",
  [#mono("build​Time")], link(<t-string>)[#"string"], "the commit's timestamp, ISO-8601; not the compiler's clock",
  [#mono("word​Size")], link(<t-sen-kernel-WordSize>)[#"Word​Size"], "architecture",
  [#mono("git​Ref")], link(<t-string>)[#"string"], "git ref spec",
  [#mono("git​Hash")], link(<t-string>)[#"string"], "git hash",
  [#mono("git​Status")], link(<t-sen-kernel-GitStatus>)[#"Git​Status"], "git status",
)
#facts[Named by #link(<t-sen-kernel-ComponentInfo>)[#"Component​Info"], #link(<t-sen-kernel-KernelApi>)[#"Kernel​Api"].]

==== #"BuiltComponentParams" #chip("structures", "structure") <t-sen-kernel-BuiltComponentParams>
#prose("Parameters related to components build by the kernel")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "the name of the component",
  [#mono("period")], link(<t-sen-Duration>)[#"sen.​Duration"], "the execution period",
  [#mono("config")], link(<t-sen-kernel-ComponentConfig>)[#"Component​Config"], "the configuration of the component",
  [#mono("imports")], link(<t-sen-kernel-StringList>)[#"String​List"], "the imported packages",
)
#facts[Named by #link(<t-sen-kernel-BuiltComponentList>)[#"Built​Component​List"].]

==== #"BusAddress" #chip("structures", "structure") <t-sen-kernel-BusAddress>
#prose("Address to connect to a bus")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("session​Name")], link(<t-string>)[#"string"], "",
  [#mono("bus​Name")], link(<t-string>)[#"string"], "",
)

==== #"ClassTypeSpec" #chip("structures", "structure") <t-sen-kernel-ClassTypeSpec>
#prose("spec of a class")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("properties")], link(<t-sen-kernel-PropertySpecList>)[#"Property​Spec​List"], "",
  [#mono("methods")], link(<t-sen-kernel-MethodSpecList>)[#"Method​Spec​List"], "",
  [#mono("events")], link(<t-sen-kernel-EventSpecList>)[#"Event​Spec​List"], "",
  [#mono("constructor")], link(<t-sen-kernel-MethodSpec>)[#"Method​Spec"], "",
  [#mono("parents")], link(<t-sen-kernel-StringList>)[#"String​List"], "",
  [#mono("is​Interface")], link(<t-bool>)[#"bool"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"].]

==== #"ClassTypeSpecV4" #chip("structures", "structure") <t-sen-kernel-ClassTypeSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("properties")], link(<t-sen-kernel-PropertySpecListV4>)[#"Property​Spec​List​V4"], "",
  [#mono("methods")], link(<t-sen-kernel-MethodSpecListV4>)[#"Method​Spec​List​V4"], "",
  [#mono("events")], link(<t-sen-kernel-EventSpecListV4>)[#"Event​Spec​List​V4"], "",
  [#mono("is​Interface")], link(<t-bool>)[#"bool"], "",
  [#mono("id")], link(<t-u32>)[#"u32"], "",
  [#mono("constructor")], link(<t-sen-kernel-MethodSpecV4>)[#"Method​Spec​V4"], "",
  [#mono("parents")], link(<t-sen-kernel-StringList>)[#"String​List"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"].]

==== #"ClassTypeSpecV5" #chip("structures", "structure") <t-sen-kernel-ClassTypeSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("properties")], link(<t-sen-kernel-PropertySpecListV5>)[#"Property​Spec​List​V5"], "",
  [#mono("methods")], link(<t-sen-kernel-MethodSpecListV5>)[#"Method​Spec​List​V5"], "",
  [#mono("events")], link(<t-sen-kernel-EventSpecListV5>)[#"Event​Spec​List​V5"], "",
  [#mono("is​Interface")], link(<t-bool>)[#"bool"], "",
  [#mono("id")], link(<t-u32>)[#"u32"], "",
  [#mono("constructor")], link(<t-sen-kernel-MethodSpecV5>)[#"Method​Spec​V5"], "",
  [#mono("parents")], link(<t-sen-kernel-StringList>)[#"String​List"], "",
  [#mono("hash")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"].]

==== #"ComponentConfig" #chip("structures", "structure") <t-sen-kernel-ComponentConfig>
#prose("Basic runtime configuration for a component")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("priority")], link(<t-sen-kernel-Priority>)[#"Priority"], "thread priority",
  [#mono("stack​Size")], link(<t-u32>)[#"u32"], "thread stack size in bytes, 0 means default",
  [#mono("group")], link(<t-u32>)[#"u32"], "the group where to run the component",
  [#mono("cpu​Affinity")], link(<t-u64>)[#"u64"], "CPU affinity mask",
  [#mono("in​Queue")], link(<t-sen-kernel-QueueConfig>)[#"Queue​Config"], "queuing of inbound information",
  [#mono("out​Queue")], link(<t-sen-kernel-QueueConfig>)[#"Queue​Config"], "queuing of outbound information",
  [#mono("sleep​Policy")], link(<t-sen-kernel-SleepPolicy>)[#"Sleep​Policy"], "configurable component sleep policy",
)
#facts[Named by #link(<t-sen-kernel-BuiltComponentParams>)[#"Built​Component​Params"], #link(<t-sen-kernel-LoadedComponentParams>)[#"Loaded​Component​Params"].]

==== #"ComponentInfo" #chip("structures", "structure") <t-sen-kernel-ComponentInfo>
#prose("Basic information of a kernel component")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "",
  [#mono("description")], link(<t-string>)[#"string"], "",
  [#mono("build​Info")], link(<t-sen-kernel-BuildInfo>)[#"Build​Info"], "",
)

==== #"CustomTypeSpec" #chip("structures", "structure") <t-sen-kernel-CustomTypeSpec>
#prose("spec of a custom type")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("qualified​Name")], link(<t-string>)[#"string"], "package-qualified name",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("data")], link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"], "spec data",
)
#facts[Named by #link(<t-sen-components-jsonrpc-TypeLookupResult>)[#"sen.​components.​jsonrpc.​Type​Lookup​Result"], #link(<t-sen-kernel-CustomTypeSpecList>)[#"Custom​Type​Spec​List"].]

==== #"CustomTypeSpecV4" #chip("structures", "structure") <t-sen-kernel-CustomTypeSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("qualified​Name")], link(<t-string>)[#"string"], "package-qualified name",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("data")], link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"], "spec data",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeSpecListV4>)[#"Custom​Type​Spec​List​V4"].]

==== #"CustomTypeSpecV5" #chip("structures", "structure") <t-sen-kernel-CustomTypeSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("qualified​Name")], link(<t-string>)[#"string"], "package-qualified name",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("data")], link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"], "spec data",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeSpecListV5>)[#"Custom​Type​Spec​List​V5"].]

==== #"EnumTypeSpec" #chip("structures", "structure") <t-sen-kernel-EnumTypeSpec>
#prose("spec of an enum type")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("enums")], link(<t-sen-kernel-EnumeratorSpecList>)[#"Enumerator​Spec​List"], "enumerators",
  [#mono("storage​Type")], link(<t-sen-kernel-IntegralType>)[#"Integral​Type"], "type used to store the value",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"].]

==== #"EnumTypeSpecV4" #chip("structures", "structure") <t-sen-kernel-EnumTypeSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("enums")], link(<t-sen-kernel-EnumeratorSpecListV4>)[#"Enumerator​Spec​List​V4"], "enumerators",
  [#mono("storage​Type")], link(<t-sen-kernel-IntegralType>)[#"Integral​Type"], "type used to store the value",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"].]

==== #"EnumTypeSpecV5" #chip("structures", "structure") <t-sen-kernel-EnumTypeSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("enums")], link(<t-sen-kernel-EnumeratorSpecListV5>)[#"Enumerator​Spec​List​V5"], "enumerators",
  [#mono("storage​Type")], link(<t-sen-kernel-IntegralType>)[#"Integral​Type"], "type used to store the value",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"].]

==== #"EnumeratorSpec" #chip("structures", "structure") <t-sen-kernel-EnumeratorSpec>
#prose("an enumerator")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "enumerator name",
  [#mono("key")], link(<t-u32>)[#"u32"], "enumerator value",
  [#mono("description")], link(<t-string>)[#"string"], "enumerator description",
)
#facts[Named by #link(<t-sen-kernel-EnumeratorSpecList>)[#"Enumerator​Spec​List"].]

==== #"EnumeratorSpecV4" #chip("structures", "structure") <t-sen-kernel-EnumeratorSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "enumerator name",
  [#mono("key")], link(<t-u64>)[#"u64"], "enumerator value",
)
#facts[Named by #link(<t-sen-kernel-EnumeratorSpecListV4>)[#"Enumerator​Spec​List​V4"].]

==== #"EnumeratorSpecV5" #chip("structures", "structure") <t-sen-kernel-EnumeratorSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "enumerator name",
  [#mono("key")], link(<t-u64>)[#"u64"], "enumerator value",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-EnumeratorSpecListV5>)[#"Enumerator​Spec​List​V5"].]

==== #"EnvVar" #chip("structures", "structure") <t-sen-kernel-EnvVar>
#prose("Information about an environment variable")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the variable",
  [#mono("value")], link(<t-string>)[#"string"], "value of the variable",
)
#facts[Named by #link(<t-sen-kernel-EnvironmentVarList>)[#"Environment​Var​List"].]

==== #"ErrorData" #chip("structures", "structure") <t-sen-kernel-ErrorData>
#prose("Information related to the error")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("time")], link(<t-sen-TimeStamp>)[#"sen.​Time​Stamp"], "time of the error",
  [#mono("error​Message")], link(<t-sen-kernel-StringList>)[#"String​List"], "message presented to the user",
  [#mono("exception​Data")], link(<t-sen-kernel-UncaughtException>)[#"Uncaught​Exception"], "information about the unhandled exception (if any)",
  [#mono("signal​Data")], link(<t-sen-kernel-SignalData>)[#"Signal​Data"], "information about the received signal (if any)",
)
#facts[Named by #link(<t-sen-kernel-ErrorReport>)[#"Error​Report"].]

==== #"ErrorReport" #chip("structures", "structure") <t-sen-kernel-ErrorReport>
#prose("Information about a detected error")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("sen​Data")], link(<t-sen-kernel-SenData>)[#"Sen​Data"], "information related to Sen",
  [#mono("process​Data")], link(<t-sen-kernel-ProcessData>)[#"Process​Data"], "information related to the process",
  [#mono("error​Data")], link(<t-sen-kernel-ErrorData>)[#"Error​Data"], "information related to the error",
)

==== #"EventSpec" #chip("structures", "structure") <t-sen-kernel-EventSpec>
#prose("spec for an event")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("args")], link(<t-sen-kernel-ArgSpecList>)[#"Arg​Spec​List"], "arguments",
  [#mono("transport​Mode")], link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "transport mode",
)
#facts[Named by #link(<t-sen-kernel-EventSpecList>)[#"Event​Spec​List"].]

==== #"EventSpecV4" #chip("structures", "structure") <t-sen-kernel-EventSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("member​Hash")], link(<t-u32>)[#"u32"], "hash of the class member",
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("args")], link(<t-sen-kernel-ArgSpecListV4>)[#"Arg​Spec​List​V4"], "arguments",
  [#mono("transport​Mode")], link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "transport mode",
)
#facts[Named by #link(<t-sen-kernel-EventSpecListV4>)[#"Event​Spec​List​V4"].]

==== #"EventSpecV5" #chip("structures", "structure") <t-sen-kernel-EventSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("member​Hash")], link(<t-u32>)[#"u32"], "hash of the class member",
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("args")], link(<t-sen-kernel-ArgSpecListV5>)[#"Arg​Spec​List​V5"], "arguments",
  [#mono("transport​Mode")], link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "transport mode",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-EventSpecListV5>)[#"Event​Spec​List​V5"].]

==== #"ExecError" #chip("structures", "structure") <t-sen-kernel-ExecError>
#prose("Holds details of an execution error")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("category")], link(<t-sen-kernel-ErrorCategory>)[#"Error​Category"], "general problem",
  [#mono("explanation")], link(<t-string>)[#"string"], "human-readable explanation",
)

==== #"KernelParams" #chip("structures", "structure") <t-sen-kernel-KernelParams>
#prose("Parameters to configure kernel execution")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("run​Mode")], link(<t-sen-kernel-RunMode>)[#"Run​Mode"], "how to run the kernel",
  [#mono("app​Name")], link(<t-string>)[#"string"], "optional name of the application",
  [#mono("bus")], link(<t-string>)[#"string"], "where to publish the kernel objects (defaults to local.kernel)",
  [#mono("clock​Bus")], link(<t-string>)[#"string"], "where to publish the virtual clock (if any), defaults to bus",
  [#mono("clock​Name")], link(<t-string>)[#"string"], "the name of the virtual clock (if any), defaults to 'clock'",
  [#mono("clock​Master")], link(<t-bool>)[#"bool"], "if time is virtualized, publish a master clock to clockBus",
  [#mono("log​Config")], link(<t-sen-kernel-log-Config>)[#"log.​Config"], "logging configuration",
  [#mono("crash​Report​Dir")], link(<t-string>)[#"string"], "where to store crash reports (defaults to the temp dir)",
  [#mono("crash​Report​Disabled")], link(<t-bool>)[#"bool"], "if true, no reports are generated",
  [#mono("lock​Memory​Pages")], link(<t-bool>)[#"bool"], "keep process pages memory-resident",
  [#mono("sleep​Policy")], link(<t-sen-kernel-SleepPolicy>)[#"Sleep​Policy"], "configurable sleep policy of the kernel component",
  [#mono("compatibility​Mode")], link(<t-sen-kernel-CompatibilityMode>)[#"Compatibility​Mode"], "what to do with a remote type that does not match",
  [#mono("limit​Cpu​Idle​Latency")], link(<t-bool>)[#"bool"], "This struct is reachable over the wire through Kernel.getConfig() and is embedded in SenData, and the encoding is positional, so adding a field here shifts what follows it either way. keep the CPUs out of the deep idle states they are slow to leave",
)
#facts[Named by #link(<t-sen-kernel-KernelApi>)[#"Kernel​Api"], #link(<t-sen-kernel-SenData>)[#"Sen​Data"].]

==== #"LoadedComponentParams" #chip("structures", "structure") <t-sen-kernel-LoadedComponentParams>
#prose("Parameters related to a component that will be loaded by the kernel")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "the name of the component",
  [#mono("config")], link(<t-sen-kernel-ComponentConfig>)[#"Component​Config"], "the configuration of the component",
)
#facts[Named by #link(<t-sen-kernel-LoadedComponentList>)[#"Loaded​Component​List"].]

==== #"MethodSpec" #chip("structures", "structure") <t-sen-kernel-MethodSpec>
#prose("spec for methods")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("args")], link(<t-sen-kernel-ArgSpecList>)[#"Arg​Spec​List"], "arguments",
  [#mono("transport​Mode")], link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "transport mode",
  [#mono("constness")], link(<t-sen-kernel-MethodConstnessSpec>)[#"Method​Constness​Spec"], "if it changes the state of the object",
  [#mono("deferred")], link(<t-bool>)[#"bool"], "if the execution of the method is deferred",
  [#mono("return​Type")], link(<t-string>)[#"string"], "qualified return type name",
  [#mono("property​Relation")], link(<t-sen-kernel-PropertyRelationSpec>)[#"Property​Relation​Spec"], "property relation of the method",
  [#mono("local​Only")], link(<t-bool>)[#"bool"], "true if the method can only be called locally",
)
#facts[Named by #link(<t-sen-kernel-ClassTypeSpec>)[#"Class​Type​Spec"], #link(<t-sen-kernel-MethodSpecList>)[#"Method​Spec​List"].]

==== #"MethodSpecV4" #chip("structures", "structure") <t-sen-kernel-MethodSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("member​Hash")], link(<t-u32>)[#"u32"], "hash of the class member",
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("args")], link(<t-sen-kernel-ArgSpecListV4>)[#"Arg​Spec​List​V4"], "arguments",
  [#mono("transport​Mode")], link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "transport mode",
  [#mono("constness")], link(<t-sen-kernel-MethodConstnessSpec>)[#"Method​Constness​Spec"], "if it changes the state of the object",
  [#mono("deferred")], link(<t-bool>)[#"bool"], "true if the method is marked as deferred",
  [#mono("return​Type​Id")], link(<t-string>)[#"string"], "qualified return type name (empty for none)",
)
#facts[Named by #link(<t-sen-kernel-ClassTypeSpecV4>)[#"Class​Type​Spec​V4"], #link(<t-sen-kernel-MethodSpecListV4>)[#"Method​Spec​List​V4"].]

==== #"MethodSpecV5" #chip("structures", "structure") <t-sen-kernel-MethodSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("member​Hash")], link(<t-u32>)[#"u32"], "hash of the class member",
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("args")], link(<t-sen-kernel-ArgSpecListV5>)[#"Arg​Spec​List​V5"], "arguments",
  [#mono("transport​Mode")], link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "transport mode",
  [#mono("constness")], link(<t-sen-kernel-MethodConstnessSpec>)[#"Method​Constness​Spec"], "if it changes the state of the object",
  [#mono("local​Only")], link(<t-bool>)[#"bool"], "true if the method can only be called locally",
  [#mono("return​Type​Id")], link(<t-string>)[#"string"], "qualified return type name (empty for none)",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-ClassTypeSpecV5>)[#"Class​Type​Spec​V5"], #link(<t-sen-kernel-MethodSpecListV5>)[#"Method​Spec​List​V5"].]

==== #"NetworkFootprint" #chip("structures", "structure") <t-sen-kernel-NetworkFootprint>
#prose("Network endpoints predicted offline or observed at runtime for one process")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("discovery​Port")], link(<t-u16>)[#"u16"], "Discovery port used to calculate multicast addresses",
  [#mono("multicast")], link(<t-sen-kernel-NetworkFootprintMulticast>)[#"Network​Footprint​Multicast"], "Multicast allocation details",
  [#mono("ports")], link(<t-sen-kernel-NetworkFootprintPortList>)[#"Network​Footprint​Port​List"], "Port settings",
  [#mono("port​Exclusions")], link(<t-sen-kernel-NetworkFootprintPortExclusions>)[#"Network​Footprint​Port​Exclusions"], "Port ranges that cannot be used",
)

==== #"NetworkFootprintAddressRange" #chip("structures", "structure") <t-sen-kernel-NetworkFootprintAddressRange>
#prose("Inclusive IPv4 address range excluded from multicast allocation")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("min")], link(<t-string>)[#"string"], "",
  [#mono("max")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintAddressRangeList>)[#"Network​Footprint​Address​Range​List"].]

==== #"NetworkFootprintBus" #chip("structures", "structure") <t-sen-kernel-NetworkFootprintBus>
#prose("Multicast endpoint allocated to a bus")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("session​Name")], link(<t-string>)[#"string"], "",
  [#mono("bus​Name")], link(<t-string>)[#"string"], "",
  [#mono("session​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("bus​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("group​Address")], link(<t-string>)[#"string"], "",
  [#mono("source")], link(<t-sen-kernel-NetworkFootprintBusSource>)[#"Network​Footprint​Bus​Source"], "",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintBusList>)[#"Network​Footprint​Bus​List"].]

==== #"NetworkFootprintBusIdentity" #chip("structures", "structure") <t-sen-kernel-NetworkFootprintBusIdentity>
#prose("Names and IDs that identify a bus")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("session​Name")], link(<t-string>)[#"string"], "",
  [#mono("bus​Name")], link(<t-string>)[#"string"], "",
  [#mono("session​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("bus​Id")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintSelfCollision>)[#"Network​Footprint​Self​Collision"].]

==== #"NetworkFootprintCollision" #chip("structures", "structure") <t-sen-kernel-NetworkFootprintCollision>
#prose("Multicast allocation capacity and collision information")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("usable​Space​Size")], link(<t-u64>)[#"u64"], "Number of addresses available",
  [#mono("bus​Count")], link(<t-u64>)[#"u64"], "Number of reported buses",
  [#mono("probability")], link(<t-f64>)[#"f64"], "Estimated conflict from 0 to 1",
  [#mono("self​Collisions")], link(<t-sen-kernel-NetworkFootprintSelfCollisionList>)[#"Network​Footprint​Self​Collision​List"], "Collisions in the reported buses",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintMulticast>)[#"Network​Footprint​Multicast"].]

==== #"NetworkFootprintMulticast" #chip("structures", "structure") <t-sen-kernel-NetworkFootprintMulticast>
#prose("Multicast information in the network footprint")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("port")], link(<t-u16>)[#"u16"], "Shared multicast port used by every bus",
  [#mono("buses")], link(<t-sen-kernel-NetworkFootprintBusList>)[#"Network​Footprint​Bus​List"], "Multicast address used by each bus",
  [#mono("exclusions")], link(<t-sen-kernel-NetworkFootprintAddressRangeList>)[#"Network​Footprint​Address​Range​List"], "Addresses that cannot be used",
  [#mono("collision")], link(<t-sen-kernel-NetworkFootprintCollision>)[#"Network​Footprint​Collision"], "Available addresses and conflict information",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprint>)[#"Network​Footprint"].]

==== #"NetworkFootprintPort" #chip("structures", "structure") <t-sen-kernel-NetworkFootprintPort>
#prose("Socket type, port selection mode and port value")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("kind")], link(<t-sen-kernel-NetworkFootprintPortKind>)[#"Network​Footprint​Port​Kind"], "",
  [#mono("mode")], link(<t-sen-kernel-NetworkFootprintPortMode>)[#"Network​Footprint​Port​Mode"], "",
  [#mono("value")], link(<t-sen-kernel-NetworkFootprintPortValue>)[#"Network​Footprint​Port​Value"], "",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintPortList>)[#"Network​Footprint​Port​List"].]

==== #"NetworkFootprintPortExclusions" #chip("structures", "structure") <t-sen-kernel-NetworkFootprintPortExclusions>
#prose("Port ranges that cannot be used")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("built​In")], link(<t-sen-kernel-NetworkFootprintPortRangeList>)[#"Network​Footprint​Port​Range​List"], "",
  [#mono("configured")], link(<t-sen-kernel-NetworkFootprintPortRangeList>)[#"Network​Footprint​Port​Range​List"], "",
  [#mono("operating​System")], link(<t-sen-kernel-NetworkFootprintPortRangeList>)[#"Network​Footprint​Port​Range​List"], "",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprint>)[#"Network​Footprint"].]

==== #"NetworkFootprintPortRange" #chip("structures", "structure") <t-sen-kernel-NetworkFootprintPortRange>
#prose("Inclusive range from which a port may be selected")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("min")], link(<t-u16>)[#"u16"], "",
  [#mono("max")], link(<t-u16>)[#"u16"], "",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintPortRangeList>)[#"Network​Footprint​Port​Range​List"], #link(<t-sen-kernel-NetworkFootprintPortValue>)[#"Network​Footprint​Port​Value"].]

==== #"NetworkFootprintSelfCollision" #chip("structures", "structure") <t-sen-kernel-NetworkFootprintSelfCollision>
#prose("Two buses allocated to the same multicast group")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("first​Bus")], link(<t-sen-kernel-NetworkFootprintBusIdentity>)[#"Network​Footprint​Bus​Identity"], "",
  [#mono("second​Bus")], link(<t-sen-kernel-NetworkFootprintBusIdentity>)[#"Network​Footprint​Bus​Identity"], "",
  [#mono("group​Address")], link(<t-string>)[#"string"], "",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintSelfCollisionList>)[#"Network​Footprint​Self​Collision​List"].]

==== #"OpFinished" #chip("structures", "structure") <t-sen-kernel-OpFinished>
#prose("An operation is complete")
#facts[Named by #link(<t-sen-kernel-OpState>)[#"Op​State"].]

==== #"OpNotFinished" #chip("structures", "structure") <t-sen-kernel-OpNotFinished>
#prose("An operation is not complete")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("wait​Hint")], link(<t-sen-Duration>)[#"sen.​Duration"], "How long to wait until the next try",
)
#facts[Named by #link(<t-sen-kernel-OpState>)[#"Op​State"].]

==== #"OptionalTypeSpec" #chip("structures", "structure") <t-sen-kernel-OptionalTypeSpec>
#prose("data of an optional type")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"].]

==== #"OptionalTypeSpecV4" #chip("structures", "structure") <t-sen-kernel-OptionalTypeSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"].]

==== #"OptionalTypeSpecV5" #chip("structures", "structure") <t-sen-kernel-OptionalTypeSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"].]

==== #"PrecisionSleep" #chip("structures", "structure") <t-sen-kernel-PrecisionSleep>
#prose("Be more precise at the expense of some CPU cycles.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("very​Coarse​Grain​Sleep​Time")], link(<t-sen-Duration>)[#"sen.​Duration"], "First set of sleeps. If 0 defaults to 7ms",
  [#mono("coarse​Grain​Sleep​Time")], link(<t-sen-Duration>)[#"sen.​Duration"], "Second set of sleeps. If 0 defaults to 1ms",
)
#facts[Named by #link(<t-sen-kernel-SleepPolicy>)[#"Sleep​Policy"].]

==== #"ProcessData" #chip("structures", "structure") <t-sen-kernel-ProcessData>
#prose("Information about the process")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("process​Info")], link(<t-sen-kernel-ProcessInfo>)[#"Process​Info"], "general process information",
  [#mono("environment")], link(<t-sen-kernel-EnvironmentVarList>)[#"Environment​Var​List"], "list of environment variables",
  [#mono("stacktrace")], link(<t-sen-kernel-StringList>)[#"String​List"], "the stack trace (if any)",
  [#mono("recent​Logs")], link(<t-sen-kernel-StringList>)[#"String​List"], "the last logs emitted by the process",
)
#facts[Named by #link(<t-sen-kernel-ErrorReport>)[#"Error​Report"].]

==== #"ProcessInfo" #chip("structures", "structure") <t-sen-kernel-ProcessInfo>
#prose("Basic information about a process")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("host​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("process​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("session​Id")], link(<t-u32>)[#"u32"], "",
  [#mono("session​Name")], link(<t-string>)[#"string"], "",
  [#mono("app​Name")], link(<t-string>)[#"string"], "",
  [#mono("host​Name")], link(<t-string>)[#"string"], "",
  [#mono("os​Kind")], link(<t-sen-kernel-OsKind>)[#"Os​Kind"], "",
  [#mono("os​Name")], link(<t-string>)[#"string"], "",
  [#mono("cpu​Arch")], link(<t-sen-kernel-CpuArch>)[#"Cpu​Arch"], "",
)
#facts[Named by #link(<t-sen-components-ether-Hello>)[#"sen.​components.​ether.​Hello"], #link(<t-sen-components-ether-SessionPresenceBeam>)[#"sen.​components.​ether.​Session​Presence​Beam"], #link(<t-sen-kernel-ProcessData>)[#"Process​Data"], #link(<t-sen-kernel-Processes>)[#"Processes"].]

==== #"PropertySpec" #chip("structures", "structure") <t-sen-kernel-PropertySpec>
#prose("spec of a property")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("category")], link(<t-sen-kernel-PropertyCategorySpec>)[#"Property​Category​Spec"], "category",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name of the property",
  [#mono("transport​Mode")], link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "how to transport the data",
  [#mono("tags")], link(<t-sen-kernel-StringList>)[#"String​List"], "user-defined tags",
  [#mono("checked​Set")], link(<t-bool>)[#"bool"], "if setting the property is checked",
)
#facts[Named by #link(<t-sen-kernel-PropertySpecList>)[#"Property​Spec​List"].]

==== #"PropertySpecV4" #chip("structures", "structure") <t-sen-kernel-PropertySpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("member​Hash")], link(<t-u32>)[#"u32"], "hash of the class member",
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("category")], link(<t-sen-kernel-PropertyCategorySpec>)[#"Property​Category​Spec"], "category",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name of the property",
  [#mono("transport​Mode")], link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "how to transport the data",
  [#mono("tags")], link(<t-sen-kernel-StringList>)[#"String​List"], "user-defined tags",
)
#facts[Named by #link(<t-sen-kernel-PropertySpecListV4>)[#"Property​Spec​List​V4"].]

==== #"PropertySpecV5" #chip("structures", "structure") <t-sen-kernel-PropertySpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("member​Hash")], link(<t-u32>)[#"u32"], "hash of the class member",
  [#mono("name")], link(<t-string>)[#"string"], "name of the type",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("category")], link(<t-sen-kernel-PropertyCategorySpec>)[#"Property​Category​Spec"], "category",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name of the property",
  [#mono("transport​Mode")], link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "how to transport the data",
  [#mono("tags")], link(<t-sen-kernel-StringList>)[#"String​List"], "user-defined tags",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-PropertySpecListV5>)[#"Property​Spec​List​V5"].]

==== #"QuantityTypeSpec" #chip("structures", "structure") <t-sen-kernel-QuantityTypeSpec>
#prose("spec of a quantity type")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("element​Type")], link(<t-sen-kernel-NumericType>)[#"Numeric​Type"], "type used to store the value",
  [#mono("unit")], link(<t-sen-kernel-UnitInfo>)[#"Unit​Info"], "used unit",
  [#mono("min​Value")], link(<t-f64>)[#"f64"], "discarded if >= maxValue",
  [#mono("max​Value")], link(<t-f64>)[#"f64"], "discarded if <= minValue",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"].]

==== #"QuantityTypeSpecV4" #chip("structures", "structure") <t-sen-kernel-QuantityTypeSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("numeric​Type")], link(<t-sen-kernel-NumericType>)[#"Numeric​Type"], "type used to store the value",
  [#mono("unit")], link(<t-sen-kernel-UnitInfo>)[#"Unit​Info"], "used unit",
  [#mono("min​Value")], link(<t-f64>)[#"f64"], "discarded if >= maxValue",
  [#mono("max​Value")], link(<t-f64>)[#"f64"], "discarded if <= minValue",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"].]

==== #"QuantityTypeSpecV5" #chip("structures", "structure") <t-sen-kernel-QuantityTypeSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("numeric​Type")], link(<t-sen-kernel-NumericType>)[#"Numeric​Type"], "type used to store the value",
  [#mono("unit")], link(<t-sen-kernel-UnitInfo>)[#"Unit​Info"], "used unit",
  [#mono("min​Value")], link(<t-f64>)[#"f64"], "discarded if >= maxValue",
  [#mono("max​Value")], link(<t-f64>)[#"f64"], "discarded if <= minValue",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"].]

==== #"QueueConfig" #chip("structures", "structure") <t-sen-kernel-QueueConfig>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("eviction​Policy")], link(<t-sen-kernel-QueueEvictionPolicy>)[#"Queue​Eviction​Policy"], "what to do when the queue is full",
  [#mono("max​Size")], link(<t-u64>)[#"u64"], "0 means unbounded",
)
#facts[Named by #link(<t-sen-kernel-ComponentConfig>)[#"Component​Config"].]

==== #"SenData" #chip("structures", "structure") <t-sen-kernel-SenData>
#prose("Information about Sen")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("version")], link(<t-string>)[#"string"], "the Sen version",
  [#mono("compiler")], link(<t-string>)[#"string"], "the compiler used to build Sen",
  [#mono("debug​Mode")], link(<t-bool>)[#"bool"], "true if it was build in debug mode",
  [#mono("build​Time")], link(<t-string>)[#"string"], "the commit's timestamp, not the compiler's clock",
  [#mono("word​Size")], link(<t-sen-kernel-WordSize>)[#"Word​Size"], "architecture",
  [#mono("git​Ref")], link(<t-string>)[#"string"], "git ref",
  [#mono("git​Hash")], link(<t-string>)[#"string"], "git hash",
  [#mono("git​Status")], link(<t-sen-kernel-GitStatus>)[#"Git​Status"], "status of the branch when build",
  [#mono("kernel​Params")], link(<t-sen-kernel-KernelParams>)[#"Kernel​Params"], "Sen kernel parameters",
  [#mono("kernel​Protocol")], link(<t-u32>)[#"u32"], "version of the kernel protocol",
  [#mono("transport​Protocol")], link(<t-u32>)[#"u32"], "version of the transport protocol (if present)",
  [#mono("loaded​Components")], link(<t-sen-kernel-LoadedComponentList>)[#"Loaded​Component​List"], "components to load",
  [#mono("built​Components")], link(<t-sen-kernel-BuiltComponentList>)[#"Built​Component​List"], "components to build",
)
#facts[Named by #link(<t-sen-kernel-ErrorReport>)[#"Error​Report"].]

==== #"SequenceTypeSpec" #chip("structures", "structure") <t-sen-kernel-SequenceTypeSpec>
#prose("spec of a sequence type")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("element​Type")], link(<t-string>)[#"string"], "qualified type name for stored values",
  [#mono("max​Size")], link(<t-u64>)[#"u64"], "discarded when 0",
  [#mono("fixed​Size")], link(<t-bool>)[#"bool"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"].]

==== #"SequenceTypeSpecV4" #chip("structures", "structure") <t-sen-kernel-SequenceTypeSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("element​Type")], link(<t-string>)[#"string"], "qualified type name for stored values",
  [#mono("max​Size")], link(<t-u64>)[#"u64"], "discarded when 0",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"].]

==== #"SequenceTypeSpecV5" #chip("structures", "structure") <t-sen-kernel-SequenceTypeSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("element​Type")], link(<t-string>)[#"string"], "qualified type name for stored values",
  [#mono("max​Size")], link(<t-u64>)[#"u64"], "discarded when 0",
  [#mono("fixed​Size")], link(<t-bool>)[#"bool"], "",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"].]

==== #"SignalData" #chip("structures", "structure") <t-sen-kernel-SignalData>
#prose("Information about an unhandled signal")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("signal​Number")], link(<t-i32>)[#"i32"], "signal number",
  [#mono("signal​Name")], link(<t-string>)[#"string"], "human-readable name",
)
#facts[Named by #link(<t-sen-kernel-ErrorData>)[#"Error​Data"].]

==== #"StructTypeFieldSpec" #chip("structures", "structure") <t-sen-kernel-StructTypeFieldSpec>
#prose("a field of a struct")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "field name",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name held by this field",
)
#facts[Named by #link(<t-sen-kernel-StructTypeFieldSpecList>)[#"Struct​Type​Field​Spec​List"].]

==== #"StructTypeFieldSpecV4" #chip("structures", "structure") <t-sen-kernel-StructTypeFieldSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "field name",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name held by this field",
)
#facts[Named by #link(<t-sen-kernel-StructTypeFieldSpecListV4>)[#"Struct​Type​Field​Spec​List​V4"].]

==== #"StructTypeFieldSpecV5" #chip("structures", "structure") <t-sen-kernel-StructTypeFieldSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "field name",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name held by this field",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-StructTypeFieldSpecListV5>)[#"Struct​Type​Field​Spec​List​V5"].]

==== #"StructTypeSpec" #chip("structures", "structure") <t-sen-kernel-StructTypeSpec>
#prose("spec of an struct type")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("fields")], link(<t-sen-kernel-StructTypeFieldSpecList>)[#"Struct​Type​Field​Spec​List"], "fields",
  [#mono("parent")], link(<t-string>)[#"string"], "qualified type name the parent struct, empty means none",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"].]

==== #"StructTypeSpecV4" #chip("structures", "structure") <t-sen-kernel-StructTypeSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("fields")], link(<t-sen-kernel-StructTypeFieldSpecListV4>)[#"Struct​Type​Field​Spec​List​V4"], "fields",
  [#mono("parent")], link(<t-string>)[#"string"], "qualified type name the parent struct, empty means none",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"].]

==== #"StructTypeSpecV5" #chip("structures", "structure") <t-sen-kernel-StructTypeSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("fields")], link(<t-sen-kernel-StructTypeFieldSpecListV5>)[#"Struct​Type​Field​Spec​List​V5"], "fields",
  [#mono("parent")], link(<t-string>)[#"string"], "qualified type name the parent struct, empty means none",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"].]

==== #"SystemSleep" #chip("structures", "structure") <t-sen-kernel-SystemSleep>
#prose("Use the native system sleep")
#facts[Named by #link(<t-sen-kernel-SleepPolicy>)[#"Sleep​Policy"].]

==== #"UncaughtException" #chip("structures", "structure") <t-sen-kernel-UncaughtException>
#prose("Information about an unhandled exception")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("exception​Kind")], link(<t-sen-kernel-ExceptionKind>)[#"Exception​Kind"], "kind of exception",
  [#mono("message")], link(<t-string>)[#"string"], "message it provides (if any)",
)
#facts[Named by #link(<t-sen-kernel-ErrorData>)[#"Error​Data"].]

==== #"UnitInfo" #chip("structures", "structure") <t-sen-kernel-UnitInfo>
#prose("Describes a unit")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "",
  [#mono("abbreviation")], link(<t-string>)[#"string"], "",
  [#mono("category")], link(<t-sen-kernel-UnitCat>)[#"Unit​Cat"], "",
)
#facts[Named by #link(<t-sen-kernel-QuantityTypeSpec>)[#"Quantity​Type​Spec"], #link(<t-sen-kernel-QuantityTypeSpecV4>)[#"Quantity​Type​Spec​V4"], #link(<t-sen-kernel-QuantityTypeSpecV5>)[#"Quantity​Type​Spec​V5"], #link(<t-sen-kernel-UnitList>)[#"Unit​List"].]

==== #"VariantTypeFieldSpec" #chip("structures", "structure") <t-sen-kernel-VariantTypeFieldSpec>
#prose("a field of a variant")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("key")], link(<t-u32>)[#"u32"], "key/index of this type within the variant",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name held by this field",
)
#facts[Named by #link(<t-sen-kernel-VariantTypeFieldSpecList>)[#"Variant​Type​Field​Spec​List"].]

==== #"VariantTypeFieldSpecV4" #chip("structures", "structure") <t-sen-kernel-VariantTypeFieldSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("key")], link(<t-u32>)[#"u32"], "key/index of this type within the variant",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name held by this field",
)
#facts[Named by #link(<t-sen-kernel-VariantTypeFieldSpecListV4>)[#"Variant​Type​Field​Spec​List​V4"].]

==== #"VariantTypeFieldSpecV5" #chip("structures", "structure") <t-sen-kernel-VariantTypeFieldSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("key")], link(<t-u32>)[#"u32"], "key/index of this type within the variant",
  [#mono("description")], link(<t-string>)[#"string"], "documentation",
  [#mono("type")], link(<t-string>)[#"string"], "qualified type name held by this field",
  [#mono("hash")], link(<t-u32>)[#"u32"], "unique hash",
)
#facts[Named by #link(<t-sen-kernel-VariantTypeFieldSpecListV5>)[#"Variant​Type​Field​Spec​List​V5"].]

==== #"VariantTypeSpec" #chip("structures", "structure") <t-sen-kernel-VariantTypeSpec>
#prose("spec of an struct type")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("fields")], link(<t-sen-kernel-VariantTypeFieldSpecList>)[#"Variant​Type​Field​Spec​List"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"].]

==== #"VariantTypeSpecV4" #chip("structures", "structure") <t-sen-kernel-VariantTypeSpecV4>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("fields")], link(<t-sen-kernel-VariantTypeFieldSpecListV4>)[#"Variant​Type​Field​Spec​List​V4"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"].]

==== #"VariantTypeSpecV5" #chip("structures", "structure") <t-sen-kernel-VariantTypeSpecV5>
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("fields")], link(<t-sen-kernel-VariantTypeFieldSpecListV5>)[#"Variant​Type​Field​Spec​List​V5"], "",
  [#mono("hash")], link(<t-u32>)[#"u32"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"].]

#section("Enumerations", "enumerations")

#summary(
  (link(<t-sen-kernel-BasicType>)[#"Basic​Type"], "built-in others", <t-sen-kernel-BasicType>),
  (link(<t-sen-kernel-CompatibilityMode>)[#"Compatibility​Mode"], "What to do when a type published by another kernel does not match the local one", <t-sen-kernel-CompatibilityMode>),
  (link(<t-sen-kernel-ComponentState>)[#"Component​State"], "State of a kernel component", <t-sen-kernel-ComponentState>),
  (link(<t-sen-kernel-CpuArch>)[#"Cpu​Arch"], "Type of CPU", <t-sen-kernel-CpuArch>),
  (link(<t-sen-kernel-ErrorCategory>)[#"Error​Category"], "Category of a problem during component execution", <t-sen-kernel-ErrorCategory>),
  (link(<t-sen-kernel-ExceptionKind>)[#"Exception​Kind"], "Types of exceptions that can occur.", <t-sen-kernel-ExceptionKind>),
  (link(<t-sen-kernel-GitStatus>)[#"Git​Status"], "Status of git", <t-sen-kernel-GitStatus>),
  (link(<t-sen-kernel-IntegralType>)[#"Integral​Type"], "built-in integrals", <t-sen-kernel-IntegralType>),
  (link(<t-sen-kernel-MethodConstnessSpec>)[#"Method​Constness​Spec"], "method constness", <t-sen-kernel-MethodConstnessSpec>),
  (link(<t-sen-kernel-NetworkFootprintBusSource>)[#"Network​Footprint​Bus​Source"], "Bus source", <t-sen-kernel-NetworkFootprintBusSource>),
  (link(<t-sen-kernel-NetworkFootprintPortKind>)[#"Network​Footprint​Port​Kind"], "Type of socket opened by the process", <t-sen-kernel-NetworkFootprintPortKind>),
  (link(<t-sen-kernel-NetworkFootprintPortMode>)[#"Network​Footprint​Port​Mode"], "Port allocation policy", <t-sen-kernel-NetworkFootprintPortMode>),
  (link(<t-sen-kernel-OsKind>)[#"Os​Kind"], "Type of operating system", <t-sen-kernel-OsKind>),
  (link(<t-sen-kernel-Priority>)[#"Priority"], "Thread execution priorities", <t-sen-kernel-Priority>),
  (link(<t-sen-kernel-PropertyCategorySpec>)[#"Property​Category​Spec"], "how a property is seen by others", <t-sen-kernel-PropertyCategorySpec>),
  (link(<t-sen-kernel-PropertyRelationSpec>)[#"Property​Relation​Spec"], "method property relation", <t-sen-kernel-PropertyRelationSpec>),
  (link(<t-sen-kernel-QueueEvictionPolicy>)[#"Queue​Eviction​Policy"], "Component queue eviction policy", <t-sen-kernel-QueueEvictionPolicy>),
  (link(<t-sen-kernel-RealType>)[#"Real​Type"], "built-in reals", <t-sen-kernel-RealType>),
  (link(<t-sen-kernel-RunMode>)[#"Run​Mode"], "Run modes for the kernel", <t-sen-kernel-RunMode>),
  (link(<t-sen-kernel-ThreadCreateErr>)[#"Thread​Create​Err"], "What can go wrong when creating a thread", <t-sen-kernel-ThreadCreateErr>),
  (link(<t-sen-kernel-TransportModeSpec>)[#"Transport​Mode​Spec"], "how to transport information", <t-sen-kernel-TransportModeSpec>),
  (link(<t-sen-kernel-UnitCat>)[#"Unit​Cat"], "Type of measurement", <t-sen-kernel-UnitCat>),
  (link(<t-sen-kernel-WordSize>)[#"Word​Size"], "Native processor word size", <t-sen-kernel-WordSize>),
)

==== #"BasicType" #chip("enumerations", "enumeration") <t-sen-kernel-BasicType>
#prose("built-in others")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("boolean​Type", [0]),
  ("string​Type", [1]),
  ("duration​Type", [2]),
  ("timestamp​Type", [3]),
)
#facts[Named by #link(<t-sen-kernel-BuiltInType>)[#"Built​In​Type"].]

==== #"CompatibilityMode" #chip("enumerations", "enumeration") <t-sen-kernel-CompatibilityMode>
#prose("What to do when a type published by another kernel does not match the local one")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("relaxed", [0]),
  ("strict", [1]),
  ("disabled", [2]),
)
#facts[Named by #link(<t-sen-kernel-KernelParams>)[#"Kernel​Params"].]

==== #"ComponentState" #chip("enumerations", "enumeration") <t-sen-kernel-ComponentState>
#prose("State of a kernel component")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("unloaded", [0]),
  ("preloaded", [1]),
  ("loading", [2]),
  ("loaded", [3]),
  ("initializing", [4]),
  ("initialized", [5]),
  ("running", [6]),
  ("stopping", [7]),
  ("stopped", [8]),
  ("unloading", [9]),
  ("error", [10]),
)

==== #"CpuArch" #chip("enumerations", "enumeration") <t-sen-kernel-CpuArch>
#prose("Type of CPU")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("x86", [0]),
  ("x64", [1]),
  ("arm2", [2]),
  ("arm3", [3]),
  ("arm4T", [4]),
  ("arm5", [5]),
  ("arm6T2", [6]),
  ("arm6", [7]),
  ("arm7", [8]),
  ("arm7a", [9]),
  ("arm7r", [10]),
  ("arm7s", [11]),
  ("arm64", [12]),
  ("mips", [13]),
  ("super​H", [14]),
  ("ppc", [15]),
  ("ppc64", [16]),
  ("sparc", [17]),
  ("m68k", [18]),
  ("other​Arch", [19]),
)
#facts[Named by #link(<t-sen-kernel-ProcessInfo>)[#"Process​Info"].]

==== #"ErrorCategory" #chip("enumerations", "enumeration") <t-sen-kernel-ErrorCategory>
#prose("Category of a problem during component execution")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("runtime​Error", [0]),
  ("logic​Error", [1]),
  ("expectations​Not​Met", [2]),
  ("io​Error", [3]),
  ("other", [4]),
)
#facts[Named by #link(<t-sen-kernel-ExecError>)[#"Exec​Error"].]

==== #"ExceptionKind" #chip("enumerations", "enumeration") <t-sen-kernel-ExceptionKind>
#prose("Types of exceptions that can occur.")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("runtime", [0]),
  ("logic", [1]),
  ("standard", [2]),
  ("unknown", [3]),
)
#facts[Named by #link(<t-sen-kernel-UncaughtException>)[#"Uncaught​Exception"].]

==== #"GitStatus" #chip("enumerations", "enumeration") <t-sen-kernel-GitStatus>
#prose("Status of git")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("clean", [0]),
  ("modified", [1]),
  ("unknown", [2]),
)
#facts[Named by #link(<t-sen-kernel-BuildInfo>)[#"Build​Info"], #link(<t-sen-kernel-SenData>)[#"Sen​Data"].]

==== #"IntegralType" #chip("enumerations", "enumeration") <t-sen-kernel-IntegralType>
#prose("built-in integrals")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("uint8Type", [0]),
  ("int16Type", [1]),
  ("uint16Type", [2]),
  ("int32Type", [3]),
  ("uint32Type", [4]),
  ("int64Type", [5]),
  ("uint64Type", [6]),
)
#facts[Named by #link(<t-sen-kernel-EnumTypeSpec>)[#"Enum​Type​Spec"], #link(<t-sen-kernel-EnumTypeSpecV4>)[#"Enum​Type​Spec​V4"], #link(<t-sen-kernel-EnumTypeSpecV5>)[#"Enum​Type​Spec​V5"], #link(<t-sen-kernel-NumericType>)[#"Numeric​Type"].]

==== #"MethodConstnessSpec" #chip("enumerations", "enumeration") <t-sen-kernel-MethodConstnessSpec>
#prose("method constness")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("constant", [0]),
  ("non​Constant", [1]),
)
#facts[Named by #link(<t-sen-kernel-MethodSpec>)[#"Method​Spec"], #link(<t-sen-kernel-MethodSpecV4>)[#"Method​Spec​V4"], #link(<t-sen-kernel-MethodSpecV5>)[#"Method​Spec​V5"].]

==== #"NetworkFootprintBusSource" #chip("enumerations", "enumeration") <t-sen-kernel-NetworkFootprintBusSource>
#prose("Bus source")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("configured", [0]),
  ("supplied", [1]),
  ("runtime", [2]),
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintBus>)[#"Network​Footprint​Bus"].]

==== #"NetworkFootprintPortKind" #chip("enumerations", "enumeration") <t-sen-kernel-NetworkFootprintPortKind>
#prose("Type of socket opened by the process")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("tcp​Acceptor", [0]),
  ("udp​Unicast", [1]),
  ("tcp​Source", [2]),
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintPort>)[#"Network​Footprint​Port"].]

==== #"NetworkFootprintPortMode" #chip("enumerations", "enumeration") <t-sen-kernel-NetworkFootprintPortMode>
#prose("Port allocation policy")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("ephemeral", [0]),
  ("probe", [1]),
  ("pinned", [2]),
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintPort>)[#"Network​Footprint​Port"].]

==== #"OsKind" #chip("enumerations", "enumeration") <t-sen-kernel-OsKind>
#prose("Type of operating system")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("windows​Os", [0]),
  ("linux​Os", [1]),
  ("android​Os", [2]),
  ("apple​Os", [3]),
  ("unix​Os", [4]),
  ("posix​Os", [5]),
  ("other​Os", [6]),
)
#facts[Named by #link(<t-sen-kernel-ProcessInfo>)[#"Process​Info"].]

==== #"Priority" #chip("enumerations", "enumeration") <t-sen-kernel-Priority>
#prose("Thread execution priorities")
#facts[Held as #mono("u32").]
#enum-columns(3,
  ("lowest", [0]),
  ("nominal​Min", [1]),
  ("nominal​Max", [2]),
  ("highest", [3]),
)
#facts[Named by #link(<t-sen-kernel-ComponentConfig>)[#"Component​Config"].]

==== #"PropertyCategorySpec" #chip("enumerations", "enumeration") <t-sen-kernel-PropertyCategorySpec>
#prose("how a property is seen by others")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("static​RW", [0]),
  ("static​RO", [1]),
  ("dynamic​RW", [2]),
  ("dynamic​RO", [3]),
)
#facts[Named by #link(<t-sen-kernel-PropertySpec>)[#"Property​Spec"], #link(<t-sen-kernel-PropertySpecV4>)[#"Property​Spec​V4"], #link(<t-sen-kernel-PropertySpecV5>)[#"Property​Spec​V5"].]

==== #"PropertyRelationSpec" #chip("enumerations", "enumeration") <t-sen-kernel-PropertyRelationSpec>
#prose("method property relation")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("non​Property​Related", [0]),
  ("property​Getter", [1]),
  ("property​Setter", [2]),
)
#facts[Named by #link(<t-sen-kernel-MethodSpec>)[#"Method​Spec"].]

==== #"QueueEvictionPolicy" #chip("enumerations", "enumeration") <t-sen-kernel-QueueEvictionPolicy>
#prose("Component queue eviction policy")
#facts[Held as #mono("u32").]
#enum-columns(3,
  ("drop​Oldest", [0]),
  ("drop​Newest", [1]),
)
#facts[Named by #link(<t-sen-kernel-QueueConfig>)[#"Queue​Config"].]

==== #"RealType" #chip("enumerations", "enumeration") <t-sen-kernel-RealType>
#prose("built-in reals")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("float32Type", [0]),
  ("float64Type", [1]),
)
#facts[Named by #link(<t-sen-kernel-NumericType>)[#"Numeric​Type"].]

==== #"RunMode" #chip("enumerations", "enumeration") <t-sen-kernel-RunMode>
#prose("Run modes for the kernel")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("real​Time", [0]),
  ("virtual​Time", [1]),
  ("virtual​Time​Running", [2]),
  ("start​And​Stop", [3]),
)
#facts[Named by #link(<t-sen-kernel-KernelParams>)[#"Kernel​Params"].]

==== #"ThreadCreateErr" #chip("enumerations", "enumeration") <t-sen-kernel-ThreadCreateErr>
#prose("What can go wrong when creating a thread")
#facts[Held as #mono("u32").]
#enum-columns(2,
  ("scheduler​Already​Started", [0]),
  ("invalid​Stack​Size", [1]),
  ("invalid​Thread​Function", [2]),
  ("invalid​Thread​Function​Argument", [3]),
  ("invalid​Affinity", [4]),
  ("too​Many​Threads", [5]),
  ("internal​Os​Error", [6]),
)

==== #"TransportModeSpec" #chip("enumerations", "enumeration") <t-sen-kernel-TransportModeSpec>
#prose("how to transport information")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("unicast", [0]),
  ("multicast", [1]),
  ("confirmed", [2]),
)
#facts[Named by #link(<t-sen-kernel-EventSpec>)[#"Event​Spec"], #link(<t-sen-kernel-EventSpecV4>)[#"Event​Spec​V4"], #link(<t-sen-kernel-EventSpecV5>)[#"Event​Spec​V5"], #link(<t-sen-kernel-MethodSpec>)[#"Method​Spec"], #link(<t-sen-kernel-MethodSpecV4>)[#"Method​Spec​V4"], #link(<t-sen-kernel-MethodSpecV5>)[#"Method​Spec​V5"], #link(<t-sen-kernel-PropertySpec>)[#"Property​Spec"], #link(<t-sen-kernel-PropertySpecV4>)[#"Property​Spec​V4"], #link(<t-sen-kernel-PropertySpecV5>)[#"Property​Spec​V5"].]

==== #"UnitCat" #chip("enumerations", "enumeration") <t-sen-kernel-UnitCat>
#prose("Type of measurement")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("length", [0]),
  ("mass", [1]),
  ("time", [2]),
  ("angle", [3]),
  ("temperature", [4]),
  ("frequency", [5]),
  ("velocity", [6]),
  ("angular​Velocity", [7]),
  ("acceleration", [8]),
  ("angular​Acceleration", [9]),
  ("density", [10]),
  ("pressure", [11]),
  ("area", [12]),
  ("force", [13]),
  ("torque", [14]),
)
#facts[Named by #link(<t-sen-kernel-UnitInfo>)[#"Unit​Info"].]

==== #"WordSize" #chip("enumerations", "enumeration") <t-sen-kernel-WordSize>
#prose("Native processor word size")
#facts[Held as #mono("u32").]
#enum-columns(3,
  ("bits32", [0]),
  ("bits64", [1]),
)
#facts[Named by #link(<t-sen-kernel-BuildInfo>)[#"Build​Info"], #link(<t-sen-kernel-SenData>)[#"Sen​Data"].]

#section("Variant records", "variants")

#summary(
  (link(<t-sen-kernel-BuiltInType>)[#"Built​In​Type"], "all built-in types", <t-sen-kernel-BuiltInType>),
  (link(<t-sen-kernel-CustomTypeData>)[#"Custom​Type​Data"], "all custom types", <t-sen-kernel-CustomTypeData>),
  (link(<t-sen-kernel-CustomTypeDataV4>)[#"Custom​Type​Data​V4"], "", <t-sen-kernel-CustomTypeDataV4>),
  (link(<t-sen-kernel-CustomTypeDataV5>)[#"Custom​Type​Data​V5"], "", <t-sen-kernel-CustomTypeDataV5>),
  (link(<t-sen-kernel-NetworkFootprintPortValue>)[#"Network​Footprint​Port​Value"], "Known port number or range", <t-sen-kernel-NetworkFootprintPortValue>),
  (link(<t-sen-kernel-NumericType>)[#"Numeric​Type"], "all built-in numeric types", <t-sen-kernel-NumericType>),
  (link(<t-sen-kernel-OpState>)[#"Op​State"], "The status of an operation", <t-sen-kernel-OpState>),
  (link(<t-sen-kernel-SleepPolicy>)[#"Sleep​Policy"], "Component sleep policy", <t-sen-kernel-SleepPolicy>),
)

==== #"BuiltInType" #chip("variants", "variant") <t-sen-kernel-BuiltInType>
#prose("all built-in types")
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-kernel-NumericType>)[#"Numeric​Type"], "",
  link(<t-sen-kernel-BasicType>)[#"Basic​Type"], "",
)

==== #"CustomTypeData" #chip("variants", "variant") <t-sen-kernel-CustomTypeData>
#prose("all custom types")
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-kernel-EnumTypeSpec>)[#"Enum​Type​Spec"], "",
  link(<t-sen-kernel-QuantityTypeSpec>)[#"Quantity​Type​Spec"], "",
  link(<t-sen-kernel-SequenceTypeSpec>)[#"Sequence​Type​Spec"], "",
  link(<t-sen-kernel-StructTypeSpec>)[#"Struct​Type​Spec"], "",
  link(<t-sen-kernel-VariantTypeSpec>)[#"Variant​Type​Spec"], "",
  link(<t-sen-kernel-AliasTypeSpec>)[#"Alias​Type​Spec"], "",
  link(<t-sen-kernel-OptionalTypeSpec>)[#"Optional​Type​Spec"], "",
  link(<t-sen-kernel-ClassTypeSpec>)[#"Class​Type​Spec"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeSpec>)[#"Custom​Type​Spec"].]

==== #"CustomTypeDataV4" #chip("variants", "variant") <t-sen-kernel-CustomTypeDataV4>
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-kernel-EnumTypeSpecV4>)[#"Enum​Type​Spec​V4"], "",
  link(<t-sen-kernel-QuantityTypeSpecV4>)[#"Quantity​Type​Spec​V4"], "",
  link(<t-sen-kernel-SequenceTypeSpecV4>)[#"Sequence​Type​Spec​V4"], "",
  link(<t-sen-kernel-StructTypeSpecV4>)[#"Struct​Type​Spec​V4"], "",
  link(<t-sen-kernel-VariantTypeSpecV4>)[#"Variant​Type​Spec​V4"], "",
  link(<t-sen-kernel-AliasTypeSpecV4>)[#"Alias​Type​Spec​V4"], "",
  link(<t-sen-kernel-OptionalTypeSpecV4>)[#"Optional​Type​Spec​V4"], "",
  link(<t-sen-kernel-ClassTypeSpecV4>)[#"Class​Type​Spec​V4"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeSpecV4>)[#"Custom​Type​Spec​V4"].]

==== #"CustomTypeDataV5" #chip("variants", "variant") <t-sen-kernel-CustomTypeDataV5>
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-kernel-EnumTypeSpecV5>)[#"Enum​Type​Spec​V5"], "",
  link(<t-sen-kernel-QuantityTypeSpecV5>)[#"Quantity​Type​Spec​V5"], "",
  link(<t-sen-kernel-SequenceTypeSpecV5>)[#"Sequence​Type​Spec​V5"], "",
  link(<t-sen-kernel-StructTypeSpecV5>)[#"Struct​Type​Spec​V5"], "",
  link(<t-sen-kernel-VariantTypeSpecV5>)[#"Variant​Type​Spec​V5"], "",
  link(<t-sen-kernel-AliasTypeSpecV5>)[#"Alias​Type​Spec​V5"], "",
  link(<t-sen-kernel-OptionalTypeSpecV5>)[#"Optional​Type​Spec​V5"], "",
  link(<t-sen-kernel-ClassTypeSpecV5>)[#"Class​Type​Spec​V5"], "",
)
#facts[Named by #link(<t-sen-kernel-CustomTypeSpecV5>)[#"Custom​Type​Spec​V5"].]

==== #"NetworkFootprintPortValue" #chip("variants", "variant") <t-sen-kernel-NetworkFootprintPortValue>
#prose("Known port number or range")
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-u16>)[#"u16"], "",
  link(<t-sen-kernel-NetworkFootprintPortRange>)[#"Network​Footprint​Port​Range"], "",
)
#facts[Named by #link(<t-sen-kernel-NetworkFootprintPort>)[#"Network​Footprint​Port"].]

==== #"NumericType" #chip("variants", "variant") <t-sen-kernel-NumericType>
#prose("all built-in numeric types")
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-kernel-IntegralType>)[#"Integral​Type"], "",
  link(<t-sen-kernel-RealType>)[#"Real​Type"], "",
)
#facts[Named by #link(<t-sen-kernel-BuiltInType>)[#"Built​In​Type"], #link(<t-sen-kernel-QuantityTypeSpec>)[#"Quantity​Type​Spec"], #link(<t-sen-kernel-QuantityTypeSpecV4>)[#"Quantity​Type​Spec​V4"], #link(<t-sen-kernel-QuantityTypeSpecV5>)[#"Quantity​Type​Spec​V5"].]

==== #"OpState" #chip("variants", "variant") <t-sen-kernel-OpState>
#prose("The status of an operation")
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-kernel-OpFinished>)[#"Op​Finished"], "done",
  link(<t-sen-kernel-OpNotFinished>)[#"Op​Not​Finished"], "not done",
)

==== #"SleepPolicy" #chip("variants", "variant") <t-sen-kernel-SleepPolicy>
#prose("Component sleep policy")
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-kernel-PrecisionSleep>)[#"Precision​Sleep"], "",
  link(<t-sen-kernel-SystemSleep>)[#"System​Sleep"], "",
)
#facts[Named by #link(<t-sen-kernel-ComponentConfig>)[#"Component​Config"], #link(<t-sen-kernel-KernelParams>)[#"Kernel​Params"].]

#section("Arrays", "sequences")

#summary(
  (link(<t-sen-kernel-ArgSpecList>)[#"Arg​Spec​List"], "list of argument specs", <t-sen-kernel-ArgSpecList>),
  (link(<t-sen-kernel-ArgSpecListV4>)[#"Arg​Spec​List​V4"], "", <t-sen-kernel-ArgSpecListV4>),
  (link(<t-sen-kernel-ArgSpecListV5>)[#"Arg​Spec​List​V5"], "", <t-sen-kernel-ArgSpecListV5>),
  (link(<t-sen-kernel-Buffer>)[#"Buffer"], "A general-purpose unbounded buffer", <t-sen-kernel-Buffer>),
  (link(<t-sen-kernel-BuiltComponentList>)[#"Built​Component​List"], "", <t-sen-kernel-BuiltComponentList>),
  (link(<t-sen-kernel-CustomTypeSpecList>)[#"Custom​Type​Spec​List"], "list of custom types", <t-sen-kernel-CustomTypeSpecList>),
  (link(<t-sen-kernel-CustomTypeSpecListV4>)[#"Custom​Type​Spec​List​V4"], "", <t-sen-kernel-CustomTypeSpecListV4>),
  (link(<t-sen-kernel-CustomTypeSpecListV5>)[#"Custom​Type​Spec​List​V5"], "", <t-sen-kernel-CustomTypeSpecListV5>),
  (link(<t-sen-kernel-EnumeratorSpecList>)[#"Enumerator​Spec​List"], "list of enumerators", <t-sen-kernel-EnumeratorSpecList>),
  (link(<t-sen-kernel-EnumeratorSpecListV4>)[#"Enumerator​Spec​List​V4"], "", <t-sen-kernel-EnumeratorSpecListV4>),
  (link(<t-sen-kernel-EnumeratorSpecListV5>)[#"Enumerator​Spec​List​V5"], "", <t-sen-kernel-EnumeratorSpecListV5>),
  (link(<t-sen-kernel-EnvironmentVarList>)[#"Environment​Var​List"], "", <t-sen-kernel-EnvironmentVarList>),
  (link(<t-sen-kernel-EventSpecList>)[#"Event​Spec​List"], "list of event specs", <t-sen-kernel-EventSpecList>),
  (link(<t-sen-kernel-EventSpecListV4>)[#"Event​Spec​List​V4"], "", <t-sen-kernel-EventSpecListV4>),
  (link(<t-sen-kernel-EventSpecListV5>)[#"Event​Spec​List​V5"], "", <t-sen-kernel-EventSpecListV5>),
  (link(<t-sen-kernel-LoadedComponentList>)[#"Loaded​Component​List"], "", <t-sen-kernel-LoadedComponentList>),
  (link(<t-sen-kernel-MethodSpecList>)[#"Method​Spec​List"], "list of method specs", <t-sen-kernel-MethodSpecList>),
  (link(<t-sen-kernel-MethodSpecListV4>)[#"Method​Spec​List​V4"], "", <t-sen-kernel-MethodSpecListV4>),
  (link(<t-sen-kernel-MethodSpecListV5>)[#"Method​Spec​List​V5"], "", <t-sen-kernel-MethodSpecListV5>),
  (link(<t-sen-kernel-NetworkFootprintAddressRangeList>)[#"Network​Footprint​Address​Range​List"], "", <t-sen-kernel-NetworkFootprintAddressRangeList>),
  (link(<t-sen-kernel-NetworkFootprintBusList>)[#"Network​Footprint​Bus​List"], "Buses included in a network footprint", <t-sen-kernel-NetworkFootprintBusList>),
  (link(<t-sen-kernel-NetworkFootprintPortList>)[#"Network​Footprint​Port​List"], "Ports used by the process", <t-sen-kernel-NetworkFootprintPortList>),
  (link(<t-sen-kernel-NetworkFootprintPortRangeList>)[#"Network​Footprint​Port​Range​List"], "", <t-sen-kernel-NetworkFootprintPortRangeList>),
  (link(<t-sen-kernel-NetworkFootprintSelfCollisionList>)[#"Network​Footprint​Self​Collision​List"], "Multicast address conflicts found in the reported buses", <t-sen-kernel-NetworkFootprintSelfCollisionList>),
  (link(<t-sen-kernel-Processes>)[#"Processes"], "Unbounded sequence of ProcessInfo", <t-sen-kernel-Processes>),
  (link(<t-sen-kernel-PropertySpecList>)[#"Property​Spec​List"], "list of property specs", <t-sen-kernel-PropertySpecList>),
  (link(<t-sen-kernel-PropertySpecListV4>)[#"Property​Spec​List​V4"], "", <t-sen-kernel-PropertySpecListV4>),
  (link(<t-sen-kernel-PropertySpecListV5>)[#"Property​Spec​List​V5"], "", <t-sen-kernel-PropertySpecListV5>),
  (link(<t-sen-kernel-StringList>)[#"String​List"], "Unbounded sequence of strings for general purpose", <t-sen-kernel-StringList>),
  (link(<t-sen-kernel-StructTypeFieldSpecList>)[#"Struct​Type​Field​Spec​List"], "list of struct fields", <t-sen-kernel-StructTypeFieldSpecList>),
  (link(<t-sen-kernel-StructTypeFieldSpecListV4>)[#"Struct​Type​Field​Spec​List​V4"], "", <t-sen-kernel-StructTypeFieldSpecListV4>),
  (link(<t-sen-kernel-StructTypeFieldSpecListV5>)[#"Struct​Type​Field​Spec​List​V5"], "", <t-sen-kernel-StructTypeFieldSpecListV5>),
  (link(<t-sen-kernel-U32List>)[#"U32List"], "", <t-sen-kernel-U32List>),
  (link(<t-sen-kernel-UnitList>)[#"Unit​List"], "Unbounded sequence of unit information structs", <t-sen-kernel-UnitList>),
  (link(<t-sen-kernel-VariantTypeFieldSpecList>)[#"Variant​Type​Field​Spec​List"], "list of variant fields", <t-sen-kernel-VariantTypeFieldSpecList>),
  (link(<t-sen-kernel-VariantTypeFieldSpecListV4>)[#"Variant​Type​Field​Spec​List​V4"], "", <t-sen-kernel-VariantTypeFieldSpecListV4>),
  (link(<t-sen-kernel-VariantTypeFieldSpecListV5>)[#"Variant​Type​Field​Spec​List​V5"], "", <t-sen-kernel-VariantTypeFieldSpecListV5>),
)

==== #"ArgSpecList" #chip("sequences", "sequence") <t-sen-kernel-ArgSpecList>
#prose("list of argument specs")
#declared[#kw[sequence]\<#link(<t-sen-kernel-ArgSpec>)[#"Arg​Spec"]\> #"Arg​Spec​List"#";"]
#facts[Named by #link(<t-sen-kernel-EventSpec>)[#"Event​Spec"], #link(<t-sen-kernel-MethodSpec>)[#"Method​Spec"].]

==== #"ArgSpecListV4" #chip("sequences", "sequence") <t-sen-kernel-ArgSpecListV4>
#declared[#kw[sequence]\<#link(<t-sen-kernel-ArgSpecV4>)[#"Arg​Spec​V4"]\> #"Arg​Spec​List​V4"#";"]
#facts[Named by #link(<t-sen-kernel-EventSpecV4>)[#"Event​Spec​V4"], #link(<t-sen-kernel-MethodSpecV4>)[#"Method​Spec​V4"].]

==== #"ArgSpecListV5" #chip("sequences", "sequence") <t-sen-kernel-ArgSpecListV5>
#declared[#kw[sequence]\<#link(<t-sen-kernel-ArgSpecV5>)[#"Arg​Spec​V5"]\> #"Arg​Spec​List​V5"#";"]
#facts[Named by #link(<t-sen-kernel-EventSpecV5>)[#"Event​Spec​V5"], #link(<t-sen-kernel-MethodSpecV5>)[#"Method​Spec​V5"].]

==== #"Buffer" #chip("sequences", "sequence") <t-sen-kernel-Buffer>
#prose("A general-purpose unbounded buffer")
#declared[#kw[sequence]\<#link(<t-u8>)[#"u8"]\> #"Buffer"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-StaticFile>)[#"sen.​components.​jsonrpc.​Static​File"].]

==== #"BuiltComponentList" #chip("sequences", "sequence") <t-sen-kernel-BuiltComponentList>
#declared[#kw[sequence]\<#link(<t-sen-kernel-BuiltComponentParams>)[#"Built​Component​Params"]\> #"Built​Component​List"#";"]
#facts[Named by #link(<t-sen-kernel-SenData>)[#"Sen​Data"].]

==== #"CustomTypeSpecList" #chip("sequences", "sequence") <t-sen-kernel-CustomTypeSpecList>
#prose("list of custom types")
#declared[#kw[sequence]\<#link(<t-sen-kernel-CustomTypeSpec>)[#"Custom​Type​Spec"]\> #"Custom​Type​Spec​List"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"sen.​components.​jsonrpc.​Json​Rpc​Server"].]

==== #"CustomTypeSpecListV4" #chip("sequences", "sequence") <t-sen-kernel-CustomTypeSpecListV4>
#declared[#kw[sequence]\<#link(<t-sen-kernel-CustomTypeSpecV4>)[#"Custom​Type​Spec​V4"]\> #"Custom​Type​Spec​List​V4"#";"]

==== #"CustomTypeSpecListV5" #chip("sequences", "sequence") <t-sen-kernel-CustomTypeSpecListV5>
#declared[#kw[sequence]\<#link(<t-sen-kernel-CustomTypeSpecV5>)[#"Custom​Type​Spec​V5"]\> #"Custom​Type​Spec​List​V5"#";"]

==== #"EnumeratorSpecList" #chip("sequences", "sequence") <t-sen-kernel-EnumeratorSpecList>
#prose("list of enumerators")
#declared[#kw[sequence]\<#link(<t-sen-kernel-EnumeratorSpec>)[#"Enumerator​Spec"]\> #"Enumerator​Spec​List"#";"]
#facts[Named by #link(<t-sen-kernel-EnumTypeSpec>)[#"Enum​Type​Spec"].]

==== #"EnumeratorSpecListV4" #chip("sequences", "sequence") <t-sen-kernel-EnumeratorSpecListV4>
#declared[#kw[sequence]\<#link(<t-sen-kernel-EnumeratorSpecV4>)[#"Enumerator​Spec​V4"]\> #"Enumerator​Spec​List​V4"#";"]
#facts[Named by #link(<t-sen-kernel-EnumTypeSpecV4>)[#"Enum​Type​Spec​V4"].]

==== #"EnumeratorSpecListV5" #chip("sequences", "sequence") <t-sen-kernel-EnumeratorSpecListV5>
#declared[#kw[sequence]\<#link(<t-sen-kernel-EnumeratorSpecV5>)[#"Enumerator​Spec​V5"]\> #"Enumerator​Spec​List​V5"#";"]
#facts[Named by #link(<t-sen-kernel-EnumTypeSpecV5>)[#"Enum​Type​Spec​V5"].]

==== #"EnvironmentVarList" #chip("sequences", "sequence") <t-sen-kernel-EnvironmentVarList>
#declared[#kw[sequence]\<#link(<t-sen-kernel-EnvVar>)[#"Env​Var"]\> #"Environment​Var​List"#";"]
#facts[Named by #link(<t-sen-kernel-ProcessData>)[#"Process​Data"].]

==== #"EventSpecList" #chip("sequences", "sequence") <t-sen-kernel-EventSpecList>
#prose("list of event specs")
#declared[#kw[sequence]\<#link(<t-sen-kernel-EventSpec>)[#"Event​Spec"]\> #"Event​Spec​List"#";"]
#facts[Named by #link(<t-sen-kernel-ClassTypeSpec>)[#"Class​Type​Spec"].]

==== #"EventSpecListV4" #chip("sequences", "sequence") <t-sen-kernel-EventSpecListV4>
#declared[#kw[sequence]\<#link(<t-sen-kernel-EventSpecV4>)[#"Event​Spec​V4"]\> #"Event​Spec​List​V4"#";"]
#facts[Named by #link(<t-sen-kernel-ClassTypeSpecV4>)[#"Class​Type​Spec​V4"].]

==== #"EventSpecListV5" #chip("sequences", "sequence") <t-sen-kernel-EventSpecListV5>
#declared[#kw[sequence]\<#link(<t-sen-kernel-EventSpecV5>)[#"Event​Spec​V5"]\> #"Event​Spec​List​V5"#";"]
#facts[Named by #link(<t-sen-kernel-ClassTypeSpecV5>)[#"Class​Type​Spec​V5"].]

==== #"LoadedComponentList" #chip("sequences", "sequence") <t-sen-kernel-LoadedComponentList>
#declared[#kw[sequence]\<#link(<t-sen-kernel-LoadedComponentParams>)[#"Loaded​Component​Params"]\> #"Loaded​Component​List"#";"]
#facts[Named by #link(<t-sen-kernel-SenData>)[#"Sen​Data"].]

==== #"MethodSpecList" #chip("sequences", "sequence") <t-sen-kernel-MethodSpecList>
#prose("list of method specs")
#declared[#kw[sequence]\<#link(<t-sen-kernel-MethodSpec>)[#"Method​Spec"]\> #"Method​Spec​List"#";"]
#facts[Named by #link(<t-sen-kernel-ClassTypeSpec>)[#"Class​Type​Spec"].]

==== #"MethodSpecListV4" #chip("sequences", "sequence") <t-sen-kernel-MethodSpecListV4>
#declared[#kw[sequence]\<#link(<t-sen-kernel-MethodSpecV4>)[#"Method​Spec​V4"]\> #"Method​Spec​List​V4"#";"]
#facts[Named by #link(<t-sen-kernel-ClassTypeSpecV4>)[#"Class​Type​Spec​V4"].]

==== #"MethodSpecListV5" #chip("sequences", "sequence") <t-sen-kernel-MethodSpecListV5>
#declared[#kw[sequence]\<#link(<t-sen-kernel-MethodSpecV5>)[#"Method​Spec​V5"]\> #"Method​Spec​List​V5"#";"]
#facts[Named by #link(<t-sen-kernel-ClassTypeSpecV5>)[#"Class​Type​Spec​V5"].]

==== #"NetworkFootprintAddressRangeList" #chip("sequences", "sequence") <t-sen-kernel-NetworkFootprintAddressRangeList>
#declared[#kw[sequence]\<#link(<t-sen-kernel-NetworkFootprintAddressRange>)[#"Network​Footprint​Address​Range"]\> #"Network​Footprint​Address​Range​List"#";"]
#facts[Named by #link(<t-sen-kernel-NetworkFootprintMulticast>)[#"Network​Footprint​Multicast"].]

==== #"NetworkFootprintBusList" #chip("sequences", "sequence") <t-sen-kernel-NetworkFootprintBusList>
#prose("Buses included in a network footprint")
#declared[#kw[sequence]\<#link(<t-sen-kernel-NetworkFootprintBus>)[#"Network​Footprint​Bus"]\> #"Network​Footprint​Bus​List"#";"]
#facts[Named by #link(<t-sen-kernel-NetworkFootprintMulticast>)[#"Network​Footprint​Multicast"].]

==== #"NetworkFootprintPortList" #chip("sequences", "sequence") <t-sen-kernel-NetworkFootprintPortList>
#prose("Ports used by the process")
#declared[#kw[sequence]\<#link(<t-sen-kernel-NetworkFootprintPort>)[#"Network​Footprint​Port"]\> #"Network​Footprint​Port​List"#";"]
#facts[Named by #link(<t-sen-kernel-NetworkFootprint>)[#"Network​Footprint"].]

==== #"NetworkFootprintPortRangeList" #chip("sequences", "sequence") <t-sen-kernel-NetworkFootprintPortRangeList>
#declared[#kw[sequence]\<#link(<t-sen-kernel-NetworkFootprintPortRange>)[#"Network​Footprint​Port​Range"]\> #"Network​Footprint​Port​Range​List"#";"]
#facts[Named by #link(<t-sen-kernel-NetworkFootprintPortExclusions>)[#"Network​Footprint​Port​Exclusions"].]

==== #"NetworkFootprintSelfCollisionList" #chip("sequences", "sequence") <t-sen-kernel-NetworkFootprintSelfCollisionList>
#prose("Multicast address conflicts found in the reported buses")
#declared[#kw[sequence]\<#link(<t-sen-kernel-NetworkFootprintSelfCollision>)[#"Network​Footprint​Self​Collision"]\> #"Network​Footprint​Self​Collision​List"#";"]
#facts[Named by #link(<t-sen-kernel-NetworkFootprintCollision>)[#"Network​Footprint​Collision"].]

==== #"Processes" #chip("sequences", "sequence") <t-sen-kernel-Processes>
#prose("Unbounded sequence of ProcessInfo")
#declared[#kw[sequence]\<#link(<t-sen-kernel-ProcessInfo>)[#"Process​Info"]\> #"Processes"#";"]

==== #"PropertySpecList" #chip("sequences", "sequence") <t-sen-kernel-PropertySpecList>
#prose("list of property specs")
#declared[#kw[sequence]\<#link(<t-sen-kernel-PropertySpec>)[#"Property​Spec"]\> #"Property​Spec​List"#";"]
#facts[Named by #link(<t-sen-kernel-ClassTypeSpec>)[#"Class​Type​Spec"].]

==== #"PropertySpecListV4" #chip("sequences", "sequence") <t-sen-kernel-PropertySpecListV4>
#declared[#kw[sequence]\<#link(<t-sen-kernel-PropertySpecV4>)[#"Property​Spec​V4"]\> #"Property​Spec​List​V4"#";"]
#facts[Named by #link(<t-sen-kernel-ClassTypeSpecV4>)[#"Class​Type​Spec​V4"].]

==== #"PropertySpecListV5" #chip("sequences", "sequence") <t-sen-kernel-PropertySpecListV5>
#declared[#kw[sequence]\<#link(<t-sen-kernel-PropertySpecV5>)[#"Property​Spec​V5"]\> #"Property​Spec​List​V5"#";"]
#facts[Named by #link(<t-sen-kernel-ClassTypeSpecV5>)[#"Class​Type​Spec​V5"].]

==== #"StringList" #chip("sequences", "sequence") <t-sen-kernel-StringList>
#prose("Unbounded sequence of strings for general purpose")
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"String​List"#";"]
#facts[Named by #link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"sen.​components.​jsonrpc.​Json​Rpc​Server"], #link(<t-sen-components-jsonrpc-NamedSelection>)[#"sen.​components.​jsonrpc.​Named​Selection"], #link(<t-sen-components-jsonrpc-SessionInfo>)[#"sen.​components.​jsonrpc.​Session​Info"], #link(<t-sen-kernel-BuiltComponentParams>)[#"Built​Component​Params"], #link(<t-sen-kernel-ClassTypeSpec>)[#"Class​Type​Spec"], #link(<t-sen-kernel-ClassTypeSpecV4>)[#"Class​Type​Spec​V4"], #link(<t-sen-kernel-ClassTypeSpecV5>)[#"Class​Type​Spec​V5"], #link(<t-sen-kernel-ErrorData>)[#"Error​Data"], #link(<t-sen-kernel-KernelApi>)[#"Kernel​Api"], #link(<t-sen-kernel-ProcessData>)[#"Process​Data"], #link(<t-sen-kernel-PropertySpec>)[#"Property​Spec"], #link(<t-sen-kernel-PropertySpecV4>)[#"Property​Spec​V4"], #link(<t-sen-kernel-PropertySpecV5>)[#"Property​Spec​V5"].]

==== #"StructTypeFieldSpecList" #chip("sequences", "sequence") <t-sen-kernel-StructTypeFieldSpecList>
#prose("list of struct fields")
#declared[#kw[sequence]\<#link(<t-sen-kernel-StructTypeFieldSpec>)[#"Struct​Type​Field​Spec"]\> #"Struct​Type​Field​Spec​List"#";"]
#facts[Named by #link(<t-sen-kernel-StructTypeSpec>)[#"Struct​Type​Spec"].]

==== #"StructTypeFieldSpecListV4" #chip("sequences", "sequence") <t-sen-kernel-StructTypeFieldSpecListV4>
#declared[#kw[sequence]\<#link(<t-sen-kernel-StructTypeFieldSpecV4>)[#"Struct​Type​Field​Spec​V4"]\> #"Struct​Type​Field​Spec​List​V4"#";"]
#facts[Named by #link(<t-sen-kernel-StructTypeSpecV4>)[#"Struct​Type​Spec​V4"].]

==== #"StructTypeFieldSpecListV5" #chip("sequences", "sequence") <t-sen-kernel-StructTypeFieldSpecListV5>
#declared[#kw[sequence]\<#link(<t-sen-kernel-StructTypeFieldSpecV5>)[#"Struct​Type​Field​Spec​V5"]\> #"Struct​Type​Field​Spec​List​V5"#";"]
#facts[Named by #link(<t-sen-kernel-StructTypeSpecV5>)[#"Struct​Type​Spec​V5"].]

==== #"U32List" #chip("sequences", "sequence") <t-sen-kernel-U32List>
#declared[#kw[sequence]\<#link(<t-u32>)[#"u32"]\> #"U32List"#";"]

==== #"UnitList" #chip("sequences", "sequence") <t-sen-kernel-UnitList>
#prose("Unbounded sequence of unit information structs")
#declared[#kw[sequence]\<#link(<t-sen-kernel-UnitInfo>)[#"Unit​Info"]\> #"Unit​List"#";"]
#facts[Named by #link(<t-sen-kernel-KernelApi>)[#"Kernel​Api"].]

==== #"VariantTypeFieldSpecList" #chip("sequences", "sequence") <t-sen-kernel-VariantTypeFieldSpecList>
#prose("list of variant fields")
#declared[#kw[sequence]\<#link(<t-sen-kernel-VariantTypeFieldSpec>)[#"Variant​Type​Field​Spec"]\> #"Variant​Type​Field​Spec​List"#";"]
#facts[Named by #link(<t-sen-kernel-VariantTypeSpec>)[#"Variant​Type​Spec"].]

==== #"VariantTypeFieldSpecListV4" #chip("sequences", "sequence") <t-sen-kernel-VariantTypeFieldSpecListV4>
#declared[#kw[sequence]\<#link(<t-sen-kernel-VariantTypeFieldSpecV4>)[#"Variant​Type​Field​Spec​V4"]\> #"Variant​Type​Field​Spec​List​V4"#";"]
#facts[Named by #link(<t-sen-kernel-VariantTypeSpecV4>)[#"Variant​Type​Spec​V4"].]

==== #"VariantTypeFieldSpecListV5" #chip("sequences", "sequence") <t-sen-kernel-VariantTypeFieldSpecListV5>
#declared[#kw[sequence]\<#link(<t-sen-kernel-VariantTypeFieldSpecV5>)[#"Variant​Type​Field​Spec​V5"]\> #"Variant​Type​Field​Spec​List​V5"#";"]
#facts[Named by #link(<t-sen-kernel-VariantTypeSpecV5>)[#"Variant​Type​Spec​V5"].]

#section("Optionals", "optionals")

#summary(
  (link(<t-sen-kernel-MaybeException>)[#"Maybe​Exception"], "", <t-sen-kernel-MaybeException>),
  (link(<t-sen-kernel-MaybeF64>)[#"Maybe​F64"], "optional value for a f64", <t-sen-kernel-MaybeF64>),
  (link(<t-sen-kernel-MaybeNetworkFootprintMulticast>)[#"Maybe​Network​Footprint​Multicast"], "", <t-sen-kernel-MaybeNetworkFootprintMulticast>),
  (link(<t-sen-kernel-MaybeNetworkFootprintPortValue>)[#"Maybe​Network​Footprint​Port​Value"], "The value is not present when the port number is not known", <t-sen-kernel-MaybeNetworkFootprintPortValue>),
  (link(<t-sen-kernel-MaybeSignal>)[#"Maybe​Signal"], "", <t-sen-kernel-MaybeSignal>),
  (link(<t-sen-kernel-MaybeStacktrace>)[#"Maybe​Stacktrace"], "", <t-sen-kernel-MaybeStacktrace>),
  (link(<t-sen-kernel-MaybeTransportProtocol>)[#"Maybe​Transport​Protocol"], "", <t-sen-kernel-MaybeTransportProtocol>),
  (link(<t-sen-kernel-MaybeU64>)[#"Maybe​U64"], "optional value for a u64", <t-sen-kernel-MaybeU64>),
)

==== #"MaybeException" #chip("optionals", "optional") <t-sen-kernel-MaybeException>
#declared[#kw[optional]\<#link(<t-sen-kernel-UncaughtException>)[#"Uncaught​Exception"]\> #"Maybe​Exception"#";"]

==== #"MaybeF64" #chip("optionals", "optional") <t-sen-kernel-MaybeF64>
#prose("optional value for a f64")
#declared[#kw[optional]\<#link(<t-f64>)[#"f64"]\> #"Maybe​F64"#";"]

==== #"MaybeNetworkFootprintMulticast" #chip("optionals", "optional") <t-sen-kernel-MaybeNetworkFootprintMulticast>
#declared[#kw[optional]\<#link(<t-sen-kernel-NetworkFootprintMulticast>)[#"Network​Footprint​Multicast"]\> #"Maybe​Network​Footprint​Multicast"#";"]

==== #"MaybeNetworkFootprintPortValue" #chip("optionals", "optional") <t-sen-kernel-MaybeNetworkFootprintPortValue>
#prose("The value is not present when the port number is not known")
#declared[#kw[optional]\<#link(<t-sen-kernel-NetworkFootprintPortValue>)[#"Network​Footprint​Port​Value"]\> #"Maybe​Network​Footprint​Port​Value"#";"]

==== #"MaybeSignal" #chip("optionals", "optional") <t-sen-kernel-MaybeSignal>
#declared[#kw[optional]\<#link(<t-sen-kernel-SignalData>)[#"Signal​Data"]\> #"Maybe​Signal"#";"]

==== #"MaybeStacktrace" #chip("optionals", "optional") <t-sen-kernel-MaybeStacktrace>
#declared[#kw[optional]\<#link(<t-sen-kernel-StringList>)[#"String​List"]\> #"Maybe​Stacktrace"#";"]

==== #"MaybeTransportProtocol" #chip("optionals", "optional") <t-sen-kernel-MaybeTransportProtocol>
#declared[#kw[optional]\<#link(<t-u32>)[#"u32"]\> #"Maybe​Transport​Protocol"#";"]

==== #"MaybeU64" #chip("optionals", "optional") <t-sen-kernel-MaybeU64>
#prose("optional value for a u64")
#declared[#kw[optional]\<#link(<t-u64>)[#"u64"]\> #"Maybe​U64"#";"]

== #"log"

#prose[16 types.]

#section("Fixed records", "structures")

#summary(
  (link(<t-sen-kernel-log-BasicFile>)[#"Basic​File"], "Basic file sink", <t-sen-kernel-log-BasicFile>),
  (link(<t-sen-kernel-log-ColorStderr>)[#"Color​Stderr"], "Colored stderr sink", <t-sen-kernel-log-ColorStderr>),
  (link(<t-sen-kernel-log-ColorStdout>)[#"Color​Stdout"], "Colored stdout sink", <t-sen-kernel-log-ColorStdout>),
  (link(<t-sen-kernel-log-Config>)[#"Config"], "Overall component configuration", <t-sen-kernel-log-Config>),
  (link(<t-sen-kernel-log-LoggerConfig>)[#"Logger​Config"], "Configuration used for a given spdlog logger.", <t-sen-kernel-log-LoggerConfig>),
  (link(<t-sen-kernel-log-Null>)[#"Null"], "Single-threaded null sink", <t-sen-kernel-log-Null>),
  (link(<t-sen-kernel-log-RotatingFile>)[#"Rotating​File"], "Rotating file sink", <t-sen-kernel-log-RotatingFile>),
  (link(<t-sen-kernel-log-SinkConfig>)[#"Sink​Config"], "Configuration used for a given spdlog Sink.", <t-sen-kernel-log-SinkConfig>),
  (link(<t-sen-kernel-log-Stderr>)[#"Stderr"], "stderr sink", <t-sen-kernel-log-Stderr>),
  (link(<t-sen-kernel-log-Stdout>)[#"Stdout"], "stdout sink", <t-sen-kernel-log-Stdout>),
  (link(<t-sen-kernel-log-Syslog>)[#"Syslog"], "Syslog sink (only in linux)", <t-sen-kernel-log-Syslog>),
)

==== #"BasicFile" #chip("structures", "structure") <t-sen-kernel-log-BasicFile>
#prose("Basic file sink")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("file​Name")], link(<t-string>)[#"string"], "file to log to",
  [#mono("truncate")], link(<t-bool>)[#"bool"], "defaults to false",
  [#mono("create​Parent​Dir")], link(<t-bool>)[#"bool"], "true if we should create the parent dir first",
)
#facts[Named by #link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"].]

==== #"ColorStderr" #chip("structures", "structure") <t-sen-kernel-log-ColorStderr>
#prose("Colored stderr sink")
#facts[Named by #link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"].]

==== #"ColorStdout" #chip("structures", "structure") <t-sen-kernel-log-ColorStdout>
#prose("Colored stdout sink")
#facts[Named by #link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"].]

==== #"Config" #chip("structures", "structure") <t-sen-kernel-log-Config>
#prose("Overall component configuration")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("pattern")], link(<t-string>)[#"string"], "default format (empty means default)",
  [#mono("level")], link(<t-sen-kernel-log-LogLevel>)[#"Log​Level"], "default global level for all",
  [#mono("sinks")], link(<t-sen-kernel-log-SinkConfigList>)[#"Sink​Config​List"], "sinks to configure",
  [#mono("loggers")], link(<t-sen-kernel-log-LoggerConfigList>)[#"Logger​Config​List"], "loggers to configure",
  [#mono("backtrace")], link(<t-bool>)[#"bool"], "log backtrace is printed on failure",
)
#facts[Named by #link(<t-sen-kernel-KernelParams>)[#"sen.​kernel.​Kernel​Params"].]

==== #"LoggerConfig" #chip("structures", "structure") <t-sen-kernel-log-LoggerConfig>
#prose("Configuration used for a given spdlog logger.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the logger",
  [#mono("sinks")], link(<t-sen-kernel-log-SinkNameList>)[#"Sink​Name​List"], "name of the sinks to connect the logger to",
  [#mono("level")], link(<t-sen-kernel-log-LogLevel>)[#"Log​Level"], "default log level for this logger",
  [#mono("pattern")], link(<t-string>)[#"string"], "Formatting for the sinks in this logger. Does nothing if empty. Note that this replaces the format of the associated sinks, and this could affect other loggers using those sinks. The config is applied following the order of the Config.loggers field.",
)
#facts[Named by #link(<t-sen-kernel-log-LoggerConfigList>)[#"Logger​Config​List"].]

==== #"Null" #chip("structures", "structure") <t-sen-kernel-log-Null>
#prose("Single-threaded null sink")
#facts[Named by #link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"].]

==== #"RotatingFile" #chip("structures", "structure") <t-sen-kernel-log-RotatingFile>
#prose("Rotating file sink")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("base​File​Name")], link(<t-string>)[#"string"], "",
  [#mono("max​Size")], link(<t-u32>)[#"u32"], "0 means use defaults",
  [#mono("max​Files")], link(<t-u32>)[#"u32"], "0 means use defaults",
)
#facts[Named by #link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"].]

==== #"SinkConfig" #chip("structures", "structure") <t-sen-kernel-log-SinkConfig>
#prose("Configuration used for a given spdlog Sink.")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("name")], link(<t-string>)[#"string"], "name of the sink",
  [#mono("single​Threaded")], link(<t-bool>)[#"bool"], "if true, single-threaded (defaults to false)",
  [#mono("pattern")], link(<t-string>)[#"string"], "the pattern used for the formatting (empty means default).",
  [#mono("level")], link(<t-sen-kernel-log-LogLevel>)[#"Log​Level"], "default log level for this sink",
  [#mono("config")], link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"], "the type-specific configuration parameters",
)
#facts[Named by #link(<t-sen-kernel-log-SinkConfigList>)[#"Sink​Config​List"].]

==== #"Stderr" #chip("structures", "structure") <t-sen-kernel-log-Stderr>
#prose("stderr sink")
#facts[Named by #link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"].]

==== #"Stdout" #chip("structures", "structure") <t-sen-kernel-log-Stdout>
#prose("stdout sink")
#facts[Named by #link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"].]

==== #"Syslog" #chip("structures", "structure") <t-sen-kernel-log-Syslog>
#prose("Syslog sink (only in linux)")
#sen-table(
  columns: (COL-NAME, COL-TYPE, 1fr),
  table.header([*Name*], [*Type*], [*Description*]),
  [#mono("identity")], link(<t-string>)[#"string"], "",
  [#mono("option")], link(<t-u32>)[#"u32"], "",
  [#mono("enable​Formatting")], link(<t-bool>)[#"bool"], "",
)
#facts[Named by #link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"].]

#section("Enumerations", "enumerations")

#summary(
  (link(<t-sen-kernel-log-LogLevel>)[#"Log​Level"], "Severity/importance of a log message.", <t-sen-kernel-log-LogLevel>),
)

==== #"LogLevel" #chip("enumerations", "enumeration") <t-sen-kernel-log-LogLevel>
#prose("Severity/importance of a log message.")
#facts[Held as #mono("u8").]
#enum-columns(3,
  ("warn", [0]),
  ("err", [1]),
  ("critical", [2]),
  ("trace", [3]),
  ("debug", [4]),
  ("info", [5]),
  ("off", [6]),
)
#facts[Named by #link(<t-sen-components-logmaster-LogMaster>)[#"sen.​components.​logmaster.​Log​Master"], #link(<t-sen-components-logmaster-Logger>)[#"sen.​components.​logmaster.​Logger"], #link(<t-sen-kernel-log-Config>)[#"Config"], #link(<t-sen-kernel-log-LoggerConfig>)[#"Logger​Config"], #link(<t-sen-kernel-log-SinkConfig>)[#"Sink​Config"].]

#section("Variant records", "variants")

#summary(
  (link(<t-sen-kernel-log-SinkType>)[#"Sink​Type"], "The type-specific configuration of the used spdlog Sink.", <t-sen-kernel-log-SinkType>),
)

==== #"SinkType" #chip("variants", "variant") <t-sen-kernel-log-SinkType>
#prose("The type-specific configuration of the used spdlog Sink.")
#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Description*]),
  link(<t-sen-kernel-log-ColorStdout>)[#"Color​Stdout"], "",
  link(<t-sen-kernel-log-ColorStderr>)[#"Color​Stderr"], "",
  link(<t-sen-kernel-log-Stdout>)[#"Stdout"], "",
  link(<t-sen-kernel-log-Stderr>)[#"Stderr"], "",
  link(<t-sen-kernel-log-BasicFile>)[#"Basic​File"], "",
  link(<t-sen-kernel-log-RotatingFile>)[#"Rotating​File"], "",
  link(<t-sen-kernel-log-Null>)[#"Null"], "",
  link(<t-sen-kernel-log-Syslog>)[#"Syslog"], "",
)
#facts[Named by #link(<t-sen-kernel-log-SinkConfig>)[#"Sink​Config"].]

#section("Arrays", "sequences")

#summary(
  (link(<t-sen-kernel-log-LoggerConfigList>)[#"Logger​Config​List"], "", <t-sen-kernel-log-LoggerConfigList>),
  (link(<t-sen-kernel-log-SinkConfigList>)[#"Sink​Config​List"], "", <t-sen-kernel-log-SinkConfigList>),
  (link(<t-sen-kernel-log-SinkNameList>)[#"Sink​Name​List"], "", <t-sen-kernel-log-SinkNameList>),
)

==== #"LoggerConfigList" #chip("sequences", "sequence") <t-sen-kernel-log-LoggerConfigList>
#declared[#kw[sequence]\<#link(<t-sen-kernel-log-LoggerConfig>)[#"Logger​Config"]\> #"Logger​Config​List"#";"]
#facts[Named by #link(<t-sen-kernel-log-Config>)[#"Config"].]

==== #"SinkConfigList" #chip("sequences", "sequence") <t-sen-kernel-log-SinkConfigList>
#declared[#kw[sequence]\<#link(<t-sen-kernel-log-SinkConfig>)[#"Sink​Config"]\> #"Sink​Config​List"#";"]
#facts[Named by #link(<t-sen-kernel-log-Config>)[#"Config"].]

==== #"SinkNameList" #chip("sequences", "sequence") <t-sen-kernel-log-SinkNameList>
#declared[#kw[sequence]\<#link(<t-string>)[#"string"]\> #"Sink​Name​List"#";"]
#facts[Named by #link(<t-sen-kernel-log-LoggerConfig>)[#"Logger​Config"].]

= Built-in types

#prose[The types the language provides. Every one is written the same way on the wire wherever it appears.]

#sen-table(
  columns: (COL-NAME, 1fr),
  table.header([*Type*], [*Meaning*]),
  [#mono("bool") <t-bool>], "classic boolean",
  [#mono("f32") <t-f32>], "32 bit floating point number",
  [#mono("f64") <t-f64>], "64 bit floating point number",
  [#mono("i32") <t-i32>], "32 bit signed integer",
  [#mono("sen.Duration") <t-sen-Duration>], "a time duration",
  [#mono("sen.TimeStamp") <t-sen-TimeStamp>], "a point in time",
  [#mono("string") <t-string>], "unbounded string",
  [#mono("u16") <t-u16>], "16 bit unsigned integer",
  [#mono("u32") <t-u32>], "32 bit unsigned integer",
  [#mono("u64") <t-u64>], "64 bit unsigned integer",
  [#mono("u8") <t-u8>], "8 bit unsigned integer",
)

= Index

#index-of(
  (link(<t-sen-components-ether-BusConfig>)[#"components.​ether.​Bus​Config"], <t-sen-components-ether-BusConfig>),
  (link(<t-sen-components-ether-BusJoined>)[#"components.​ether.​Bus​Joined"], <t-sen-components-ether-BusJoined>),
  (link(<t-sen-components-ether-BusLeft>)[#"components.​ether.​Bus​Left"], <t-sen-components-ether-BusLeft>),
  (link(<t-sen-components-ether-ByteRange>)[#"components.​ether.​Byte​Range"], <t-sen-components-ether-ByteRange>),
  (link(<t-sen-components-ether-Configuration>)[#"components.​ether.​Configuration"], <t-sen-components-ether-Configuration>),
  (link(<t-sen-components-ether-ControlMessage>)[#"components.​ether.​Control​Message"], <t-sen-components-ether-ControlMessage>),
  (link(<t-sen-components-ether-DiscoveryConfig>)[#"components.​ether.​Discovery​Config"], <t-sen-components-ether-DiscoveryConfig>),
  (link(<t-sen-components-ether-DiscoveryHubAddress>)[#"components.​ether.​Discovery​Hub​Address"], <t-sen-components-ether-DiscoveryHubAddress>),
  (link(<t-sen-components-ether-Endpoint>)[#"components.​ether.​Endpoint"], <t-sen-components-ether-Endpoint>),
  (link(<t-sen-components-ether-EnpointList>)[#"components.​ether.​Enpoint​List"], <t-sen-components-ether-EnpointList>),
  (link(<t-sen-components-ether-Ephemeral>)[#"components.​ether.​Ephemeral"], <t-sen-components-ether-Ephemeral>),
  (link(<t-sen-components-ether-Hello>)[#"components.​ether.​Hello"], <t-sen-components-ether-Hello>),
  (link(<t-sen-components-ether-MaybeDeviceName>)[#"components.​ether.​Maybe​Device​Name"], <t-sen-components-ether-MaybeDeviceName>),
  (link(<t-sen-components-ether-MaybeDiscoveryHubPort>)[#"components.​ether.​Maybe​Discovery​Hub​Port"], <t-sen-components-ether-MaybeDiscoveryHubPort>),
  (link(<t-sen-components-ether-MaybePortConfig>)[#"components.​ether.​Maybe​Port​Config"], <t-sen-components-ether-MaybePortConfig>),
  (link(<t-sen-components-ether-MulticastAddressExclusions>)[#"components.​ether.​Multicast​Address​Exclusions"], <t-sen-components-ether-MulticastAddressExclusions>),
  (link(<t-sen-components-ether-MulticastAddressRange>)[#"components.​ether.​Multicast​Address​Range"], <t-sen-components-ether-MulticastAddressRange>),
  (link(<t-sen-components-ether-MulticastDiscovery>)[#"components.​ether.​Multicast​Discovery"], <t-sen-components-ether-MulticastDiscovery>),
  (link(<t-sen-components-ether-MulticastRange>)[#"components.​ether.​Multicast​Range"], <t-sen-components-ether-MulticastRange>),
  (link(<t-sen-components-ether-PinnedPort>)[#"components.​ether.​Pinned​Port"], <t-sen-components-ether-PinnedPort>),
  (link(<t-sen-components-ether-PortBinding>)[#"components.​ether.​Port​Binding"], <t-sen-components-ether-PortBinding>),
  (link(<t-sen-components-ether-PortConfig>)[#"components.​ether.​Port​Config"], <t-sen-components-ether-PortConfig>),
  (link(<t-sen-components-ether-PortExclusions>)[#"components.​ether.​Port​Exclusions"], <t-sen-components-ether-PortExclusions>),
  (link(<t-sen-components-ether-PortRange>)[#"components.​ether.​Port​Range"], <t-sen-components-ether-PortRange>),
  (link(<t-sen-components-ether-ProbePortRange>)[#"components.​ether.​Probe​Port​Range"], <t-sen-components-ether-ProbePortRange>),
  (link(<t-sen-components-ether-ProtocolVersion>)[#"components.​ether.​Protocol​Version"], <t-sen-components-ether-ProtocolVersion>),
  (link(<t-sen-components-ether-QueueConfig>)[#"components.​ether.​Queue​Config"], <t-sen-components-ether-QueueConfig>),
  (link(<t-sen-components-ether-QueueEvictionPolicy>)[#"components.​ether.​Queue​Eviction​Policy"], <t-sen-components-ether-QueueEvictionPolicy>),
  (link(<t-sen-components-ether-Ready>)[#"components.​ether.​Ready"], <t-sen-components-ether-Ready>),
  (link(<t-sen-components-ether-SessionPresenceBeam>)[#"components.​ether.​Session​Presence​Beam"], <t-sen-components-ether-SessionPresenceBeam>),
  (link(<t-sen-components-ether-TcpDiscovery>)[#"components.​ether.​Tcp​Discovery"], <t-sen-components-ether-TcpDiscovery>),
  (link(<t-sen-components-explorer-Configuration>)[#"components.​explorer.​Configuration"], <t-sen-components-explorer-Configuration>),
  (link(<t-sen-components-influx-Configuration>)[#"components.​influx.​Configuration"], <t-sen-components-influx-Configuration>),
  (link(<t-sen-components-influx-Protocol>)[#"components.​influx.​Protocol"], <t-sen-components-influx-Protocol>),
  (link(<t-sen-components-influx-SelectionList>)[#"components.​influx.​Selection​List"], <t-sen-components-influx-SelectionList>),
  (link(<t-sen-components-jsonrpc-AddedObjectEntry>)[#"components.​jsonrpc.​Added​Object​Entry"], <t-sen-components-jsonrpc-AddedObjectEntry>),
  (link(<t-sen-components-jsonrpc-AddedObjectList>)[#"components.​jsonrpc.​Added​Object​List"], <t-sen-components-jsonrpc-AddedObjectList>),
  (link(<t-sen-components-jsonrpc-Configuration>)[#"components.​jsonrpc.​Configuration"], <t-sen-components-jsonrpc-Configuration>),
  (link(<t-sen-components-jsonrpc-ConnectionLimits>)[#"components.​jsonrpc.​Connection​Limits"], <t-sen-components-jsonrpc-ConnectionLimits>),
  (link(<t-sen-components-jsonrpc-Identity>)[#"components.​jsonrpc.​Identity"], <t-sen-components-jsonrpc-Identity>),
  (link(<t-sen-components-jsonrpc-JsonRpcServer>)[#"components.​jsonrpc.​Json​Rpc​Server"], <t-sen-components-jsonrpc-JsonRpcServer>),
  (link(<t-sen-components-jsonrpc-MaybeConnectionLimits>)[#"components.​jsonrpc.​Maybe​Connection​Limits"], <t-sen-components-jsonrpc-MaybeConnectionLimits>),
  (link(<t-sen-components-jsonrpc-MaybeControlBusName>)[#"components.​jsonrpc.​Maybe​Control​Bus​Name"], <t-sen-components-jsonrpc-MaybeControlBusName>),
  (link(<t-sen-components-jsonrpc-MaybeFlag>)[#"components.​jsonrpc.​Maybe​Flag"], <t-sen-components-jsonrpc-MaybeFlag>),
  (link(<t-sen-components-jsonrpc-MaybeHighBackpressureBytes>)[#"components.​jsonrpc.​Maybe​High​Backpressure​Bytes"], <t-sen-components-jsonrpc-MaybeHighBackpressureBytes>),
  (link(<t-sen-components-jsonrpc-MaybeIdleTimeoutSeconds>)[#"components.​jsonrpc.​Maybe​Idle​Timeout​Seconds"], <t-sen-components-jsonrpc-MaybeIdleTimeoutSeconds>),
  (link(<t-sen-components-jsonrpc-MaybeMaxBackpressureBytes>)[#"components.​jsonrpc.​Maybe​Max​Backpressure​Bytes"], <t-sen-components-jsonrpc-MaybeMaxBackpressureBytes>),
  (link(<t-sen-components-jsonrpc-MaybeMaxPayloadBytes>)[#"components.​jsonrpc.​Maybe​Max​Payload​Bytes"], <t-sen-components-jsonrpc-MaybeMaxPayloadBytes>),
  (link(<t-sen-components-jsonrpc-MaybeMemberSelector>)[#"components.​jsonrpc.​Maybe​Member​Selector"], <t-sen-components-jsonrpc-MaybeMemberSelector>),
  (link(<t-sen-components-jsonrpc-MaybeRateHz>)[#"components.​jsonrpc.​Maybe​Rate​Hz"], <t-sen-components-jsonrpc-MaybeRateHz>),
  (link(<t-sen-components-jsonrpc-MaybeStringList>)[#"components.​jsonrpc.​Maybe​String​List"], <t-sen-components-jsonrpc-MaybeStringList>),
  (link(<t-sen-components-jsonrpc-MaybeSubscribeBlock>)[#"components.​jsonrpc.​Maybe​Subscribe​Block"], <t-sen-components-jsonrpc-MaybeSubscribeBlock>),
  (link(<t-sen-components-jsonrpc-MaybeTlsConfig>)[#"components.​jsonrpc.​Maybe​Tls​Config"], <t-sen-components-jsonrpc-MaybeTlsConfig>),
  (link(<t-sen-components-jsonrpc-MaybeUpdateFreqHz>)[#"components.​jsonrpc.​Maybe​Update​Freq​Hz"], <t-sen-components-jsonrpc-MaybeUpdateFreqHz>),
  (link(<t-sen-components-jsonrpc-MemberSelector>)[#"components.​jsonrpc.​Member​Selector"], <t-sen-components-jsonrpc-MemberSelector>),
  (link(<t-sen-components-jsonrpc-NamedSelection>)[#"components.​jsonrpc.​Named​Selection"], <t-sen-components-jsonrpc-NamedSelection>),
  (link(<t-sen-components-jsonrpc-ObjectInfo>)[#"components.​jsonrpc.​Object​Info"], <t-sen-components-jsonrpc-ObjectInfo>),
  (link(<t-sen-components-jsonrpc-ObjectInfos>)[#"components.​jsonrpc.​Object​Infos"], <t-sen-components-jsonrpc-ObjectInfos>),
  (link(<t-sen-components-jsonrpc-ObjectState>)[#"components.​jsonrpc.​Object​State"], <t-sen-components-jsonrpc-ObjectState>),
  (link(<t-sen-components-jsonrpc-ObjectStateList>)[#"components.​jsonrpc.​Object​State​List"], <t-sen-components-jsonrpc-ObjectStateList>),
  (link(<t-sen-components-jsonrpc-PropertyError>)[#"components.​jsonrpc.​Property​Error"], <t-sen-components-jsonrpc-PropertyError>),
  (link(<t-sen-components-jsonrpc-PropertyErrorList>)[#"components.​jsonrpc.​Property​Error​List"], <t-sen-components-jsonrpc-PropertyErrorList>),
  (link(<t-sen-components-jsonrpc-PropertyValueList>)[#"components.​jsonrpc.​Property​Value​List"], <t-sen-components-jsonrpc-PropertyValueList>),
  (link(<t-sen-components-jsonrpc-PropertyValuePair>)[#"components.​jsonrpc.​Property​Value​Pair"], <t-sen-components-jsonrpc-PropertyValuePair>),
  (link(<t-sen-components-jsonrpc-RegisteredBundleInfo>)[#"components.​jsonrpc.​Registered​Bundle​Info"], <t-sen-components-jsonrpc-RegisteredBundleInfo>),
  (link(<t-sen-components-jsonrpc-RegisteredBundleInfoList>)[#"components.​jsonrpc.​Registered​Bundle​Info​List"], <t-sen-components-jsonrpc-RegisteredBundleInfoList>),
  (link(<t-sen-components-jsonrpc-SessionInfo>)[#"components.​jsonrpc.​Session​Info"], <t-sen-components-jsonrpc-SessionInfo>),
  (link(<t-sen-components-jsonrpc-SessionInfoList>)[#"components.​jsonrpc.​Session​Info​List"], <t-sen-components-jsonrpc-SessionInfoList>),
  (link(<t-sen-components-jsonrpc-StaticBundle>)[#"components.​jsonrpc.​Static​Bundle"], <t-sen-components-jsonrpc-StaticBundle>),
  (link(<t-sen-components-jsonrpc-StaticFile>)[#"components.​jsonrpc.​Static​File"], <t-sen-components-jsonrpc-StaticFile>),
  (link(<t-sen-components-jsonrpc-StaticFileList>)[#"components.​jsonrpc.​Static​File​List"], <t-sen-components-jsonrpc-StaticFileList>),
  (link(<t-sen-components-jsonrpc-StaticFileServer>)[#"components.​jsonrpc.​Static​File​Server"], <t-sen-components-jsonrpc-StaticFileServer>),
  (link(<t-sen-components-jsonrpc-SubscribeBlock>)[#"components.​jsonrpc.​Subscribe​Block"], <t-sen-components-jsonrpc-SubscribeBlock>),
  (link(<t-sen-components-jsonrpc-TlsConfig>)[#"components.​jsonrpc.​Tls​Config"], <t-sen-components-jsonrpc-TlsConfig>),
  (link(<t-sen-components-jsonrpc-TypeLookupResult>)[#"components.​jsonrpc.​Type​Lookup​Result"], <t-sen-components-jsonrpc-TypeLookupResult>),
  (link(<t-sen-components-jsonrpc-WildcardSelection>)[#"components.​jsonrpc.​Wildcard​Selection"], <t-sen-components-jsonrpc-WildcardSelection>),
  (link(<t-sen-components-logmaster-Config>)[#"components.​logmaster.​Config"], <t-sen-components-logmaster-Config>),
  (link(<t-sen-components-logmaster-LogMaster>)[#"components.​logmaster.​Log​Master"], <t-sen-components-logmaster-LogMaster>),
  (link(<t-sen-components-logmaster-Logger>)[#"components.​logmaster.​Logger"], <t-sen-components-logmaster-Logger>),
  (link(<t-sen-components-py-Configuration>)[#"components.​py.​Configuration"], <t-sen-components-py-Configuration>),
  (link(<t-sen-components-py-PythonInterpreter>)[#"components.​py.​Python​Interpreter"], <t-sen-components-py-PythonInterpreter>),
  (link(<t-sen-components-py-StringList>)[#"components.​py.​String​List"], <t-sen-components-py-StringList>),
  (link(<t-sen-components-recorder-Configuration>)[#"components.​recorder.​Configuration"], <t-sen-components-recorder-Configuration>),
  (link(<t-sen-components-recorder-Recorder>)[#"components.​recorder.​Recorder"], <t-sen-components-recorder-Recorder>),
  (link(<t-sen-components-recorder-RecorderState>)[#"components.​recorder.​Recorder​State"], <t-sen-components-recorder-RecorderState>),
  (link(<t-sen-components-recorder-RecordingSettings>)[#"components.​recorder.​Recording​Settings"], <t-sen-components-recorder-RecordingSettings>),
  (link(<t-sen-components-recorder-RecordingSettingsList>)[#"components.​recorder.​Recording​Settings​List"], <t-sen-components-recorder-RecordingSettingsList>),
  (link(<t-sen-components-recorder-SelectionList>)[#"components.​recorder.​Selection​List"], <t-sen-components-recorder-SelectionList>),
  (link(<t-sen-components-replayer-Configuration>)[#"components.​replayer.​Configuration"], <t-sen-components-replayer-Configuration>),
  (link(<t-sen-components-replayer-Replay>)[#"components.​replayer.​Replay"], <t-sen-components-replayer-Replay>),
  (link(<t-sen-components-replayer-ReplayStatus>)[#"components.​replayer.​Replay​Status"], <t-sen-components-replayer-ReplayStatus>),
  (link(<t-sen-components-replayer-Replayer>)[#"components.​replayer.​Replayer"], <t-sen-components-replayer-Replayer>),
  (link(<t-sen-components-rest-Argument>)[#"components.​rest.​Argument"], <t-sen-components-rest-Argument>),
  (link(<t-sen-components-rest-Arguments>)[#"components.​rest.​Arguments"], <t-sen-components-rest-Arguments>),
  (link(<t-sen-components-rest-Bus>)[#"components.​rest.​Bus"], <t-sen-components-rest-Bus>),
  (link(<t-sen-components-rest-BusNames>)[#"components.​rest.​Bus​Names"], <t-sen-components-rest-BusNames>),
  (link(<t-sen-components-rest-BusSummary>)[#"components.​rest.​Bus​Summary"], <t-sen-components-rest-BusSummary>),
  (link(<t-sen-components-rest-Buses>)[#"components.​rest.​Buses"], <t-sen-components-rest-Buses>),
  (link(<t-sen-components-rest-ClientAuth>)[#"components.​rest.​Client​Auth"], <t-sen-components-rest-ClientAuth>),
  (link(<t-sen-components-rest-Configuration>)[#"components.​rest.​Configuration"], <t-sen-components-rest-Configuration>),
  (link(<t-sen-components-rest-Error>)[#"components.​rest.​Error"], <t-sen-components-rest-Error>),
  (link(<t-sen-components-rest-Events>)[#"components.​rest.​Events"], <t-sen-components-rest-Events>),
  (link(<t-sen-components-rest-HttpMethod>)[#"components.​rest.​Http​Method"], <t-sen-components-rest-HttpMethod>),
  (link(<t-sen-components-rest-Interest>)[#"components.​rest.​Interest"], <t-sen-components-rest-Interest>),
  (link(<t-sen-components-rest-InterestSummary>)[#"components.​rest.​Interest​Summary"], <t-sen-components-rest-InterestSummary>),
  (link(<t-sen-components-rest-InterestsSummary>)[#"components.​rest.​Interests​Summary"], <t-sen-components-rest-InterestsSummary>),
  (link(<t-sen-components-rest-InvokeStatus>)[#"components.​rest.​Invoke​Status"], <t-sen-components-rest-InvokeStatus>),
  (link(<t-sen-components-rest-Link>)[#"components.​rest.​Link"], <t-sen-components-rest-Link>),
  (link(<t-sen-components-rest-Links>)[#"components.​rest.​Links"], <t-sen-components-rest-Links>),
  (link(<t-sen-components-rest-Notification>)[#"components.​rest.​Notification"], <t-sen-components-rest-Notification>),
  (link(<t-sen-components-rest-NotificationType>)[#"components.​rest.​Notification​Type"], <t-sen-components-rest-NotificationType>),
  (link(<t-sen-components-rest-Object>)[#"components.​rest.​Object"], <t-sen-components-rest-Object>),
  (link(<t-sen-components-rest-ObjectEvent>)[#"components.​rest.​Object​Event"], <t-sen-components-rest-ObjectEvent>),
  (link(<t-sen-components-rest-ObjectMethod>)[#"components.​rest.​Object​Method"], <t-sen-components-rest-ObjectMethod>),
  (link(<t-sen-components-rest-ObjectSummary>)[#"components.​rest.​Object​Summary"], <t-sen-components-rest-ObjectSummary>),
  (link(<t-sen-components-rest-ObjectsSummary>)[#"components.​rest.​Objects​Summary"], <t-sen-components-rest-ObjectsSummary>),
  (link(<t-sen-components-rest-Properties>)[#"components.​rest.​Properties"], <t-sen-components-rest-Properties>),
  (link(<t-sen-components-rest-RelType>)[#"components.​rest.​Rel​Type"], <t-sen-components-rest-RelType>),
  (link(<t-sen-components-rest-Session>)[#"components.​rest.​Session"], <t-sen-components-rest-Session>),
  (link(<t-sen-components-rest-SessionSummary>)[#"components.​rest.​Session​Summary"], <t-sen-components-rest-SessionSummary>),
  (link(<t-sen-components-rest-Sessions>)[#"components.​rest.​Sessions"], <t-sen-components-rest-Sessions>),
  (link(<t-sen-components-rest-Status>)[#"components.​rest.​Status"], <t-sen-components-rest-Status>),
  (link(<t-sen-components-rest-SubscriptionOptions>)[#"components.​rest.​Subscription​Options"], <t-sen-components-rest-SubscriptionOptions>),
  (link(<t-sen-components-rest-Subscriptions>)[#"components.​rest.​Subscriptions"], <t-sen-components-rest-Subscriptions>),
  (link(<t-sen-components-rest-Success>)[#"components.​rest.​Success"], <t-sen-components-rest-Success>),
  (link(<t-sen-components-rest-ThreadPoolSize>)[#"components.​rest.​Thread​Pool​Size"], <t-sen-components-rest-ThreadPoolSize>),
  (link(<t-sen-components-rest-UpdateFrequency>)[#"components.​rest.​Update​Frequency"], <t-sen-components-rest-UpdateFrequency>),
  (link(<t-sen-components-rest-UpdateTime>)[#"components.​rest.​Update​Time"], <t-sen-components-rest-UpdateTime>),
  (link(<t-sen-components-rest-Version>)[#"components.​rest.​Version"], <t-sen-components-rest-Version>),
  (link(<t-sen-components-shell-BufferStyle>)[#"components.​shell.​Buffer​Style"], <t-sen-components-shell-BufferStyle>),
  (link(<t-sen-components-shell-CPrint>)[#"components.​shell.​CPrint"], <t-sen-components-shell-CPrint>),
  (link(<t-sen-components-shell-ClearCurrentLine>)[#"components.​shell.​Clear​Current​Line"], <t-sen-components-shell-ClearCurrentLine>),
  (link(<t-sen-components-shell-ClearRemainingCurrentLine>)[#"components.​shell.​Clear​Remaining​Current​Line"], <t-sen-components-shell-ClearRemainingCurrentLine>),
  (link(<t-sen-components-shell-ClearScreen>)[#"components.​shell.​Clear​Screen"], <t-sen-components-shell-ClearScreen>),
  (link(<t-sen-components-shell-Configuration>)[#"components.​shell.​Configuration"], <t-sen-components-shell-Configuration>),
  (link(<t-sen-components-shell-HideCursor>)[#"components.​shell.​Hide​Cursor"], <t-sen-components-shell-HideCursor>),
  (link(<t-sen-components-shell-ListStyle>)[#"components.​shell.​List​Style"], <t-sen-components-shell-ListStyle>),
  (link(<t-sen-components-shell-MoveCursorAllLeft>)[#"components.​shell.​Move​Cursor​All​Left"], <t-sen-components-shell-MoveCursorAllLeft>),
  (link(<t-sen-components-shell-MoveCursorDown>)[#"components.​shell.​Move​Cursor​Down"], <t-sen-components-shell-MoveCursorDown>),
  (link(<t-sen-components-shell-MoveCursorLeft>)[#"components.​shell.​Move​Cursor​Left"], <t-sen-components-shell-MoveCursorLeft>),
  (link(<t-sen-components-shell-MoveCursorRight>)[#"components.​shell.​Move​Cursor​Right"], <t-sen-components-shell-MoveCursorRight>),
  (link(<t-sen-components-shell-MoveCursorUp>)[#"components.​shell.​Move​Cursor​Up"], <t-sen-components-shell-MoveCursorUp>),
  (link(<t-sen-components-shell-NewLine>)[#"components.​shell.​New​Line"], <t-sen-components-shell-NewLine>),
  (link(<t-sen-components-shell-Print>)[#"components.​shell.​Print"], <t-sen-components-shell-Print>),
  (link(<t-sen-components-shell-Query>)[#"components.​shell.​Query"], <t-sen-components-shell-Query>),
  (link(<t-sen-components-shell-QueryList>)[#"components.​shell.​Query​List"], <t-sen-components-shell-QueryList>),
  (link(<t-sen-components-shell-RestoreCursorPosition>)[#"components.​shell.​Restore​Cursor​Position"], <t-sen-components-shell-RestoreCursorPosition>),
  (link(<t-sen-components-shell-SaveCursorPosition>)[#"components.​shell.​Save​Cursor​Position"], <t-sen-components-shell-SaveCursorPosition>),
  (link(<t-sen-components-shell-SetBgColor>)[#"components.​shell.​Set​Bg​Color"], <t-sen-components-shell-SetBgColor>),
  (link(<t-sen-components-shell-SetFgColor>)[#"components.​shell.​Set​Fg​Color"], <t-sen-components-shell-SetFgColor>),
  (link(<t-sen-components-shell-SetWindowTitle>)[#"components.​shell.​Set​Window​Title"], <t-sen-components-shell-SetWindowTitle>),
  (link(<t-sen-components-shell-Shell>)[#"components.​shell.​Shell"], <t-sen-components-shell-Shell>),
  (link(<t-sen-components-shell-ShowCursor>)[#"components.​shell.​Show​Cursor"], <t-sen-components-shell-ShowCursor>),
  (link(<t-sen-components-shell-SourcesList>)[#"components.​shell.​Sources​List"], <t-sen-components-shell-SourcesList>),
  (link(<t-sen-components-shell-TerminalCmd>)[#"components.​shell.​Terminal​Cmd"], <t-sen-components-shell-TerminalCmd>),
  (link(<t-sen-components-shell-TimeStyle>)[#"components.​shell.​Time​Style"], <t-sen-components-shell-TimeStyle>),
  (link(<t-sen-db-OutSettings>)[#"db.​Out​Settings"], <t-sen-db-OutSettings>),
  (link(<t-sen-db-OutStats>)[#"db.​Out​Stats"], <t-sen-db-OutStats>),
  (link(<t-sen-db-Summary>)[#"db.​Summary"], <t-sen-db-Summary>),
  (link(<t-sen-kernel-AliasTypeSpec>)[#"kernel.​Alias​Type​Spec"], <t-sen-kernel-AliasTypeSpec>),
  (link(<t-sen-kernel-AliasTypeSpecV4>)[#"kernel.​Alias​Type​Spec​V4"], <t-sen-kernel-AliasTypeSpecV4>),
  (link(<t-sen-kernel-AliasTypeSpecV5>)[#"kernel.​Alias​Type​Spec​V5"], <t-sen-kernel-AliasTypeSpecV5>),
  (link(<t-sen-kernel-ArgSpec>)[#"kernel.​Arg​Spec"], <t-sen-kernel-ArgSpec>),
  (link(<t-sen-kernel-ArgSpecList>)[#"kernel.​Arg​Spec​List"], <t-sen-kernel-ArgSpecList>),
  (link(<t-sen-kernel-ArgSpecListV4>)[#"kernel.​Arg​Spec​List​V4"], <t-sen-kernel-ArgSpecListV4>),
  (link(<t-sen-kernel-ArgSpecListV5>)[#"kernel.​Arg​Spec​List​V5"], <t-sen-kernel-ArgSpecListV5>),
  (link(<t-sen-kernel-ArgSpecV4>)[#"kernel.​Arg​Spec​V4"], <t-sen-kernel-ArgSpecV4>),
  (link(<t-sen-kernel-ArgSpecV5>)[#"kernel.​Arg​Spec​V5"], <t-sen-kernel-ArgSpecV5>),
  (link(<t-sen-kernel-BasicType>)[#"kernel.​Basic​Type"], <t-sen-kernel-BasicType>),
  (link(<t-sen-kernel-Buffer>)[#"kernel.​Buffer"], <t-sen-kernel-Buffer>),
  (link(<t-sen-kernel-BuildInfo>)[#"kernel.​Build​Info"], <t-sen-kernel-BuildInfo>),
  (link(<t-sen-kernel-BuiltComponentList>)[#"kernel.​Built​Component​List"], <t-sen-kernel-BuiltComponentList>),
  (link(<t-sen-kernel-BuiltComponentParams>)[#"kernel.​Built​Component​Params"], <t-sen-kernel-BuiltComponentParams>),
  (link(<t-sen-kernel-BuiltInType>)[#"kernel.​Built​In​Type"], <t-sen-kernel-BuiltInType>),
  (link(<t-sen-kernel-BusAddress>)[#"kernel.​Bus​Address"], <t-sen-kernel-BusAddress>),
  (link(<t-sen-kernel-ClassTypeSpec>)[#"kernel.​Class​Type​Spec"], <t-sen-kernel-ClassTypeSpec>),
  (link(<t-sen-kernel-ClassTypeSpecV4>)[#"kernel.​Class​Type​Spec​V4"], <t-sen-kernel-ClassTypeSpecV4>),
  (link(<t-sen-kernel-ClassTypeSpecV5>)[#"kernel.​Class​Type​Spec​V5"], <t-sen-kernel-ClassTypeSpecV5>),
  (link(<t-sen-kernel-CompatibilityMode>)[#"kernel.​Compatibility​Mode"], <t-sen-kernel-CompatibilityMode>),
  (link(<t-sen-kernel-ComponentConfig>)[#"kernel.​Component​Config"], <t-sen-kernel-ComponentConfig>),
  (link(<t-sen-kernel-ComponentInfo>)[#"kernel.​Component​Info"], <t-sen-kernel-ComponentInfo>),
  (link(<t-sen-kernel-ComponentState>)[#"kernel.​Component​State"], <t-sen-kernel-ComponentState>),
  (link(<t-sen-kernel-CpuArch>)[#"kernel.​Cpu​Arch"], <t-sen-kernel-CpuArch>),
  (link(<t-sen-kernel-CustomTypeData>)[#"kernel.​Custom​Type​Data"], <t-sen-kernel-CustomTypeData>),
  (link(<t-sen-kernel-CustomTypeDataV4>)[#"kernel.​Custom​Type​Data​V4"], <t-sen-kernel-CustomTypeDataV4>),
  (link(<t-sen-kernel-CustomTypeDataV5>)[#"kernel.​Custom​Type​Data​V5"], <t-sen-kernel-CustomTypeDataV5>),
  (link(<t-sen-kernel-CustomTypeSpec>)[#"kernel.​Custom​Type​Spec"], <t-sen-kernel-CustomTypeSpec>),
  (link(<t-sen-kernel-CustomTypeSpecList>)[#"kernel.​Custom​Type​Spec​List"], <t-sen-kernel-CustomTypeSpecList>),
  (link(<t-sen-kernel-CustomTypeSpecListV4>)[#"kernel.​Custom​Type​Spec​List​V4"], <t-sen-kernel-CustomTypeSpecListV4>),
  (link(<t-sen-kernel-CustomTypeSpecListV5>)[#"kernel.​Custom​Type​Spec​List​V5"], <t-sen-kernel-CustomTypeSpecListV5>),
  (link(<t-sen-kernel-CustomTypeSpecV4>)[#"kernel.​Custom​Type​Spec​V4"], <t-sen-kernel-CustomTypeSpecV4>),
  (link(<t-sen-kernel-CustomTypeSpecV5>)[#"kernel.​Custom​Type​Spec​V5"], <t-sen-kernel-CustomTypeSpecV5>),
  (link(<t-sen-kernel-EnumTypeSpec>)[#"kernel.​Enum​Type​Spec"], <t-sen-kernel-EnumTypeSpec>),
  (link(<t-sen-kernel-EnumTypeSpecV4>)[#"kernel.​Enum​Type​Spec​V4"], <t-sen-kernel-EnumTypeSpecV4>),
  (link(<t-sen-kernel-EnumTypeSpecV5>)[#"kernel.​Enum​Type​Spec​V5"], <t-sen-kernel-EnumTypeSpecV5>),
  (link(<t-sen-kernel-EnumeratorSpec>)[#"kernel.​Enumerator​Spec"], <t-sen-kernel-EnumeratorSpec>),
  (link(<t-sen-kernel-EnumeratorSpecList>)[#"kernel.​Enumerator​Spec​List"], <t-sen-kernel-EnumeratorSpecList>),
  (link(<t-sen-kernel-EnumeratorSpecListV4>)[#"kernel.​Enumerator​Spec​List​V4"], <t-sen-kernel-EnumeratorSpecListV4>),
  (link(<t-sen-kernel-EnumeratorSpecListV5>)[#"kernel.​Enumerator​Spec​List​V5"], <t-sen-kernel-EnumeratorSpecListV5>),
  (link(<t-sen-kernel-EnumeratorSpecV4>)[#"kernel.​Enumerator​Spec​V4"], <t-sen-kernel-EnumeratorSpecV4>),
  (link(<t-sen-kernel-EnumeratorSpecV5>)[#"kernel.​Enumerator​Spec​V5"], <t-sen-kernel-EnumeratorSpecV5>),
  (link(<t-sen-kernel-EnvVar>)[#"kernel.​Env​Var"], <t-sen-kernel-EnvVar>),
  (link(<t-sen-kernel-EnvironmentVarList>)[#"kernel.​Environment​Var​List"], <t-sen-kernel-EnvironmentVarList>),
  (link(<t-sen-kernel-ErrorCategory>)[#"kernel.​Error​Category"], <t-sen-kernel-ErrorCategory>),
  (link(<t-sen-kernel-ErrorData>)[#"kernel.​Error​Data"], <t-sen-kernel-ErrorData>),
  (link(<t-sen-kernel-ErrorReport>)[#"kernel.​Error​Report"], <t-sen-kernel-ErrorReport>),
  (link(<t-sen-kernel-EventSpec>)[#"kernel.​Event​Spec"], <t-sen-kernel-EventSpec>),
  (link(<t-sen-kernel-EventSpecList>)[#"kernel.​Event​Spec​List"], <t-sen-kernel-EventSpecList>),
  (link(<t-sen-kernel-EventSpecListV4>)[#"kernel.​Event​Spec​List​V4"], <t-sen-kernel-EventSpecListV4>),
  (link(<t-sen-kernel-EventSpecListV5>)[#"kernel.​Event​Spec​List​V5"], <t-sen-kernel-EventSpecListV5>),
  (link(<t-sen-kernel-EventSpecV4>)[#"kernel.​Event​Spec​V4"], <t-sen-kernel-EventSpecV4>),
  (link(<t-sen-kernel-EventSpecV5>)[#"kernel.​Event​Spec​V5"], <t-sen-kernel-EventSpecV5>),
  (link(<t-sen-kernel-ExceptionKind>)[#"kernel.​Exception​Kind"], <t-sen-kernel-ExceptionKind>),
  (link(<t-sen-kernel-ExecError>)[#"kernel.​Exec​Error"], <t-sen-kernel-ExecError>),
  (link(<t-sen-kernel-GitStatus>)[#"kernel.​Git​Status"], <t-sen-kernel-GitStatus>),
  (link(<t-sen-kernel-IntegralType>)[#"kernel.​Integral​Type"], <t-sen-kernel-IntegralType>),
  (link(<t-sen-kernel-KernelApi>)[#"kernel.​Kernel​Api"], <t-sen-kernel-KernelApi>),
  (link(<t-sen-kernel-KernelParams>)[#"kernel.​Kernel​Params"], <t-sen-kernel-KernelParams>),
  (link(<t-sen-kernel-LoadedComponentList>)[#"kernel.​Loaded​Component​List"], <t-sen-kernel-LoadedComponentList>),
  (link(<t-sen-kernel-LoadedComponentParams>)[#"kernel.​Loaded​Component​Params"], <t-sen-kernel-LoadedComponentParams>),
  (link(<t-sen-kernel-MaybeException>)[#"kernel.​Maybe​Exception"], <t-sen-kernel-MaybeException>),
  (link(<t-sen-kernel-MaybeF64>)[#"kernel.​Maybe​F64"], <t-sen-kernel-MaybeF64>),
  (link(<t-sen-kernel-MaybeNetworkFootprintMulticast>)[#"kernel.​Maybe​Network​Footprint​Multicast"], <t-sen-kernel-MaybeNetworkFootprintMulticast>),
  (link(<t-sen-kernel-MaybeNetworkFootprintPortValue>)[#"kernel.​Maybe​Network​Footprint​Port​Value"], <t-sen-kernel-MaybeNetworkFootprintPortValue>),
  (link(<t-sen-kernel-MaybeSignal>)[#"kernel.​Maybe​Signal"], <t-sen-kernel-MaybeSignal>),
  (link(<t-sen-kernel-MaybeStacktrace>)[#"kernel.​Maybe​Stacktrace"], <t-sen-kernel-MaybeStacktrace>),
  (link(<t-sen-kernel-MaybeTransportProtocol>)[#"kernel.​Maybe​Transport​Protocol"], <t-sen-kernel-MaybeTransportProtocol>),
  (link(<t-sen-kernel-MaybeU64>)[#"kernel.​Maybe​U64"], <t-sen-kernel-MaybeU64>),
  (link(<t-sen-kernel-MethodConstnessSpec>)[#"kernel.​Method​Constness​Spec"], <t-sen-kernel-MethodConstnessSpec>),
  (link(<t-sen-kernel-MethodSpec>)[#"kernel.​Method​Spec"], <t-sen-kernel-MethodSpec>),
  (link(<t-sen-kernel-MethodSpecList>)[#"kernel.​Method​Spec​List"], <t-sen-kernel-MethodSpecList>),
  (link(<t-sen-kernel-MethodSpecListV4>)[#"kernel.​Method​Spec​List​V4"], <t-sen-kernel-MethodSpecListV4>),
  (link(<t-sen-kernel-MethodSpecListV5>)[#"kernel.​Method​Spec​List​V5"], <t-sen-kernel-MethodSpecListV5>),
  (link(<t-sen-kernel-MethodSpecV4>)[#"kernel.​Method​Spec​V4"], <t-sen-kernel-MethodSpecV4>),
  (link(<t-sen-kernel-MethodSpecV5>)[#"kernel.​Method​Spec​V5"], <t-sen-kernel-MethodSpecV5>),
  (link(<t-sen-kernel-NetworkFootprint>)[#"kernel.​Network​Footprint"], <t-sen-kernel-NetworkFootprint>),
  (link(<t-sen-kernel-NetworkFootprintAddressRange>)[#"kernel.​Network​Footprint​Address​Range"], <t-sen-kernel-NetworkFootprintAddressRange>),
  (link(<t-sen-kernel-NetworkFootprintAddressRangeList>)[#"kernel.​Network​Footprint​Address​Range​List"], <t-sen-kernel-NetworkFootprintAddressRangeList>),
  (link(<t-sen-kernel-NetworkFootprintBus>)[#"kernel.​Network​Footprint​Bus"], <t-sen-kernel-NetworkFootprintBus>),
  (link(<t-sen-kernel-NetworkFootprintBusIdentity>)[#"kernel.​Network​Footprint​Bus​Identity"], <t-sen-kernel-NetworkFootprintBusIdentity>),
  (link(<t-sen-kernel-NetworkFootprintBusList>)[#"kernel.​Network​Footprint​Bus​List"], <t-sen-kernel-NetworkFootprintBusList>),
  (link(<t-sen-kernel-NetworkFootprintBusSource>)[#"kernel.​Network​Footprint​Bus​Source"], <t-sen-kernel-NetworkFootprintBusSource>),
  (link(<t-sen-kernel-NetworkFootprintCollision>)[#"kernel.​Network​Footprint​Collision"], <t-sen-kernel-NetworkFootprintCollision>),
  (link(<t-sen-kernel-NetworkFootprintMulticast>)[#"kernel.​Network​Footprint​Multicast"], <t-sen-kernel-NetworkFootprintMulticast>),
  (link(<t-sen-kernel-NetworkFootprintPort>)[#"kernel.​Network​Footprint​Port"], <t-sen-kernel-NetworkFootprintPort>),
  (link(<t-sen-kernel-NetworkFootprintPortExclusions>)[#"kernel.​Network​Footprint​Port​Exclusions"], <t-sen-kernel-NetworkFootprintPortExclusions>),
  (link(<t-sen-kernel-NetworkFootprintPortKind>)[#"kernel.​Network​Footprint​Port​Kind"], <t-sen-kernel-NetworkFootprintPortKind>),
  (link(<t-sen-kernel-NetworkFootprintPortList>)[#"kernel.​Network​Footprint​Port​List"], <t-sen-kernel-NetworkFootprintPortList>),
  (link(<t-sen-kernel-NetworkFootprintPortMode>)[#"kernel.​Network​Footprint​Port​Mode"], <t-sen-kernel-NetworkFootprintPortMode>),
  (link(<t-sen-kernel-NetworkFootprintPortRange>)[#"kernel.​Network​Footprint​Port​Range"], <t-sen-kernel-NetworkFootprintPortRange>),
  (link(<t-sen-kernel-NetworkFootprintPortRangeList>)[#"kernel.​Network​Footprint​Port​Range​List"], <t-sen-kernel-NetworkFootprintPortRangeList>),
  (link(<t-sen-kernel-NetworkFootprintPortValue>)[#"kernel.​Network​Footprint​Port​Value"], <t-sen-kernel-NetworkFootprintPortValue>),
  (link(<t-sen-kernel-NetworkFootprintSelfCollision>)[#"kernel.​Network​Footprint​Self​Collision"], <t-sen-kernel-NetworkFootprintSelfCollision>),
  (link(<t-sen-kernel-NetworkFootprintSelfCollisionList>)[#"kernel.​Network​Footprint​Self​Collision​List"], <t-sen-kernel-NetworkFootprintSelfCollisionList>),
  (link(<t-sen-kernel-NumericType>)[#"kernel.​Numeric​Type"], <t-sen-kernel-NumericType>),
  (link(<t-sen-kernel-OpFinished>)[#"kernel.​Op​Finished"], <t-sen-kernel-OpFinished>),
  (link(<t-sen-kernel-OpNotFinished>)[#"kernel.​Op​Not​Finished"], <t-sen-kernel-OpNotFinished>),
  (link(<t-sen-kernel-OpState>)[#"kernel.​Op​State"], <t-sen-kernel-OpState>),
  (link(<t-sen-kernel-OptionalTypeSpec>)[#"kernel.​Optional​Type​Spec"], <t-sen-kernel-OptionalTypeSpec>),
  (link(<t-sen-kernel-OptionalTypeSpecV4>)[#"kernel.​Optional​Type​Spec​V4"], <t-sen-kernel-OptionalTypeSpecV4>),
  (link(<t-sen-kernel-OptionalTypeSpecV5>)[#"kernel.​Optional​Type​Spec​V5"], <t-sen-kernel-OptionalTypeSpecV5>),
  (link(<t-sen-kernel-OsKind>)[#"kernel.​Os​Kind"], <t-sen-kernel-OsKind>),
  (link(<t-sen-kernel-PrecisionSleep>)[#"kernel.​Precision​Sleep"], <t-sen-kernel-PrecisionSleep>),
  (link(<t-sen-kernel-Priority>)[#"kernel.​Priority"], <t-sen-kernel-Priority>),
  (link(<t-sen-kernel-ProcessData>)[#"kernel.​Process​Data"], <t-sen-kernel-ProcessData>),
  (link(<t-sen-kernel-ProcessInfo>)[#"kernel.​Process​Info"], <t-sen-kernel-ProcessInfo>),
  (link(<t-sen-kernel-Processes>)[#"kernel.​Processes"], <t-sen-kernel-Processes>),
  (link(<t-sen-kernel-PropertyCategorySpec>)[#"kernel.​Property​Category​Spec"], <t-sen-kernel-PropertyCategorySpec>),
  (link(<t-sen-kernel-PropertyRelationSpec>)[#"kernel.​Property​Relation​Spec"], <t-sen-kernel-PropertyRelationSpec>),
  (link(<t-sen-kernel-PropertySpec>)[#"kernel.​Property​Spec"], <t-sen-kernel-PropertySpec>),
  (link(<t-sen-kernel-PropertySpecList>)[#"kernel.​Property​Spec​List"], <t-sen-kernel-PropertySpecList>),
  (link(<t-sen-kernel-PropertySpecListV4>)[#"kernel.​Property​Spec​List​V4"], <t-sen-kernel-PropertySpecListV4>),
  (link(<t-sen-kernel-PropertySpecListV5>)[#"kernel.​Property​Spec​List​V5"], <t-sen-kernel-PropertySpecListV5>),
  (link(<t-sen-kernel-PropertySpecV4>)[#"kernel.​Property​Spec​V4"], <t-sen-kernel-PropertySpecV4>),
  (link(<t-sen-kernel-PropertySpecV5>)[#"kernel.​Property​Spec​V5"], <t-sen-kernel-PropertySpecV5>),
  (link(<t-sen-kernel-QuantityTypeSpec>)[#"kernel.​Quantity​Type​Spec"], <t-sen-kernel-QuantityTypeSpec>),
  (link(<t-sen-kernel-QuantityTypeSpecV4>)[#"kernel.​Quantity​Type​Spec​V4"], <t-sen-kernel-QuantityTypeSpecV4>),
  (link(<t-sen-kernel-QuantityTypeSpecV5>)[#"kernel.​Quantity​Type​Spec​V5"], <t-sen-kernel-QuantityTypeSpecV5>),
  (link(<t-sen-kernel-QueueConfig>)[#"kernel.​Queue​Config"], <t-sen-kernel-QueueConfig>),
  (link(<t-sen-kernel-QueueEvictionPolicy>)[#"kernel.​Queue​Eviction​Policy"], <t-sen-kernel-QueueEvictionPolicy>),
  (link(<t-sen-kernel-RealType>)[#"kernel.​Real​Type"], <t-sen-kernel-RealType>),
  (link(<t-sen-kernel-RunMode>)[#"kernel.​Run​Mode"], <t-sen-kernel-RunMode>),
  (link(<t-sen-kernel-SenData>)[#"kernel.​Sen​Data"], <t-sen-kernel-SenData>),
  (link(<t-sen-kernel-SequenceTypeSpec>)[#"kernel.​Sequence​Type​Spec"], <t-sen-kernel-SequenceTypeSpec>),
  (link(<t-sen-kernel-SequenceTypeSpecV4>)[#"kernel.​Sequence​Type​Spec​V4"], <t-sen-kernel-SequenceTypeSpecV4>),
  (link(<t-sen-kernel-SequenceTypeSpecV5>)[#"kernel.​Sequence​Type​Spec​V5"], <t-sen-kernel-SequenceTypeSpecV5>),
  (link(<t-sen-kernel-SignalData>)[#"kernel.​Signal​Data"], <t-sen-kernel-SignalData>),
  (link(<t-sen-kernel-SleepPolicy>)[#"kernel.​Sleep​Policy"], <t-sen-kernel-SleepPolicy>),
  (link(<t-sen-kernel-StringList>)[#"kernel.​String​List"], <t-sen-kernel-StringList>),
  (link(<t-sen-kernel-StructTypeFieldSpec>)[#"kernel.​Struct​Type​Field​Spec"], <t-sen-kernel-StructTypeFieldSpec>),
  (link(<t-sen-kernel-StructTypeFieldSpecList>)[#"kernel.​Struct​Type​Field​Spec​List"], <t-sen-kernel-StructTypeFieldSpecList>),
  (link(<t-sen-kernel-StructTypeFieldSpecListV4>)[#"kernel.​Struct​Type​Field​Spec​List​V4"], <t-sen-kernel-StructTypeFieldSpecListV4>),
  (link(<t-sen-kernel-StructTypeFieldSpecListV5>)[#"kernel.​Struct​Type​Field​Spec​List​V5"], <t-sen-kernel-StructTypeFieldSpecListV5>),
  (link(<t-sen-kernel-StructTypeFieldSpecV4>)[#"kernel.​Struct​Type​Field​Spec​V4"], <t-sen-kernel-StructTypeFieldSpecV4>),
  (link(<t-sen-kernel-StructTypeFieldSpecV5>)[#"kernel.​Struct​Type​Field​Spec​V5"], <t-sen-kernel-StructTypeFieldSpecV5>),
  (link(<t-sen-kernel-StructTypeSpec>)[#"kernel.​Struct​Type​Spec"], <t-sen-kernel-StructTypeSpec>),
  (link(<t-sen-kernel-StructTypeSpecV4>)[#"kernel.​Struct​Type​Spec​V4"], <t-sen-kernel-StructTypeSpecV4>),
  (link(<t-sen-kernel-StructTypeSpecV5>)[#"kernel.​Struct​Type​Spec​V5"], <t-sen-kernel-StructTypeSpecV5>),
  (link(<t-sen-kernel-SystemSleep>)[#"kernel.​System​Sleep"], <t-sen-kernel-SystemSleep>),
  (link(<t-sen-kernel-ThreadCreateErr>)[#"kernel.​Thread​Create​Err"], <t-sen-kernel-ThreadCreateErr>),
  (link(<t-sen-kernel-TransportModeSpec>)[#"kernel.​Transport​Mode​Spec"], <t-sen-kernel-TransportModeSpec>),
  (link(<t-sen-kernel-U32List>)[#"kernel.​U32List"], <t-sen-kernel-U32List>),
  (link(<t-sen-kernel-UncaughtException>)[#"kernel.​Uncaught​Exception"], <t-sen-kernel-UncaughtException>),
  (link(<t-sen-kernel-UnitCat>)[#"kernel.​Unit​Cat"], <t-sen-kernel-UnitCat>),
  (link(<t-sen-kernel-UnitInfo>)[#"kernel.​Unit​Info"], <t-sen-kernel-UnitInfo>),
  (link(<t-sen-kernel-UnitList>)[#"kernel.​Unit​List"], <t-sen-kernel-UnitList>),
  (link(<t-sen-kernel-VariantTypeFieldSpec>)[#"kernel.​Variant​Type​Field​Spec"], <t-sen-kernel-VariantTypeFieldSpec>),
  (link(<t-sen-kernel-VariantTypeFieldSpecList>)[#"kernel.​Variant​Type​Field​Spec​List"], <t-sen-kernel-VariantTypeFieldSpecList>),
  (link(<t-sen-kernel-VariantTypeFieldSpecListV4>)[#"kernel.​Variant​Type​Field​Spec​List​V4"], <t-sen-kernel-VariantTypeFieldSpecListV4>),
  (link(<t-sen-kernel-VariantTypeFieldSpecListV5>)[#"kernel.​Variant​Type​Field​Spec​List​V5"], <t-sen-kernel-VariantTypeFieldSpecListV5>),
  (link(<t-sen-kernel-VariantTypeFieldSpecV4>)[#"kernel.​Variant​Type​Field​Spec​V4"], <t-sen-kernel-VariantTypeFieldSpecV4>),
  (link(<t-sen-kernel-VariantTypeFieldSpecV5>)[#"kernel.​Variant​Type​Field​Spec​V5"], <t-sen-kernel-VariantTypeFieldSpecV5>),
  (link(<t-sen-kernel-VariantTypeSpec>)[#"kernel.​Variant​Type​Spec"], <t-sen-kernel-VariantTypeSpec>),
  (link(<t-sen-kernel-VariantTypeSpecV4>)[#"kernel.​Variant​Type​Spec​V4"], <t-sen-kernel-VariantTypeSpecV4>),
  (link(<t-sen-kernel-VariantTypeSpecV5>)[#"kernel.​Variant​Type​Spec​V5"], <t-sen-kernel-VariantTypeSpecV5>),
  (link(<t-sen-kernel-VirtualClock>)[#"kernel.​Virtual​Clock"], <t-sen-kernel-VirtualClock>),
  (link(<t-sen-kernel-VirtualKernelClock>)[#"kernel.​Virtual​Kernel​Clock"], <t-sen-kernel-VirtualKernelClock>),
  (link(<t-sen-kernel-VirtualMasterClock>)[#"kernel.​Virtual​Master​Clock"], <t-sen-kernel-VirtualMasterClock>),
  (link(<t-sen-kernel-WordSize>)[#"kernel.​Word​Size"], <t-sen-kernel-WordSize>),
  (link(<t-sen-kernel-log-BasicFile>)[#"kernel.​log.​Basic​File"], <t-sen-kernel-log-BasicFile>),
  (link(<t-sen-kernel-log-ColorStderr>)[#"kernel.​log.​Color​Stderr"], <t-sen-kernel-log-ColorStderr>),
  (link(<t-sen-kernel-log-ColorStdout>)[#"kernel.​log.​Color​Stdout"], <t-sen-kernel-log-ColorStdout>),
  (link(<t-sen-kernel-log-Config>)[#"kernel.​log.​Config"], <t-sen-kernel-log-Config>),
  (link(<t-sen-kernel-log-LogLevel>)[#"kernel.​log.​Log​Level"], <t-sen-kernel-log-LogLevel>),
  (link(<t-sen-kernel-log-LoggerConfig>)[#"kernel.​log.​Logger​Config"], <t-sen-kernel-log-LoggerConfig>),
  (link(<t-sen-kernel-log-LoggerConfigList>)[#"kernel.​log.​Logger​Config​List"], <t-sen-kernel-log-LoggerConfigList>),
  (link(<t-sen-kernel-log-Null>)[#"kernel.​log.​Null"], <t-sen-kernel-log-Null>),
  (link(<t-sen-kernel-log-RotatingFile>)[#"kernel.​log.​Rotating​File"], <t-sen-kernel-log-RotatingFile>),
  (link(<t-sen-kernel-log-SinkConfig>)[#"kernel.​log.​Sink​Config"], <t-sen-kernel-log-SinkConfig>),
  (link(<t-sen-kernel-log-SinkConfigList>)[#"kernel.​log.​Sink​Config​List"], <t-sen-kernel-log-SinkConfigList>),
  (link(<t-sen-kernel-log-SinkNameList>)[#"kernel.​log.​Sink​Name​List"], <t-sen-kernel-log-SinkNameList>),
  (link(<t-sen-kernel-log-SinkType>)[#"kernel.​log.​Sink​Type"], <t-sen-kernel-log-SinkType>),
  (link(<t-sen-kernel-log-Stderr>)[#"kernel.​log.​Stderr"], <t-sen-kernel-log-Stderr>),
  (link(<t-sen-kernel-log-Stdout>)[#"kernel.​log.​Stdout"], <t-sen-kernel-log-Stdout>),
  (link(<t-sen-kernel-log-Syslog>)[#"kernel.​log.​Syslog"], <t-sen-kernel-log-Syslog>),
)
