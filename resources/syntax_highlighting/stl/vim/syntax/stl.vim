" Vim syntax file
" Language:     Sen STL (Sen Type Language)
" Maintainer:   Sen project

if exists("b:current_syntax")
  finish
endif

" --- Comments ---------------------------------------------------------------
syn keyword stlTodo             contained TODO FIXME XXX NOTE
syn match   stlLineComment      "//.*$" contains=stlTodo,stlDocParam
syn match   stlDocParam         contained "@param\s\+\S\+"

" --- Storage types (declaration openers) -----------------------------------
syn keyword stlStorageType      class struct enum variant interface
syn keyword stlStorageType      sequence array optional quantity alias

" --- Class members ---------------------------------------------------------
syn keyword stlMemberKeyword    var fn event

" --- Control-flow-ish keywords ---------------------------------------------
syn keyword stlControl          package import extends implements abstract

" --- Attributes / modifiers (only inside [...]) ----------------------------
syn keyword stlAttribute        contained static static_no_config writable
syn keyword stlAttribute        contained confirmed bestEffort const local
syn keyword stlAttribute        contained tag min max

" --- Built-in types --------------------------------------------------------
syn keyword stlType             u8 u16 u32 u64 i16 i32 i64 f32 f64 bool
syn keyword stlType             string TimeStamp Duration

" --- Boolean literals ------------------------------------------------------
syn keyword stlBoolean          true false

" --- SQL-like query keywords (reserved in STL) -----------------------------
syn keyword stlQueryKeyword     SELECT FROM WHERE BETWEEN IN NOT AND OR

" --- Strings ---------------------------------------------------------------
syn region  stlString           start=+"+ end=+"+ oneline
syn region  stlString           start=+'+ end=+'+ oneline

" --- Numbers ---------------------------------------------------------------
syn match   stlFloat            "-\?\<[0-9]\+\.[0-9]\+\>"
syn match   stlInteger          "-\?\<[0-9]\+\>"

" --- Type/identifier names -------------------------------------------------
syn match   stlTypeName         "\<[A-Z][A-Za-z0-9_]*\>"

" --- Attribute brackets ----------------------------------------------------
syn region  stlAttrBracket      matchgroup=stlBracket start="\[" end="\]"
      \ contains=stlAttribute,stlInteger,stlFloat,stlString,stlBoolean,stlTypeName

" --- Operators -------------------------------------------------------------
syn match   stlArrow            "->"

" --- Highlight groups ------------------------------------------------------
hi def link stlTodo             Todo
hi def link stlLineComment      Comment
hi def link stlDocParam         SpecialComment
hi def link stlStorageType      Structure
hi def link stlMemberKeyword    Keyword
hi def link stlControl          Include
hi def link stlAttribute        StorageClass
hi def link stlType             Type
hi def link stlBoolean          Boolean
hi def link stlQueryKeyword     PreProc
hi def link stlString           String
hi def link stlFloat            Float
hi def link stlInteger          Number
hi def link stlTypeName         Type
hi def link stlBracket          Delimiter
hi def link stlArrow            Operator

let b:current_syntax = "stl"
