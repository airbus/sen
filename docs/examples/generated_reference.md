# HTML Reference

Sen can turn a data model into a small web application: a tree of every type, a search over
names, fields and descriptions, and a page for each type showing what it carries, what it is
built on, and what uses it.

```shell
sen generate html fom --directories rpr netn link16 -t "RPR, NETN and Link 16" -o reference
```

That writes five files. Opening `index.html` is enough — there is no server to run, nothing is
fetched, and the whole thing works from a disk or a memory stick with no network at all.

[Open the reference generated from RPR, NETN and Link 16](../snippets/fom/index.html){ target="_blank" }

The same command takes `stl` in place of `fom` for a model written in STL, or a model that
mixes the two.
