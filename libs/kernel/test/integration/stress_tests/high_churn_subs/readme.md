# High churn subscriptions test
This test aims to stress the functionality of the Sen kernel when subscriptions are being created and removed from several participants simultaneously. This introduces a load in the `ObjectProvider` and `RemoteInterestHandler` objects.
The test involves two different types of objects:
- `Producer`: Publishes an object which keeps updating a f64 dynamic property in each update. Producers are instantiated in really high frequency components in the test (1000Hz).
- `Consumer`: Creates/removes subscriptions to the `TestObject` instances published by the producers. In each update:
  - An action is performed around 80% of the time (uniform distribution)
  - An action can be adding a local subscriber to the bus, with a random interest selected from a list of 10 interests, and a random object list selected from a set of 10 object lists.
  - An action can also be deleting one of the subscriber lists that were created before
  - These actions are performed in batches of up to three in each update of the consumer component.
  - For the test to pass, this process needs to go on for 20 seconds without any runtime errors.

```
       ┌───────────┐                 ┌──────────┐
       │ Producer  │                 │ Consumer │
       └─────┬─────┘                 └────┬─────┘
             │                            │
             │ (Every 1ms / 1000Hz)       │
             ▼                            ▼
   ┌───────────────────┐        (1) 20s test duration expired?
   │ Update f64 Prop   │              ├── Yes ──> [State -> Finished]
   │ in TestObject     │              └── No  ──> Continue
   └─────────┬─────────┘                  │
             │                            ▼
             │                  (2) Dice roll 1-100 > 80? (Skip 20%)
             │                        ├── Yes ──> [Skip Frame / Idle]
             │                        └── No  ──> Proceed (80% load)
             │                            │
             │                            ▼
             │                  (3) Action selector: Dice roll 0-1
             │                        ├── 1 (50%) ──> Add Subs Batch
             │                        └── 0 (50%) ──> Remove Subs Batch
             │                            │
             │                            ├─► [Add Sub Batch] (Loop 1-3x)
             │                            │     ├── Pick 1 of 10 Interests
             │                            │     ├── Pick 1 of 10 Lists
             │                            │     └── Track in active set
             │                            │
             │                            └─► [Remove Sub Batch] (Loop 1-3x)
             │                                  ├── Pop oldest list from set
             │                                  └── Unregister list
             │                            │
             ▼                            ▼
   (4) All consumers finished?
         (State -> Finished)

```
