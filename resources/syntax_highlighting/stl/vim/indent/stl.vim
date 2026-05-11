" Vim indent file
" Language:     Sen STL (Sen Type Language)
" Maintainer:   Sen project

if exists("b:did_indent")
  finish
endif
let b:did_indent = 1

setlocal expandtab
setlocal shiftwidth=2
setlocal softtabstop=2
setlocal autoindent
setlocal indentexpr=GetStlIndent()
setlocal indentkeys=0{,0},0),!^F,o,O,e

if exists("*GetStlIndent")
  finish
endif

function! GetStlIndent()
  let lnum = prevnonblank(v:lnum - 1)
  if lnum == 0
    return 0
  endif

  let prev = getline(lnum)
  let curr = getline(v:lnum)
  let indent = indent(lnum)
  let sw = shiftwidth()

  " Previous line opens a new block -> indent one level deeper
  if prev =~ '{\s*\(//.*\)\?$'
    let indent += sw
  endif

  " Current line starts with a closing brace -> dedent
  if curr =~ '^\s*}'
    let indent -= sw
  endif

  return indent < 0 ? 0 : indent
endfunction
