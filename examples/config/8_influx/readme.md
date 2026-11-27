# Influx component example

> **Prerequisites:** [4 - School](../4_school/readme.md) (the data source used here). Docker must be
> installed and running.

Here you will find a container that hosts Grafana, InfluxDB and Telegraf. Your Sen instance will
connect to Telegraf and you can then connect to Grafana to see the data.

## 1. Build and start the container

```shell
cd config/8_influx/docker
./build.sh
./start.sh
```

## 2. Look at the data

Open `http://localhost:3001` and log in with **admin** / **admin**.

The InfluxDB data source and a dashboard named **School** are already there: the container carries
them as Grafana provisioning files, so there is nothing to type in. Open Dashboards and then
School, and start the Sen instance in a terminal:

```shell
sen run config/8_influx/1_influx_school.yaml
```

The panel queries `school.Student.brainActivity`, grouped by the `object` tag, over the last
minute. Data appears within a few seconds of the Sen process starting.

If you would rather build it by hand, the provisioning files say what to enter:
`docker/config/grafana/datasources/influxdb.yaml` holds the data source, and
`docker/config/grafana/dashboards/school.json` the dashboard's query and time range.

## 3. Change how Sen reaches Telegraf

`1_influx_school.yaml` sends line protocol over TCP. The `transport` option takes any of the
schemes Telegraf's socket listener accepts, and both sides have to name the same one.
`docker/config/telegraf.conf` holds the TCP listener this example uses, plus a commented line for
gzip and a commented block for HTTP. Any other scheme means editing `service_address` there
yourself — `udp://:8094` for a datagram listener, say — and changing the Sen side to match.
`start.sh` mounts that file into the container, so `docker restart datavis` picks up an edit; the
image does not need rebuilding.

Compression, which needs `content_encoding = "gzip"` on the listener:

```yaml
    compression:
      type: GZip
      value:
        level: 6
```

HTTP, which needs the `[[inputs.influxdb_listener]]` block instead of the socket listener. It is
the only option here that reports whether a batch was accepted:

```yaml
    transport:
      type: Http
      value:
        address: 127.0.0.1
        port: 8186
        path: /write
        maxMessageSize: 0
```

`type: Unix` and `type: UnixGram` reach a Telegraf over a socket file instead of the network
stack, but not this one: it runs in a container, and `start.sh` shares no directory a socket could
live in. They are for a Telegraf running directly on the host.

Then build your own dashboards: add visualizations, pick other measurements, and experiment with
Grafana's query editor.
