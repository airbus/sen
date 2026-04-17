# Web Explorer for Sen

This web explorer demonstrates how to use the Sen REST API from a browser environment. It provides
real-time updates through Server-Sent Events (SSE).

## Prerequisites

- Docker
- A running Sen REST component

## Example structure

```
web_explorer/
├── index.html          # Web page
├── sen_client.js       # JavaScript client
├── nginx.conf          # nginx configuration
├── run_server.sh       # shell script to run nginx Docker container
└── README.md           # This file
```

## How to run

Create a **sen** configuration file (e.g. config.yaml) to load the REST component:

```yaml
load:
  - name: rest
    group: 3
    address: "0.0.0.0"
    port: 8080
```

Note: run_server.sh script expects REST component opens on port 8080.

Now start **sen** and the web server as follows:

```bash
# Run sen with previous configuration
sen run config.yaml

# Run with default port (8000)
./run_server.sh
```

Open the browser (http://localhost:8000) and developer console to see:

- API request/response details
- SSE event streams
- Error messages
- Object interaction logs
