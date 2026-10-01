# Moving an object between two buses

One process owns two buses and moves an object from the first to the second and back, each move a
removal and an addition in the same cycle. Another process watches both buses through one
`ObjectMux` and reports what it was told.

`ObjectMux` counts how many providers offer an object. It reports an addition only when that count
leaves zero and a removal only when it reaches zero, so a move is announced only if the removal
arrives first; if the addition arrives first the count goes 1 -> 2 -> 1 and the two callbacks that
fire reach no listener in this repository. The suite exists because that order is decided in the
watching process, which no unit test in one process can show.

A correct run gives the watcher three arrivals and two departures: the first publication, then one
of each per move. The watcher says `BUS MOVE OK` as soon as it has them, and `BUS MOVE FAIL` with
everything it did hear once its deadline passes, so a run that hears too little still says what it
heard. Both are matched by ctest.

The mover holds each move until the watcher publishes that it has heard the one before it. A fixed
gap between the moves would be a guess about the watcher's drain: a stall long enough to put both
moves in one drain over there makes the totals come out wrong with nothing actually broken, and a
gap short enough to be quick is the one most likely to be outrun on a loaded machine. Waiting on the
count also means a slow discovery cannot turn a real failure into a missed first publication.
