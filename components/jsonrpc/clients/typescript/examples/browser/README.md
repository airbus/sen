# Browser smoke

Static HTML page that loads the built `@sen/client` bundle in a real browser, connects to a
Sen subprocess over WebSocket, and runs the top-level [README](../../README.md)'s hello-world
against the actual `dist/` artifact.

## Run

```bash
# 1. Build the bundle.
cd /workspace/components/jsonrpc/clients/typescript
npm run build

# 2. Start Sen on the bundled animals config (port 8080).
/workspace/build/gcc/Release/build/gcc/Release/bin/sen \
  run /workspace/components/jsonrpc/clients/typescript/examples/browser/animals.yaml &

# 3. Serve the package (port 8000) so the page can import dist/index.js.
python3 -m http.server 8000
```

Open `http://localhost:8000/examples/browser/`. You should see:

```
connecting to ws://127.0.0.1:8080...
connected.
got animals.Cat named rufus
position: x=0 m, y=0 m
position: x=42 m, y=17 m
done.
```

An `<script type="importmap">` aliases `@sen/client` to the bundle path so the page can
`import { connect } from "@sen/client"` as a real consumer would.
