# Sen STL — CLion (and other IntelliJ IDEs)

CLion, IntelliJ IDEA, Rider, PyCharm and the rest of the JetBrains family can
highlight Sen STL files through either (a) a **file-type + TextMate bundle**
combination, or (b) a plain **file-type** definition if you only need
keyword / comment / bracket matching without full TextMate support.

## Option A — TextMate bundle (recommended)

This reuses the same grammar as the VSCode extension and gives you the full
highlighter.

### Setup

1. Open **Settings** → **Editor** → **TextMate Bundles**.
2. Click **+** and add the directory
   `resources/syntax_highlighting/stl/vscode`.
   > Yes, the VSCode subfolder. JetBrains IDEs read the TextMate grammar
   > (`syntaxes/stl.tmLanguage.json`) directly from it.
3. Make sure the new **Sen STL** entry is ticked, then **Apply**.

That's it. The `.stl` extension is picked up automatically from the bundle's
`package.json`, so you normally don't need to touch **File Types**.

### If an `.stl` file still opens as "Plain text"

1. Check the language indicator at the bottom-right of the editor when an
   `.stl` file is open. If it shows something other than *Sen STL*, click it
   and pick *Sen STL*.
2. If the menu doesn't list it, try **File → Invalidate Caches → Invalidate
   and Restart** — CLion sometimes needs a cache refresh before it picks up
   a newly-added bundle's file extensions.
3. As a last resort, register the extension manually:
   **Settings → Editor → File Types → Recognized File Types**, find
   **Sen STL**, and add `*.stl` to its **File name patterns**.

## Option B — Native file-type only

If you prefer not to enable TextMate:

1. Open **Settings** → **Editor** → **File Types**.
2. Click **+** under "Recognized File Types".
3. Fill the form:
   - **Name:** `Sen STL`
   - **Description:** `Sen Type Language`
   - **Line comment:** `//`
   - **Keywords 1** (bold):
     `class struct enum variant interface sequence array optional quantity alias`
   - **Keywords 2**:
     `var fn event`
   - **Keywords 3**:
     `package import extends implements abstract`
   - **Keywords 4**:
     `static static_no_config writable confirmed bestEffort const local tag min max`
   - Check **Ignore case** → **off**.
   - Check **Support paired braces**, **Support paired brackets**,
     **Support paired parens**, **Support string escapes**.
4. Under **File name patterns**, add `*.stl`.
5. Apply.

This gives you keyword highlighting and bracket matching. For a richer
experience (quoted strings, numeric literals, query keywords, type-name
colouring), use Option A.

## Tips

- **Code Style** — set **Settings → Editor → Code Style → Other File Types**
  to *2 spaces*, *no tabs* to match Sen conventions.
- **Colors** — the IDE reuses your existing theme; no extra colour scheme is
  needed.
