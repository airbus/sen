# Typeset reference (PDF)

A data model set as a document you would hand to somebody: an overview of the packages, the class
hierarchy across all of them, a table for every type, and an index. Every type name is a link, so it
reads on screen the way the [HTML reference](generated_reference.md) does, and it prints.

Two, because they are shaped differently:

[The RPR, NETN and Link 16 FOMs](../snippets/fom-document/reference.pdf){ target="_blank" } — the
same model as the HTML reference. One deep class tree, several hundred pages.

[The Sen API](../snippets/api-document/reference.pdf){ target="_blank" } — Sen's own model, which is
fifteen packages nested three deep, so the overview shows the package tree rather than one flat list.

Both were produced by running the model through the generator and compiling the result, with no
editing in between. The cover and the page after the contents are Typst files of Sen's own, included
at points the generated document leaves open — the documents explain that about themselves.

This is how the `hla_fom` example asks for it:

```cmake title="examples/packages/hla_fom/CMakeLists.txt"
--8<-- "examples/packages/hla_fom/CMakeLists.txt:typst"
```

That writes three files. `document.typ` is the one you compile; it imports `style.typ` and includes
`reference.typ`, which holds the model. `PDF` adds the compile step, which needs the
[Typst](https://typst.app/) compiler on your path — a single binary with no runtime under it.

From the command line the same thing is `sen generate typst fom --directories rpr netn link16
-o document`, followed by `typst compile document/document.typ`.

## Making it yours

The generator writes the model. Everything around it is yours, named on the command line rather than
guessed at:

| | What it does |
| --- | --- |
| `--front-matter` | A Typst file included first, for a title page. |
| `--before-reference` | A Typst file included after the front matter, for a scope or an approvals page. |
| `--after-reference` | A Typst file included after the model, for appendices. |
| `--style` | Replaces `style.typ`, and with it every decision about how the document looks. |

The generator never reads any of those files. It writes an `#include`, copies the file in beside
the document, and Typst resolves the name when the document is compiled — so a title page can be
anything Typst can set, and changing it does not mean regenerating the model. They are copied
because Typst resolves an include against the directory holding the document and refuses a path
that climbs out of it.

Sections can also be dropped: `--no-overview`, `--no-hierarchy`, `--no-summaries`, `--no-index`,
`--no-used-by`, `--no-flag-legend` and `--no-built-ins`. `--include-package` and `--exclude-package`
narrow the document to part of the model, and each takes one package and may be repeated.

## Two things that are not styling

**The fonts are named, not chosen.** Liberation Sans for the text and DejaVu Sans Mono for
everything monospaced, so the same model produces the same document on every machine — which matters
for something people cite by page number. Typst bundles DejaVu Sans Mono and no sans-serif at all, so
Liberation Sans has to be installed: it is in Sen's build image, and `fonts-liberation` on a Linux
desktop. Without it Typst warns and sets the document in a serif, which repaginates it and stops the
tree connectors lining up, because those only align in a fixed pitch. `--style` replaces the choice
entirely.

**A link only ever points into the document.** Typst treats a link to something absent as an error
rather than a dangling link, so narrowing the model with `--exclude-package` turns references into
that package into plain text instead of breaking the build.

The same command takes `stl` in place of `fom` for a model written in STL, or a model that mixes
the two.
