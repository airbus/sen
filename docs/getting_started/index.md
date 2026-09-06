# Getting started

**1. [Install Sen](install.md).** On Linux the quickest route is the installer script, which
downloads a release and writes an `activate` script you source from your shell. The same page covers
consuming Sen as a Conan dependency, and building it from source if you need to.

**2. Then take whichever route suits you.** Both start from an installed Sen and cover much the same
ground.

- **[The tutorials](../tutorials/index.md)** walk it as a story: one object that counts and
  publishes itself, then two objects calling each other, then the same pair split across two
  processes.
- **[Create your first package](first_package.md)** lays the same territory out as reference.
  `sen package init` generates a skeleton, and the page walks through what each generated file is
  for.

If a word is doing more work than you expect, the [glossary](../users_guide/glossary.md) covers the
ones Sen redefines, and the ones that collide with something you already know.

After that, the [examples](../examples/index.md) are a graded set of working packages, the
[how-to guides](../howto_guides/index.md) answer specific questions, and the
[manual](../users_guide/index.md) explains how Sen works underneath.

---

[Running the tests](testing.md) is a different kind of page. It covers building and testing **Sen
itself** from a source checkout, which is a contributor task and not a step on this path. You do
not need it to use Sen.
