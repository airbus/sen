# Integration testing

[Unit testing](unit_tests.md) drives one kernel from a test binary. Integration testing drives
several.

The container harness runs the real thing: separate processes, each in its own container, on a
Docker network, talking over the actual transport. It is the slower option and the one that proves
the chain end to end.

Sen's own integration suite is built this way and is worth reading before you write your own. It
lives in `libs/kernel/test/integration/`, and the cases there cover object synchronization, interest
filtering, transport behavior, type clashes, runtime compatibility, crash reporting and stress.
Each case is an ordinary Sen package: an `.stl` file, its C++, configuration templates and a readme
explaining the scenario.

The driver is `run.py`, which uses `testcontainers` to bring up a network and a container per
participant, and streams their logs. It is controlled by environment variables.
`SEN_INTEGRATION_TEST_IMAGE` names the image and has no default, because the harness runs binaries
that are already built and the image therefore has to match the system they were built on;
`tools/ci/runtime.Dockerfile` builds a suitable one. `SEN_INTEGRATION_TEST_TIMEOUT` bounds a run,
defaulting to thirty seconds. `SEN_INTEGRATION_TEST_MOUNT` is where the repository appears inside
the container, defaulting to `/home/builder/sen`.

The part worth copying is how the assertions are written. The test driver is itself a Sen component.
The configuration loads `ether` for the transport and `py` running a test module, so the code making
the assertions is inside a kernel, subscribing to objects and calling methods like any other
participant, instead of poking at the system from outside it.
