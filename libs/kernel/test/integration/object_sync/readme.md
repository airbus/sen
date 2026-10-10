# Object Synchronization integration tests

These tests check data correctness in the communication between kernel participants in different
network configurations.

## Publisher / Listeners

The following objects are involved:

- _publisher_: This object is published in the kernel pipeline configuration (yaml) and is in charge
  of publishing the object under test, which will be detected by the listeners. It is used to
  correctly sync with the listeners and avoid timing errors when running the listeners in a separate
  component.

- _testObject_: Contains all properties, events and methods whose correctness will be tested. It is
  added and removed from the bus by the publisher. It contains a method called `doUpdate()`, which
  will be called by the publisher when it certifies that the listeners have all detected the object.
  This `doUpdate()` method enables automatic property updates and event emissions on every update of
  the object (implemented in the `update()` method).

- _listeners_: Collection of listener objects, four in the current version of the test, which
  detect the test object and check whether the property updates, events or method responses received
  are the expected ones. Each test checks a certain class member (e.g. confirmed property updates),
  and in order to implement the specific checks for each test, various listener classes are derived
  from a base `ListenerImpl` class. Additionally, each of the listeners have a configurable query
  for the interest in the object, allowing to test different local participants with the same, or
  different interests.

The implementation of the publisher and listeners contains logic that shuts down the kernels if the
checks pass. `run.py` just launches the processes, spits the logs through stdout, and exits with a
timeout (5 seconds) if processes do not exit automatically before. Any communication error between
the participants results in a timeout error in the integration test. If the error is in the
correctness of the data, assert statements will indicate precisely what failed. Each process is
executed in an isolated container spawned from `run.py` using the `test_containers` python library.

The publisher and listeners interact in the following way (in a successful test):

```text
       ┌───────────┐          ┌─────────────┐
       │ Publisher │          │  Listeners  │
       └─────┬─────┘          └──────┬──────┘
             │                       │
             ▼                       │
(1) all listeners detected?          │
      (add testObject)               │
             │                       ▼
             │           (2) test object detected?
             │                 (state -> ready)
             ▼                       │
 (3) all listeners ready?            │
     (update testObject)             │
             │                       ▼
             │           (4) 10 updates received?
             │                (state -> inSync)
             ▼                       │
 (5) all listeners inSync?           │
     (remove testObject)             │
             │                       ▼
             │           (6) testObject removed?
             │     (check correctness, state -> finished and kernel shutdown)
             ▼
 (7) all listeners finished?
      (kernel shutdown)

```

As you can see from the diagram, both the listeners and publisher shutdown the kernel in case of a
correct execution.

## Peer-to-Peer

This topology checks peer-to-peer data synchronization where
multiple processes act simultaneously as publishers and listeners.

- _SyncNode_: Each process runs a `SyncNode` object. It publishes its own `TestObject` and subscribes
  to the `TestObject`s published by all other nodes on the network.

- _State synchronization_: To prevent communication errors, nodes monitor each other's states.
  A node begins issuing property updates only after it has detected the test objects from all
  other expected nodes. It only shuts down when all nodes have validated the data.

The nodes interact in the following way (in a successful test):

```text
       ┌───────────┐                  ┌───────────┐                 ┌───────────┐
       │  Node 1   │                  │  Node 2   │       ...       │  Node N   │
       └─────┬─────┘                  └─────┬─────┘                 └─────┬─────┘
             │                              │                             │
   (1) publish testObject_1        publish testObject_2           publish testObject_n
             │                              │                             │
             ▼                              ▼                             ▼
   (2) all remote objects            (state -> ready)              (state -> ready)
         detected?                          │                             │
             │                              │                             │
             ▼                              ▼                             ▼
   (3) update testObject_1          update testObject_2           update testObject_n
             │                              │                             │
             ▼                              ▼                             ▼
   (4) 10 updates received from      (state -> inSync)             (state -> inSync)
      all remote objects?                   │                             │
             │                              │                             │
             ▼                              ▼                             ▼
   (5) all nodes inSync?          (state synchronization)       (state synchronization)
             │                              │                             │
             ▼                              ▼                             ▼
   (6) remove testObject_1          remove testObject_2           remove testObject_n
             │                              │                             │
             ▼                              ▼                             ▼
   (7) all remote objects            (state -> finished)           (state -> finished)
          removed?                          │                             │
             │                              │                             │
             ▼                              ▼                             ▼
   (8) all nodes finished?           (kernel shutdown)             (kernel shutdown)
      (kernel shutdown)

```

## Execution

All tests in this module are executed in three distinct network configurations:

- `Local`: All participants (publishers, listeners, or nodes) run in the same component.
  Local method calling can be checked in this configuration.
- `Single process`: Participants run in separated components, but all within the same process.
  This asserts the correct functioning of the kernel with several local participants across multiple threads (components).
- `Multiple processes`: Participants are split across different processes, and also in different components,
  testing the detection of a remote object published by a remote participant by several local participants in different threads.

The implementation of the participants contains logic that shuts down the kernels if the
checks pass. `run.py` just launches the processes, spits the logs through stdout, and exits with a
timeout (5 seconds) if processes do not exit automatically before. Any communication error between
the participants results in a timeout error in the integration test. If the error is in the
correctness of the data, assert statements will indicate precisely what failed. Each process is
executed in an isolated container spawned from `run.py` using the `testcontainers` python library.
