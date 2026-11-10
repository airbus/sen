# Python REST Client example

> **Prerequisites:** Familiarity with the [REST API Component](../../../../components/rest.md). Python 3 installed.

This example demonstrates how to interact with the Sen REST API using Python. Building a proper client in Python allows you to programmatically authenticate, create interests, discover objects, invoke methods, and easily handle real-time Server-Sent Events (SSE) push notifications.

## The Python Client

The example consists of a Python script `sen_client.py` that implements a `SenClient` class. This class uses the `requests` library for standard HTTP requests and `sseclient` for handling the persistent Server-Sent Events connection in a background thread.

=== "_sen_client.py_"

    ```python
    --8<-- "snippets/examples/apps/rest_python/sen_client.py"
    ```

=== "_requirements.txt_"

    ```text
    --8<-- "snippets/examples/apps/rest_python/requirements.txt"
    ```

## How to run it

First, create a basic Sen configuration that loads the `rest` component on port `8080`.

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

Now, open a new terminal window to prepare the Python environment and run the client:

```shell


python3 -m venv .env
source .env/bin/activate
pip install -r requirements.txt
python sen_client.py
```

## What happens under the hood

When you execute `sen_client.py`, the script performs the following sequence of actions:

1. **Authentication:** It connects to `/api/auth` with a client ID (`my-client-id`) and retrieves a JWT token. This token is automatically attached to the `Authorization` header for all subsequent requests.
2. **SSE Connection:** It spawns a background thread that connects to `/api/sse`. This thread listens for push notifications (like `object_added` or `invoke` results) and prints them to the console in real-time.
3. **Session Discovery:** It queries `/api/sessions` to list available Sen sessions.
4. **Creating an Interest:** It creates an interest named `new_interest` with the query `SELECT * FROM local.kernel`. This tells the REST API to track all objects matching that SQL query.
5. **Object Introspection:** It lists the objects matching the interest, then requests detailed information for a specific object (the `api` object), including its available methods, properties, and events.
6. **Method Invocation:** Finally, it invokes the `getConfig` method on the `api` object and prints the asynchronous invocation result (which is also broadcasted via SSE).
