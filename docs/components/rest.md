![Screenshot](../assets/images/rest_light.svg#only-light){: style="width:250px; float: right;"}
![Screenshot](../assets/images/rest_dark.svg#only-dark){: style="width:250px; float: right;"}

# The REST API Component

This component allows interaction with Sen sessions, interests, and objects through a REST API with
JWT authentication.

## What is a REST API?

A REST API is a specific type of API that follows the Representational State Transfer (REST)
architecture style, promoting stateless communication over HTTP.

## Key Concepts

### Authentication

The REST API uses JWT (JSON Web Tokens) for authentication. Most endpoints require a valid bearer
token obtained from the `/api/auth` endpoint. The token must be included in the `Authorization`
header of subsequent requests.

### Interests

An **interest** is a named query that selects a set of Sen objects using Sen Query Language. Instead
of directly accessing sessions and buses, clients create interests to specify which objects they
want to interact with. For example:

```sql
SELECT * FROM local.kernel
```

This allows for flexible object discovery and filtering.

### Objects, Methods, Properties, and Events

Once you have an interest, you can:

- **List objects** matching the interest query
- **Get object details** including available methods, properties, and events
- **Invoke methods** on objects (synchronous for getters, asynchronous for setters and regular
  methods)
- **Read and subscribe to properties** to receive updates when values change
- **Subscribe to events** to receive notifications when events are emitted

### Type Introspection

The API provides type introspection through the `/api/types/{type}` endpoint, allowing you to
discover the structure of custom Sen types at runtime.

## HTTP Server

This component implements its own HTTP/1.1 server on top of Asio TCP. It does not rely on any other
third-party libraries in order to keep dependencies low and uses HTTP/1.1 to keep the implementation
minimal.

The main features of this server are:

- URL path–based routing with parameter support: For example, a route `/api/session/:session` would
  parse `:session` as an alphanumeric string and pass it to the handler of that route.
- Support for HTTP methods GET, POST, PUT, DELETE
- Limited CORS support: CORS is enabled by default, and it is not currently configurable.
- Server-Sent Events (SSE) support: Used for sending push notifications to clients.

As a side note, WebSockets are not supported since they are not required for Sen’s REST API; only
**SSE** is implemented for push notifications.

## Getting Started

To start this component, you will need to configure the following parameters:

- `address`: The IP address or hostname the server will bind to
- `port`: The port number the server will listen on

For example, the following configuration binds the server to all network interfaces on port 8080:

```yaml
load:
  - name: rest
    group: 3
    address: "0.0.0.0"
    port: 8080
```

### Example Using cURL

#### 1. Authentication

Most endpoints require authentication. First, authenticate to get a JWT token:

```shell
curl -X POST http://localhost:8080/api/auth \
  -H "Content-Type: application/json" \
  -d '{"id": "my-client"}'
```

Expected response:

```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

Save the token for subsequent requests. You'll need to include it in the `Authorization` header:

```shell
export TOKEN="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
```

#### 2. List Available Sessions

You can list all available Sen sessions (no authentication required):

```shell
curl http://localhost:8080/api/sessions
```

Expected response:

```json
["local"]
```

#### 3. Create an Interest

Create an interest to select objects using a query:

```shell
curl -X POST http://localhost:8080/api/interests \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{
    "name": "my-interest",
    "query": "SELECT * FROM local.kernel"
  }'
```

Expected response:

```json
{
  "name": "my-interest"
}
```

#### 4. List Interests

Get all interests for the authenticated client:

```shell
curl http://localhost:8080/api/interests \
  -H "Authorization: Bearer $TOKEN"
```

#### 5. List Objects in an Interest

Get all objects matching the interest query:

```shell
curl http://localhost:8080/api/interests/my-interest/objects \
  -H "Authorization: Bearer $TOKEN"
```

This returns an array of object summaries with links to detailed information.

#### 6. Get Object Details

Get detailed information about a specific object:

```shell
curl http://localhost:8080/api/interests/my-interest/objects/OBJECT_NAME \
  -H "Authorization: Bearer $TOKEN"
```

The response includes available methods, properties, and events with links to invoke/subscribe.

For more details on all available endpoints, check the
[OpenAPI documentation](#openapi-documentation).

### Push Notifications

The `rest` component provides real-time notifications through Server-Sent Events (SSE). Clients can
receive live updates for subscribed events, property changes, method invocations, and object
additions/removals through a persistent connection.

To receive push notifications, make a **GET** request to the `/api/sse` endpoint with
authentication:

```shell
curl http://localhost:8080/api/sse \
  -H "Authorization: Bearer $TOKEN"
```

The connection will remain open, and you'll receive notifications in real-time for:

- **Events**: Object events you've subscribed to
- **Properties**: Property changes you've subscribed to
- **Invokes**: Method invocation status updates
- **Objects**: Object added/removed notifications

Before receiving notifications, you must subscribe to the specific events or properties you're
interested in. For more details on how to subscribe to events or properties, check the
[OpenAPI documentation](#openapi-documentation).

## Updating the OpenAPI Specification

The OpenAPI specification must be validated whenever updates are made, such as when adding a new
feature.

First, install `swagger-cli`:

```shell
npm i -g swagger-cli
```

Then use the `validate-openapi` CMake target to check its validity:

```shell
cmake --build build/Debug --target validate-openapi
```

## OpenAPI Documentation

<redoc src="openapi.yaml" />
