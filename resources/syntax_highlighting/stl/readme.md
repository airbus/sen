# Editor support for Sen STL

This directory ships syntax highlighting, indentation, bracket matching, and
snippets for the Sen Type Language (`.stl`) across the five editors we use
most:

| Editor                 | Folder                                   | What is provided                                                                                    |
|------------------------|------------------------------------------|-----------------------------------------------------------------------------------------------------|
| **Visual Studio Code** | [`vscode/`](vscode/)                     | Extension with TextMate grammar, language-configuration, and snippets.                              |
| **CLion / IntelliJ**   | [`clion/`](clion/)                       | Instructions for reusing the TextMate grammar as a TextMate bundle, or defining a native file type. |
| **Vim / Neovim**       | [`vim/`](vim/)                           | `syntax/`, `ftdetect/`, `ftplugin/`, `indent/` scripts.                                             |
| **Emacs**              | [`emacs/`](emacs/)                       | `stl-mode.el` major mode with font-lock and 2-space indent.                                         |
| **Notepad++**          | [`notepad_plusplus/`](notepad_plusplus/) | UDL XML importable through *Language → User Defined Language → Define your language...*             |

Each subdirectory has its own `readme.md` with editor-specific install
instructions.

## What the highlighters recognize

Every highlighter in this directory covers the same language surface:

- **Declaration openers** — `class`, `struct`, `enum`, `variant`, `interface`,
  `sequence`, `array`, `optional`, `quantity`, `alias`.
- **Member keywords** — `var`, `fn`, `event`.
- **Control / inheritance** — `package`, `import`, `extends`, `implements`,
  `abstract`.
- **Attribute vocabulary** — `static`, `static_no_config`, `writable`,
  `confirmed`, `bestEffort`, `const`, `local`, `tag`, `min`, `max`.
- **Built-in types** — `u8`..`u64`, `i16`..`i64`, `f32`, `f64`, `bool`,
  `string`, `TimeStamp`, `Duration`.
- **Reserved query keywords** — `SELECT`, `FROM`, `WHERE`, `BETWEEN`, `IN`,
  `NOT`, `AND`, `OR` (these belong to Sen's object-query sub-language but are
  tokenized alongside STL and cannot be used as identifiers).
- **Literals** — decimal integer and floating-point numbers, booleans,
  double- and single-quoted strings.
- **Line comments** — `//` (block comments are not part of STL).
- **Method return arrow** — `->`.
