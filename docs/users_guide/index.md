![Screenshot](../assets/images/eyes_light.svg#only-light){: style="width:150px; float: right;"}
![Screenshot](../assets/images/eyes_dark.svg#only-dark){: style="width:150px; float: right;"}

# Overview

- In Sen, you write *packages*. Packages contain your functionality in the form of classes that can
  be instantiated to create objects. Sen objects are regular C++ objects with "super-powers" infused
  by the Sen code generator.

- By writing config files, you tell Sen which packages to load and which objects to create. The
  grouping of objects that run in a thread and serve some specific purpose is called *component*.

- Components can be instantiated wherever you like (same process, same computer, other computers)
  and Sen makes this transparent to your code. You achieve this by publishing objects to *buses*.
  Sen *sessions* are namespaces for *buses*.

- Time can be virtualized. Sen can run your code in real time or stepped mode (slower or faster than
  real time - you choose).

- You can define your types and classes using a little language called STL, and it can also be done
  using standard HLA specs (for those who like it, or cannot avoid it).

- Sen comes with a bunch of tools that let you interact with your systems, test them, script them,
  and create your own apps.

That´s basically it. Now we take a step back and get an understanding of the bigger picture and how
everything fits together.

To understand how Sen sees the world, let's define a few lightweight concepts:

- **Classes** define the interface of objects.

- **Objects** encapsulate state (properties) and behavior (your code).

- **Packages** are libraries of classes.

- **Components** run your code by importing packages and instantiating objects.

- **Systems** are a collection of components that are organized for a common purpose.
