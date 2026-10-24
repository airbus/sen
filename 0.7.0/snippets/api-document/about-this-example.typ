// Included between the contents and the generated reference. Like the title page, Sen
// never reads this: it is a Typst file named on the command line, and everything it says
// about itself is demonstrated by the fact that you are reading it.

#pagebreak()

= About this document

#block(width: 15cm)[
  This is an example published with Sen's documentation. It was produced by running the
  model through `sen generate typst` and compiling the result, with no editing in between.

  Everything from #emph[Model overview] onwards is generated. It follows the model and only
  the model: if a type is added, renamed or given a new description, the next build says so,
  and nothing anyone wrote by hand is lost in the process.

  This page, and the cover before it, are not generated. They are Typst files of your own
  that the document includes at points it leaves open for you:

  #v(4pt)
  #table(
    columns: (4.6cm, 1fr),
    stroke: none,
    inset: (x: 0pt, y: 3pt),
    align: (left, left),
    [#raw("--front-matter")], [A cover, before the contents.],
    [#raw("--before-reference")], [Anything between the contents and the model — a scope, a
      set of conventions, an approvals table. This page.],
    [#raw("--after-reference")], [Appendices, after the model.],
    [#raw("--style")], [Every decision about how the document looks.],
  )

  #v(4pt)
  The generator does not read any of those files. It writes an `#raw("#include")` and Typst
  resolves it when the document is compiled, so a cover page can be anything Typst can set —
  and changing it never means regenerating the model.
]
