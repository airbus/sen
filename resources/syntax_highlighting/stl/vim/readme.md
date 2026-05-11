# Sen STL — Vim / Neovim plugin

Provides filetype detection, syntax highlighting, and indentation for Sen STL
(`.stl`) files.

## Installation

### Manual

Copy the folder contents into your runtimepath:

```bash
# Vim
cp -r resources/syntax_highlighting/stl/vim/* ~/.vim/

# Neovim
cp -r resources/syntax_highlighting/stl/vim/* ~/.config/nvim/
```

### With a plugin manager

Point any plugin manager (vim-plug, lazy.nvim, packer, pathogen, ...) at the
`resources/syntax_highlighting/stl/vim` directory, or at a fork that contains
only this subtree.

## What is provided

- `ftdetect/stl.vim` — recognises `*.stl` files.
- `syntax/stl.vim` — syntax colouring (Structure / Keyword / Type / Include /
  StorageClass / Boolean / PreProc / Comment / Float / Number / String /
  Delimiter / Operator highlight groups).
- `indent/stl.vim` — 2-space, Allman-brace indentation.
- `ftplugin/stl.vim` — `commentstring=// %s` for `gc` / `comment.nvim` /
  `vim-commentary`.
