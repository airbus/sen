# UML generation

Sen draws UML diagrams from HLA FOMs, or from STL. This is how the `hla_fom` example asks for one:

```cmake title="examples/packages/hla_fom/CMakeLists.txt"
--8<-- "examples/packages/hla_fom/CMakeLists.txt:uml"
```

That writes a PlantUML document. Running `plantuml` over it gives the picture, which for these
three FOMs is:

![file](../snippets/fom.svg){ .on-glb }

You can open the image on a separate tab to get a better view.
