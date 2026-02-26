# Using the Dead Reckoning library

- First, in the **Sen Examples**, the aircraft example uses the Dead Reckoning library (both the
  `DeadReckoner<T>` and the `SettableDeadReckoner<T>`) to compute the location of the aircraft and
  update its FOM Spatial data accordingly.

- Second, in the **Sen Sim Tools** repository, specifically in the examples section, you can find a
  simple example on how to use the Sen Dead Reckoning Library. We created a package called Dead
  Reckoning Viewer, which detects FOM objects on a Sen bus and extrapolates their location and
  orientation using Dead Reckoning. To run the example, configure the environment (take a look at
  the readme in the examples folder) and run the following command:

```bash title="Run Dead Reckoning Viewer"
python3 <sen_sim_tools_root>/examples/sen_dr/run_sen_dr.py
```

This example opens a Sen Explorer window where you can plot the smoothed position of the entity and
compare it with the position received from the Sen bus. The data from the bus comes from a recording
where the entity was randomly moved to generate noisy spatial data. The input data from the
recording can be seen in the `dr.input` bus and the smoothed output in the `local.output` bus.

The configuration of the `DeadReckoner<T>` class has some parameters that the user may need to trim
in some scenarios.

Consider the following recommendations:

- If the smoothed solution becomes unstable (check the orientation, where this issue is more
  likely), try setting a smaller `smoothingInterval`, as it will prevent the acceleration predictor
  from overshooting.

- If you see a lack of responsiveness in the position/orientation, try decreasing the corresponding
  convergence time. Keep in mind that a smaller convergence time can lead to instabilities in the
  smoothing algorithm.

- If the smoothed solution is under damped and oscillates, try increasing the corresponding damping
  coefficient until you see these oscillations disappear.

- Keep in mind that the Dead Reckoner works well when the updates come in at a considerable
  frequency (at least 20 Hz). If data is received at a slower rate, the smoothed solution could
  slightly oscillate around the incoming updates. This is specially observed when trying to
  extrapolate orientations, and the updates do not include angular velocity information.
