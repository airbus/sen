# REST Component Architecture

## Introduction

The REST component is an HTTP server that serves a REST API to clients such as web browsers, curl,
or any other HTTP client. The REST API is implemented as a custom HTTP router that handles requests
to access Sen resources including interests, objects and sessions.

This document serves as a design reference explaining the internal workings of the component while
providing a high-level description of its architecture. We assume the reader is familiar with the
HTTP protocol and REST APIs.

## Files and Directory Structure

This component is organized with a flat directory structure as follows:

```
[components/rest]
├── src/                # Component source code. Entry point is in component.cpp
├── config/             # Example configurations to start a Sen runtime with the REST component
├── test/               # Unit and end-to-end (E2E) tests. E2E tests can serve as integration tests
├── stl/                # STL types used by this component
└── architecture.md     # This document
```

Usage documentation and OpenAPI specification can be found at:

```
[docs/components]
├── rest.md            # Introduction and usage documentation
└── openapi.yaml       # OpenAPI specification
```

## Core Components

The core components and their interactions are:

### RestAPIComponent

The entry point of the Sen component, implementing the `sen::kernel::Component` interface. Its main
responsibility is to handle the initialization, startup, and shutdown of the HttpServer.

### HttpServer

Implements a simplified asynchronous HTTP/1.0 server with minimal dependencies. The server lifecycle
is intentionally simple: it can be started and bound to an endpoint (e.g., 127.0.0.1:8080) with an
attached router. Once started, it handles incoming HTTP requests and routes them to registered
handlers.

### BaseRouter

HttpServer and BaseRouter are closely related. While HttpServer handles the HTTP/1.0 server
functionality, BaseRouter routes all incoming HTTP requests to their corresponding paths.

BaseRouter is designed to be extended, allowing end users to implement various REST APIs. To add new
routes, implementors call `BaseRouter::addRoute(...)` for a given path and HTTP method.

### SenRouter

SenRouter implements and extends the BaseRouter interface, configuring URL paths for different HTTP
methods to access Sen resources. As an introductory starting point, examine the SenRouter
constructor, where you'll find several `addRoute` calls that attach handlers to paths. For example:

```cpp
addRoute(HttpMethod::httpGet, "/api/sessions", bindRouteCallback(this, &SenRouter::getSessionsHandler));
```

This `addRoute` call attaches the `getSessionsHandler` to the `/api/sessions` path for the HTTP GET
method.

### ClientSession

ClientSession abstracts the concept of a Sen session. When a client requests authorization via the
`/api/auth` endpoint, it receives a session token that must be included as an HTTP header in
subsequent requests (see the OpenAPI specification for details). This session token identifies a
specific ClientSession instance, which manages all calls to Sen resources without interfering with
other clients' state.

### ObjectInterestsManager, ObjectMembersManager, and ObjectInvokesManager

Each ClientSession is composed of these three managers that implement operations for interacting
with basic Sen primitives: interests, object members (properties and events), and object methods.
These operations are not reentrant, and ClientSession is responsible for calling them in a
thread-safe manner.

These classes also extend Notifier to emit notifications to observers when manager operations occur.

### Notifier

Notifier is designed to be extended by classes that need to notify Sen of internal events such as:

- Property updates
- Object events
- Objects added to or removed from a Sen session
- Object method invocation status and results

Classes extending Notifier act as publishers of notifications, and other classes can subscribe (or
observe, in the terminology used in this implementation) to these notifications by acquiring an
ObserverGuard.

### ObserverGuard

ObserverGuard instances are created by a Notifier. For example, ObjectInterestsManager extends the
Notifier class and asynchronously publishes interest-related notifications to observers. In the
implementation, you can see how SenRouter acquires observer guards, making it an observer of certain
notifications.

ObserverGuard is a RAII class that automatically deallocates itself when it goes out of scope.

Its main purpose is to serve server-sent-event notifications in the HTTP server (more details
below).

## External Dependencies

### llhttp

The REST component adds [llhttp](https://llhttp.org/) as the only new dependency to the current Sen
dependency tree. **llhttp** is a well-proven, MIT-licensed HTTP parser used by the **Node.js**
project to serve countless applications.

### asio

**Asio** is the other main dependency, already included in the Sen dependency tree. The REST
component is built primarily on top of **Asio** for the TCP server and **llhttp** for HTTP parsing
and serving.

## Considerations

### Server-Sent Events

We explained how the notification mechanism works. It's a powerful way to push notifications to
clients like web browsers. Modern browsers support WebSockets but also the less widely known
[Server-Sent Events (SSE)](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events). We
chose to support only SSE instead of WebSockets for simplicity, what is crucial for reducing the
error surface of the implementation. Where webSockets enable two-way communication (server and
client exchange messages), SSE provides one-way communication (server sends events to clients) and
it is compatible with almost all
[web browsers](https://developer.mozilla.org/en-US/docs/Web/API/EventSource#browser_compatibility).
See the OpenAPI specification for details on using the SSE endpoint.

### Load Balancing

A load-balanced scenario helps not only for distributing load but also for having multiple REST
components serve clients to avoid a single point of failure. However, the current implementation
doesn't support this scenario due to the nature of client sessions (see the ClientSession class).
Session tokens identify each session but are not shared between servers. To support load balancing,
tokens would need to be stored in a central repository (e.g., Redis or etcd), and load balancers
would need to route requests to servers where the session data persists. Another strategy would be
to make REST components stateless, storing all client interest subscriptions in session data so that
in the event of a crash, another REST component could restore the interests, properties, and event
subscriptions. We note this as future work.

### HTTP Server

HTTP servers typically follow a request-reply pattern, meaning multiple requests occur throughout
the server's lifespan. To support this, the REST component leverages
[Asio](https://think-async.com/Asio/) for asynchronicity. Resource deallocation and thread safety
are critical, and Asio is a well-known, proven implementation. However, we simplified Sen resource
usage through ClientSession, which locks the execution thread for each client reply. A lock-free
implementation is left for future improvements, but for safety reasons and because this server is
not designed to handle thousands of concurrent clients, we chose this more conservative approach.

Another important consideration is our rationale for implementing a custom HTTP server. We wanted
something easy to debug, built on proven and well-maintained foundations, and suitable for the
current codebase. We evaluated several libraries and found them incompatible with our criteria:

- **pistache**: Large codebase with several unresolved crash issues on GitHub. Also includes its own
  async library instead of leveraging Asio.
- **restbed**: Mature but dual-licensed under AGPL3 and a commercial license.
- **oatpp**: Zero-dependency full web framework with a large, highly opinionated codebase.

On the other hand, **llhttp** is extensively used behind the scenes in **Node.js**, and **asio** is
simple, well-known, and already used in the Sen codebase. Given these considerations and the fact
that we didn't need a complete web framework, we decided to implement a custom server.
