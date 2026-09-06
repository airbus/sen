# HTML Reference

A browsable reference over Sen's own RPR, NETN and Link 16 example FOMs.
[Code generation](../users_guide/code_generation.md) covers what the generator is for; this page is
one you can open.

This is how the `hla_fom` example asks for it:

```cmake title="examples/packages/hla_fom/CMakeLists.txt"
--8<-- "examples/packages/hla_fom/CMakeLists.txt:html"
```

That writes five files. Opening `index.html` is enough — there is no server to run, nothing is
fetched, and the whole thing works from a disk or a memory stick with no network at all.

From the command line the same thing is `sen generate html fom --directories rpr netn link16
-o reference`.

[Open the reference generated from RPR, NETN and Link 16](../snippets/fom/index.html){ target="_blank" }

The same command takes `stl` in place of `fom` for a model written in STL, or a model that
mixes the two.
