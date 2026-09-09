// The document the generator emits. Everything that decides how it LOOKS is in
// style.typ; everything that decides what NON-GENERATED content appears is a file
// you supply. This skeleton is a starting point — replace it freely, and only
// reference.typ is rewritten when the model changes.

#let doc-title = "The Sen API"
#let doc-subtitle = "Interface Control Document"

#set document(title: doc-title)

#import "style.typ": apply-text-rules, reference-layout

#set page(
  paper: "a4",
  margin: (top: 2.2cm, bottom: 2cm, x: 2cm),
  header: context {
    if counter(page).get().first() > 1 [
      #set text(8pt, fill: luma(100))
      #doc-title #h(1fr) #doc-subtitle
      #v(-6pt) #line(length: 100%, stroke: 0.4pt + luma(180))
    ]
  },
  footer: context [
    #set text(8pt, fill: luma(100))
    #h(1fr) #counter(page).display("1 of 1", both: true)
  ],
)
// Named, not a fallback chain: this is a reference people cite by page, and metrics
// move the page count. Typst bundles no sans-serif, so Liberation Sans has to be
// installed — it is in Sen's build image, and `fonts-liberation` on a Linux desktop.
// Without it typst warns and sets the document in a serif instead.
#set text(font: "Liberation Sans", size: 9pt)
#set par(justify: true, leading: 0.62em)
#show: apply-text-rules

#include "front-matter.typ"

#pagebreak()
#outline(title: [Contents], depth: 2, indent: 1em)

#include "about-this-example.typ"

#show: reference-layout
#include "reference.typ"

// your appendices: supply a .typ file and include it here
