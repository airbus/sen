# Sen STL — Emacs major mode

`stl-mode.el` provides syntax highlighting, comment handling, and a simple
indent function for Sen STL (`.stl`) files.

## Installation

### Manual

```elisp
(add-to-list 'load-path "/path/to/resources/syntax_highlighting/stl/emacs")
(require 'stl-mode)
```

### With `use-package`

```elisp
(use-package stl-mode
  :load-path "/path/to/resources/syntax_highlighting/stl/emacs"
  :mode "\\.stl\\'")
```

The mode auto-associates with `*.stl` via `auto-mode-alist`.

## Features

- `font-lock` highlighting for all STL keywords, member declarations
  (`var`/`fn`/`event`), primitive types, attribute vocabulary, string /
  numeric / boolean literals, and the reserved SQL-like query keywords.
- `// ...` comments with `M-;` / `comment-dwim` support.
- 2-space indentation with Allman-brace awareness.
- `@param name` is highlighted inside documentation comments.
