" Filetype plugin for Sen STL

if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

setlocal commentstring=//\ %s
setlocal comments=:\/\/
setlocal formatoptions-=t
setlocal formatoptions+=croql
