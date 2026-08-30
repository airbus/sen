# Versioning and compatibility

Sen carries two protocol versions, and they answer different questions.

| Protocol | What it governs | Today | Compatible with |
|---|---|---|---|
| Kernel | The wire format two kernels use to talk to each other | 9 | 9 |
| Ether | Discovery and transport between processes | 2 | 2 and later |

A kernel protocol difference refuses the connection, and the log names both versions along with the
other side's host, application and process id. It is not a silent failure and not a degraded mode.
An ether difference at 2 or above is adapted and logged; below 2 the connection is refused the same
way.

Type versions are a separate matter and are expected to differ. Participants that disagree about a
type adapt to each other, within the limits that
[Run-time compatibility](compatibility_conversions.md) describes and under the `compatibility`
setting you choose.

The versioning exists so that backwards compatibility can be kept across both protocols, and so
that a compatibility matrix can say which Sen releases work together.

## How a release is made

Versions are `major.minor.patch`. A release begins as a branch named for its series, such as
`release/0.6.x`. Release candidates are tagged on that branch with an `-rcN` suffix, `0.6.0-rc1`
and `0.6.0-rc2`, and when one is accepted it takes the final tag, `0.6.0`. The branch stays
afterwards, so a released series can still receive patches without waiting for the next one:
`0.5.1` and `0.5.2` were both tagged on `release/0.5.x`.
