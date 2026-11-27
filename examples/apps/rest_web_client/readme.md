# JavaScript REST Client example

> **Prerequisites:** Familiarity with the [REST API Component](../../../../components/rest.md).
Docker Engine installed (see the [Docker installation guide](https://docs.docker.com/engine/install/) and
[Linux post-installation steps](https://docs.docker.com/engine/install/linux-postinstall/)).

This example demonstrates how to interact with the Sen REST API directly from a browser environment.
It provides a lightweight web interface built with vanilla JavaScript to authenticate, manage interests,
introspect objects, invoke methods, and receive real-time Server-Sent Events (SSE) push notifications.

## The Web Client

The example consists of a static frontend served via an `nginx` container that acts as a reverse proxy to the
Sen REST API:

* `index.html`: Provides the user interface to interact with sessions, interests, and objects.
* `sen_client.js`: Implements the HTTP requests and manages the `EventSource` connection for SSE.
* `nginx.conf`: Configures the web server and routes API requests to the Sen REST port.
* `run_server.sh`: Automates building and running the Docker container.

=== "_index.html_"

    ```html
    --8<-- "snippets/examples/apps/rest_web_client/index.html"
    ```

=== "_sen_client.js_"

    ```javascript
    --8<-- "snippets/examples/apps/rest_web_client/sen_client.js"
    ```

=== "_nginx.conf_"

    ```nginx
    --8<-- "snippets/examples/apps/rest_web_client/nginx.conf"
    ```

=== "_run_server.sh_"

    ```bash
    --8<-- "snippets/examples/apps/rest_web_client/run_server.sh"
    ```

## How to run it

First, create a basic Sen configuration that loads the `rest` component on port `8080`
(the default port expected by `run_server.sh`).

```yaml title="Configuration file"
load:
  - name: rest
    group: 3
    address: "0.0.0.0"
    port: 8080
```

Start the **sen** kernel with this configuration:

```shell
sen run config.yaml
```

Now, open a new terminal window and launch the web server container on default port `8000`:

```shell
./run_server.sh
```

Finally, open `http://localhost:8000` in your browser. You can open the browser's **Developer Console** to inspect raw
API requests, SSE event streams, and interaction logs.

## Usage workflow

Once the interface is loaded in your browser, follow these steps to interact with Sen:

1. **Start Session:** Enter a Client ID of your choice and click **Start Session** to authenticate
   and obtain a session token. Check the **SSE** box next to the button to subscribe to real-time
   push notifications.
2. **Create an Interest:** In the **Interest** section, provide an interest name and a valid Sen
   Query Language string (e.g., `SELECT * FROM local.kernel`). Click **Reload** under
   **Available Interests** to list active interests.
3. **Discover Objects:** Click **Retrieve objects** on an interest to list matching objects,
   then click **Get Object** to inspect its elements.
4. **Interact with Elements:**
   * **Methods:** Click **Definition** to view the signature, or enter parameters in
      **Invoke Method Args** and click **Invoke**.
   * **Properties:** Click **Get Value** for a one-time read, or **Subscribe** / **Unsubscribe**
      to track changes via SSE.
   * **Events:** Click **Subscribe** or **Unsubscribe** to listen to specific object events.

!!! note
    Interests persist until the Sen REST component stops. If you create new interests after already clicking **Reload**,
     refresh the browser page and click **Reload** again to update the list.
