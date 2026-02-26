# Using the shipped components

Sen allows you to create your own components, but there are some common needs that are present in
all projects. Maybe you will experience them during development, testing or in production, but they
will be there.

To make your life easier, Sen comes with a set of components that are at your disposal:

- A command-line interface for interacting with the components and objects that are running in a
  process. This component is called *shell*, and you can also remotely connect to it.
- A graphic user interface to explore the set of components, objects and their interactions in an
  easy way. This component is called *explorer*.
- An optimized inter-process communication over ethernet networks. This component is called *ether*.
- A recorder to store what happens in a process: objects, types, property changes, events,
  keyframes, etc.
- A replayer to playback whatever you have previously recorded.
- An embedded Python interpreter with full access to the Sen world, for tests, orchestrators and
  functional logic.
- A gateway towards InfluxDB, which can be connected to Grafana to inspect and analyze data.
- A REST API for interfacing with non-Sen systems.
- A connector to the [Tracy](https://github.com/wolfpld/tracy) profiler.
- A helper for controlling and configuring your debug logs.

These are the shipped components that are present today, but this list might grow in the future.

Let's put an example. Imagine that you have a system made of four functional components, and you
want to have the *ether* component to allow them to interact with other systems. You need the
*shell* during development to test your functionality. The *explorer* also helps you visualize
what's going on and monitor the execution. The *REST* helps you by allowing a script stimulate and
test your system. Finally, you also sometimes store the execution data using the *recorder*
component.

Your system might look like this:

![Screenshot](../assets/images/services_1_light.svg#only-light){: style="width:1100px"}
![Screenshot](../assets/images/services_1_dark.svg#only-dark){: style="width:1100px"}

This is fine during development, but you also start to see that you need to test your system a bit
more independently of the tools that you use during development. For that, you can simply change the
configuration file and move the *explorer* and *shell* components to a different process.

To get the two processes to see each-other you need to instantiate the *eth* component in both.

![Screenshot](../assets/images/services_3_light.svg#only-light){: style="width:1250px"}
![Screenshot](../assets/images/services_3_dark.svg#only-dark){: style="width:1250px"}

We can now independently start and stop our monitoring tools when needed. Of course, running the
*shell*, the *explorer* and the *eth* components creates some load on our computer, so we can move
those tools to another computer and keep our system running truly independently in another one (we
have two computers to take care of, so we will now be in the need of some kind of deployment and
orchestration solution. Containers will probably help you here, but we will not go into that in this
document). After a while we start to see that the computer where we run our functional components
does not have a good storage device (maybe not big enough, or not fast enough).

![Screenshot](../assets/images/services_4_light.svg#only-light){: style="width:1200px"}
![Screenshot](../assets/images/services_4_dark.svg#only-dark){: style="width:1200px"}

We can have a dedicated computer for doing the archiving and have it as an optional element in our
setup. So, we modify our configuration file accordingly.

![Screenshot](../assets/images/services_5_light.svg#only-light){: style="width:1400px"}
![Screenshot](../assets/images/services_5_dark.svg#only-dark){: style="width:1400px"}

This is just an example, but the takeaway here is that you can mix and match the built-in components
as you would do with your own, and deploy them according to your needs.
