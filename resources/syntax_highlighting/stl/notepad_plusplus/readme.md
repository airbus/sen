# Sen STL — Notepad++ User Defined Language

`stl.xml` is a Notepad++ UDL-2.1 definition for Sen STL (`.stl`) files.

## Installation

1. Open Notepad++.
2. Menu: **Language → User Defined Language → Define your language...**
3. Click **Import...** and select `stl.xml`.
4. Close the dialog and re-open any `.stl` file (or pick **Language → Sen STL**
   from the menu).

## Keyword groups and colours

| Group | Category              | Colour (default) |
| ----- | --------------------- | ---------------- |
| 1     | Declaration openers   | Blue, bold       |
| 2     | Member keywords       | Purple, bold     |
| 3     | Control / inheritance | Green, bold      |
| 4     | Attribute vocabulary  | Brown            |
| 5     | Built-in types        | Light blue       |
| 6     | Boolean literals      | Red              |
| 7     | Query keywords (SQL)  | Dark red, bold   |

Adjust the colours by editing the corresponding `<WordsStyle>` entries in
`stl.xml` before importing.
