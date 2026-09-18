# Using the Dead Reckoning library

The aircraft example uses both `DeadReckonerBase` and `SettableDeadReckoner<T>`, the first to
extrapolate the entity's position from the speed it was given, the second to write the result into
the RPR `Spatial` attribute. This is the whole of it:

```c++ title="examples/packages/aircrafts/src/dummy_aircraft_impl.cpp"
--8<-- "examples/packages/aircrafts/src/dummy_aircraft_impl.cpp:dead_reckoning"
```

`SettableDeadReckoner<T>` is constructed with `*this`, so it writes into the object's own spatial
properties, and with a `DrThreshold` saying how far the extrapolation may drift from the data before
the `Spatial` is written. The example passes zeros there, so it writes on every cycle.

`DeadReckonerBase` is not bound to an object: you give it a situation and ask for one back. That is
why the example seeds it on the first cycle with a world location and the commanded velocity, and
feeds it again whenever `speed` changes. Extrapolating an entity you own is not what these classes
are for, since they exist to fill the gaps between updates from somewhere else, but it makes for a
compact example.

A second, larger example lives in the **Sen Sim Tools** repository, which is a separate project and
not part of this distribution. It adds a Dead Reckoning Viewer package that finds FOM objects on a
bus, extrapolates their position and orientation, and plots the smoothed result against the raw
values. If you have that repository checked out:

```bash title="Run Dead Reckoning Viewer"
python3 <sen_sim_tools_root>/examples/sen_dr/run_sen_dr.py
```

This example opens a Sen explorer window where you can plot the smoothed position of the entity and
compare it with the position received from the Sen bus. The data from the bus comes from a recording
where the entity was randomly moved to generate noisy spatial data. The input data from the
recording can be seen in the `dr.input` bus and the smoothed output in the `local.output` bus.

The configuration of the `DeadReckoner<T>` class has some parameters that you may need to tune in
some scenarios.

Consider the following recommendations:

- If the smoothed solution becomes unstable (check the orientation, where this issue is more
  likely), try setting a smaller `smoothingInterval`, as it will prevent the acceleration predictor
  from overshooting.

- If you see a lack of responsiveness in the position/orientation, try decreasing the corresponding
  convergence time. Keep in mind that a smaller convergence time can lead to instabilities in the
  smoothing algorithm.

- If the smoothed solution is underdamped and oscillates, try increasing the corresponding damping
  coefficient until you see these oscillations disappear.

- Keep in mind that the Dead Reckoner works well when the updates come in at a considerable
  frequency (at least 20 Hz). If data is received at a slower rate, the smoothed solution could
  slightly oscillate around the incoming updates. This is most noticeable when extrapolating
  orientations from updates that carry no angular velocity.
