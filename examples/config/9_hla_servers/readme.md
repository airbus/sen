# HLA Servers Examples

> **Prerequisites:** [3 - Aircraft](../3_aircraft/readme.md) (HLA concepts and request/response patterns).

Here we implement a terrain server (and a client) and a weather server.

## Terrain Server

### Interface

The terrain server interface is defined like this:

```rust title="terrain_server.stl"
--8<-- "snippets/examples/packages/terrain_server/stl/terrain_server.stl"
```

The main functions are `requestLos` and `requestHatHot`.

This class uses the types defined in the following STL file:

```rust title="basic_types.stl"
--8<-- "snippets/examples/packages/terrain_server/stl/basic_types.stl"
```

### Server Implementation

This terrain server implementation provides answers with random values.

```c++ title="terrain_server_impl.cpp"
--8<-- "snippets/examples/packages/terrain_server/src/terrain_server_impl.cpp"
```

### Client Implementation

This terrain client implementation looks for all the servers and continuously requests information.

```c++ title="terrain_server_impl.cpp"
--8<-- "snippets/examples/packages/terrain_server/src/terrain_client_impl.cpp"
```

### How to run it

In one terminal:

```
sen run config/9_hla_servers/1_terrain_client.yaml
```

In another terminal:

```
sen run config/9_hla_servers/1_terrain_server.yaml
```

You should see how the client detects the server and prints the query results.

## Weather Server

### Interface

The weather server interface is structured similarly to the terrain example:

```rust title="weather_server.stl"
--8<-- "snippets/examples/packages/weather_server/stl/weather_server.stl"
```

### Implementation

The implementation is also very simple. We generate random data as well, but we need to take
care of more object types and handle UUIDs.

```c++ title="weather_server.cpp"
--8<-- "snippets/examples/packages/weather_server/src/weather_server.cpp"
```

### How to run it

```
sen run config/9_hla_servers/3_weather_server.yaml
```

In this case you need to populate the parameters in a more involved manner due to the nature of the
data model:

```
my.tutorial.weatherServer.reqWeather {"type": "GeodeticLocation", "value": { "lat": 0, "lon": 0}}, false
```
