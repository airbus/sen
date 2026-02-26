# UML Generation

With this CMake function you can tell Sen to generate UML diagrams from HLA FOMs:

```cmake
  sen_generate_uml(TARGET fom_uml OUT fom.plantuml HLA_FOM_DIRS rpr netn link16 CLASSES_ONLY)
```

This produces the following output:

![file](../snippets/fom.svg){ .on-glb }

You can open the image on a separate tab to get a better view.
