// THE STYLE MODULE. One file decides how every table, heading and navigation
// aid looks. The generated content imports it; a user can replace it wholesale
// without touching a line of generated output.

#let accent = rgb("#2f6fd0")

// The html generator's kind palette, reused rather than invented: a reader who
// uses both outputs sees one visual language, and the colour carries the kind
// rather than decorating it.
#let KIND-COLOUR = (
  classes:      rgb("#4055c8"),
  structures:   rgb("#1d7a70"),
  enumerations: rgb("#8e3b9c"),
  sequences:    rgb("#a05f00"),
  variants:     rgb("#c2185b"),
  quantities:   rgb("#4a7c1f"),
  aliases:      rgb("#5a6b75"),
  optionals:    rgb("#4553a8"),
)
#let kind-of(k) = KIND-COLOUR.at(k, default: luma(90))

// Named rather than a fallback chain: metrics move the page count, and the tree
// connectors only line up in a fixed pitch. DejaVu Sans Mono ships inside typst;
// Liberation Sans is in the build image.
#let MONO = "DejaVu Sans Mono"

#let COL-NAME = 4.1cm
#let COL-TYPE = 3.7cm
#let COL-FLAGS = 1.9cm

// An identifier is one long word and will not break on its own. Offering a
// break at each camelCase boundary lets it wrap inside its column instead of
// widening the table or running off the page.
#let mono(s) = {
  set text(font: MONO, size: 8pt)
  s
}
// A wrapper type is one line of the language, so the page shows that line rather than
// describing it in prose. Same shape and the same colours as the html explorer, so a
// reader who uses both sees one visual language.
#let kw(s) = text(fill: rgb("#8250df"), weight: 600, s)
#let lit(s) = text(fill: rgb("#1d7a70"), s)
#let declared(body) = block(
  above: 4pt,
  below: 8pt,
  inset: (x: 7pt, y: 5pt),
  fill: luma(249),
  radius: 2pt,
  width: 100%,
  {
    set text(font: MONO, size: 8.5pt)
    body
  },
)

#let rule-grey = luma(180)

// The current section's colour, set by the generated content as each section
// opens. A state rather than an argument, because the show rule that draws the
// heading rule cannot be passed one.
#let section-colour = state("section-colour", luma(150))
#let section(name, kind) = {
  section-colour.update(kind-of(kind))
  heading(level: 3, name)
}

// A small coloured chip carrying the kind. Information, not ornament: it says
// at a glance what a reader would otherwise have to infer from the tables.
#let chip(kind, wording) = box(
  inset: (x: 4pt, y: 1.5pt), outset: (y: 1pt), radius: 2pt,
  fill: kind-of(kind).lighten(88%),
  text(7pt, weight: 600, fill: kind-of(kind).darken(12%), upper(wording)),
)

// Justification is right for prose and wrong in a narrow cell, where it opens
// rivers. Tables set ragged-right.
#let sen-table(..args) = block(above: 8pt, below: 10pt, context {
  set par(justify: false)
  let c = section-colour.get()
  table(
    stroke: (x, y) => if y == 0 { (bottom: 0.8pt + c) } else { none },
    inset: (x: 6pt, y: 4pt),
    fill: (_, y) => if y == 0 { luma(243) } else if calc.odd(y) { luma(251) },
    align: left + top,
    ..args,
  )
})

#let facts(body) = block(above: 3pt, below: 7pt, text(8pt, fill: luma(95), body))

// Full measure, by preference: the narrower prose column was tried and the
// wider one reads better here, where a description sits directly above the
// table it introduces and the eye travels between them.
#let prose(body) = block(above: 2pt, below: 7pt, body)
#let legend(body) = block(above: 2pt, below: 8pt,
  text(7.5pt, style: "italic", fill: luma(95), body))

// The class hierarchy, in the spirit of the OMT object class structure table:
// what inherits from what, at a glance, before any of the detail. Indentation
// rather than the standard's nested columns, because four levels of nesting in
// columns leaves the deepest names a few centimetres wide.
#let hierarchy(..rows) = block(above: 6pt, below: 14pt, {
  set par(justify: false)
  set text(8.5pt)
  table(
    columns: (1fr, auto),
    stroke: none,
    inset: (x: 4pt, y: 2.4pt),
    align: (left + top, right + top),
    ..rows.pos().map(((branch, name, lbl)) => (
      {
        // The connectors must be monospace or the levels do not line up.
        text(font: MONO, size: 8pt, fill: luma(155), branch)
        name
      },
      context text(fill: luma(120), str(query(lbl).first().location().page())),
    )).flatten(),
  )
})

// What the model is made of, before any of it: one row per package, its size
// broken down by kind, and where it starts.
#let packages(ncols, head, ..rows) = block(above: 6pt, below: 14pt, {
  set par(justify: false)
  // Ten columns: at the body size the headers touch. They are short words and
  // read fine a point smaller, which is cheaper than abbreviating them.
  set text(7.5pt)
  table(
    // The name column takes what it needs; the counts are narrow and equal.
    columns: (auto,) + (auto,) * (ncols - 1),
    stroke: (x, y) => if y == 0 { (bottom: 0.8pt + luma(120)) } else { none },
    inset: (x: 7pt, y: 3.5pt),
    fill: (_, y) => if y == 0 { luma(243) } else if calc.odd(y) { luma(251) },
    align: (col, _) => if col == 0 { left } else { right },
    ..head, ..rows.pos().flatten(),
  )
})

// ---- navigation ----------------------------------------------------------

// The print equivalent of the html tree: every type in the section, one line
// each, with the page it is on. Dot leaders because a reader scans across.
#let summary(..rows) = block(above: 6pt, below: 14pt, {
  set par(justify: false)
  set text(8pt)
  table(
    columns: (auto, 1fr, auto),
    stroke: none,
    inset: (x: 4pt, y: 2.6pt),
    align: (left + top, left + top, right + top),
    ..rows.pos().map(((name, desc, lbl)) => (
      name,
      text(fill: luma(95), desc),
      // A label's page is only knowable after layout, hence context.
      context text(fill: luma(120), str(query(lbl).first().location().page())),
    )).flatten(),
  )
})

// A long enumeration is a narrow list. Running it down one side of the page
// wastes three quarters of the width, so flow it into columns.
#let enum-columns(n, ..rows) = block(above: 8pt, below: 10pt, {
  set par(justify: false)
  set text(8.5pt)
  let pairs = rows.pos()
  let inner(w) = table(
    columns: w,
    stroke: none,
    inset: (x: 4pt, y: 2.2pt),
    fill: (_, y) => if calc.odd(y) { luma(250) },
    align: (left, right),
    ..pairs.flatten(),
  )
  if n == 1 {
    // One column means the names are long, not that the table should span the
    // page: stretched, the value strands itself at the right margin two hundred
    // millimetres from the name it belongs to. Hug the content instead.
    inner((auto, auto))
  } else {
    // A right-aligned value at the edge of a balanced column sits against the
    // next column's first character and reads as overlap. The gutter has to
    // clear the widest value, not merely separate the boxes.
    columns(n, gutter: 26pt, inner((1fr, auto)))
  }
})

// The search equivalent: every type, alphabetical, with its page number.
#let index-of(..entries) = {
  set text(8pt)
  set par(justify: false)
  columns(3, gutter: 16pt, {
    for (name, lbl) in entries.pos() {
      block(above: 0pt, below: 1.6pt, {
        name
        h(1fr)
        context text(fill: luma(120), str(query(lbl).first().location().page()))
      })
    }
  })
}

// Applied by the document so a user replacing this file cannot silently lose
// the page furniture.
// TYPOGRAPHY applies to the whole document, user pages included: one look.
#let apply-text-rules(doc) = {
  // Real space above, not zero: a heading a caller writes can follow other content,
  // and with zero space it overlaps the line before it.
  show heading.where(level: 1): it => block(above: 24pt, below: 14pt,
    text(19pt, weight: 700, it.body))
  show heading.where(level: 2): it => block(above: 16pt, below: 10pt, {
    text(16pt, weight: 600, it.body)
    v(-6pt)
    context line(length: 100%, stroke: 1.2pt + section-colour.get())
  })
  // A type heading alone did not separate entries strongly enough. A rule the
  // full width of the page above it makes each type a visible band.
  show heading.where(level: 4): it => block(above: 18pt, below: 6pt, width: 100%, {
    context line(length: 100%, stroke: 0.7pt + section-colour.get().lighten(45%))
    v(3pt)
    text(11.5pt, weight: 600, font: MONO, fill: rgb("#12243c"), it.body)
  })
  show raw: set text(font: MONO, size: 8.5pt)
  show link: set text(fill: accent)
  doc
}

// PAGE BREAKING is scoped to the generated reference only. Applying it globally
// gave a user's own "== Acronyms" a page break they never asked for: a style
// rule that changes layout must not reach into pages the user wrote.
#let reference-layout(doc) = {
  // A package starts a page. A kind section does not: breaking on both left a
  // package opener alone on a page with one line under it, and the coloured
  // rule already separates the sections plainly enough.
  show heading.where(level: 1): it => { pagebreak(weak: true); it }
  show heading.where(level: 2): it => { pagebreak(weak: true); it }
  doc
}
