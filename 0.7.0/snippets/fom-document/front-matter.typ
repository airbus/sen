// The title page for the documents the website publishes as examples. Sen never reads
// this file: the generator writes `#include "front-matter.typ"` at the point the skeleton
// marks, and Typst resolves it when the document is compiled.

#v(1fr)

#align(center)[
  #context text(26pt, weight: 700)[#document.title]

  #v(6pt)
  #text(13pt, fill: luma(90))[Interface Control Document]

  #v(2cm)
  #block(width: 11cm)[
    #set par(justify: false)
    #set align(left)
    #text(10pt, fill: luma(70))[
      An example, generated from the data model by Sen. The reference that follows —
      every package, every type, every table — is written by the tool and is rewritten
      whenever the model changes.

      This page is not. It is an ordinary Typst file included at a point the generated
      document leaves open, which is how a project puts its own cover, approvals and
      appendices around a reference it never has to edit.
    ]
  ]
]

#v(1fr)

#align(center)[
  #text(9pt, fill: luma(120))[
    Placeholder for your own project or product assets
  ]
]

#v(1cm)
