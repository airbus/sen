# Base architecture for the Kernel Integration tests

The base architecture for the integration tests allows to set up publishers and listeners in a
random amount of processes and components, arranged in any desired configuration, to test the
functionality of the Sen kernel in complex scenarios.

The architecture is composed of these three objects:

- `Publisher`: performs actions in the test, such as publishing or deleting objects
- `Listener`: reacts to the actions performed by the publisher and checks the results
- `ProcessTerminator`: in charge of terminating the process once all publishers and listeners have
  finished their tasks. This helps the test stop all processes automatically.

Both the Publisher and Listener evolve according to this shared state:

```rust
// A StatefulObject used in the integration tests can transition between these different states
enum ConnectionState : u8
{
  starting,
  ready,
  step1,
  step2,
  step3,
  step4,
  finished
}
```

This 4-phase state machine allows a single integration test to execute four sequential
action-and-verification steps. It ensures both the publisher and listener advance deterministically
without race conditions.

The workflow for the integration tests is the following:

```
       ┌───────────┐          ┌─────────────┐          ┌────────────────────┐
       │ Publisher │          │  Listeners  │          │ Process Terminator │
       └─────┬─────┘          └──────┬──────┘          └─────────┬──────────┘
             │                       │                           │
             ▼                       │                           │
(1) all listeners detected?          │                           │
      (state -> READY)               │                           │
             │                       ▼                           │
             │           (2) all publishers READY?               │
             │                (state -> STEP1)                   │
             ▼                       │                           │
(3) all listeners STEP1?             │                           │
     (act + state -> STEP1)          │                           │
             │                       ▼                           │
             │           (4) all publishers STEP1?               │
             │            (check + state -> STEP2)               │
             ▼                       │                           │
(5) all listeners STEP2?             │                           │
     (act + state -> STEP2)          │                           │
             │                       ▼                           │
             │           (6) all publishers STEP2?               │
             │          (check + state -> FINISHED)              │
             ▼                       │                           │
(7) all listeners FINISHED?          │                           │
    (state -> FINISHED)              │                           │
             │                       │                           ▼
             │                       │              (8) all components FINISHED?
             │                       │                    (system -> SHUTDOWN)
             ▼                       ▼                           ▼
```

For brevity, the diagram illustrates only two states; however, the remaining phases follow the exact
same transition pattern.
