# Sen STL — VSCode extension

Syntax highlighting, indentation, bracket matching, and snippets for Sen STL
(`.stl`) files.

## Features

- Highlighting for all STL constructs (`class`, `struct`, `enum`, `variant`,
  `interface`, `sequence`, `array`, `optional`, `quantity`, `alias`), their
  members (`var`, `fn`, `event`), and their attribute vocabulary
  (`static`, `writable`, `confirmed`, `bestEffort`, `const`, `local`, etc.).
- Highlighting for the query-language keywords (`SELECT`, `FROM`, `WHERE`, ...)
  that are reserved in STL.
- Bracket matching and auto-close for `{}`, `[]`, `()`, `<>`, and strings.
- Indentation rules matching Sen's Allman / 2-space style.
- Snippets for every declaration kind (see `snippets/stl.code-snippets`).

## Installation

### Option 1: install locally (recommended while this ships with the Sen repo)

Copy the directory into your VSCode extensions folder:

```bash
# macOS / Linux
cp -r resources/syntax_highlighting/stl/vscode ~/.vscode/extensions/sen-stl-0.1.0

# Windows
xcopy /E /I resources\syntax_highlighting\stl\vscode %USERPROFILE%\.vscode\extensions\sen-stl-0.1.0
```

Then restart VSCode.

### Option 2: build a `.vsix`

```bash
cd resources/syntax_highlighting/stl/vscode
npm install -g @vscode/vsce
vsce package
code --install-extension sen-stl-0.1.0.vsix
```

## Theme pairing

The grammar uses standard TextMate scope names (`storage.type`,
`entity.name.type.class`, `keyword.control`, etc.), so any VSCode colour
theme will style STL automatically.

## Reporting issues

File issues in the main Sen repository.
