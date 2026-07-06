# Web Explorer for Sen

This web explorer demonstrates how to use the Sen REST API from a browser environment. It provides
real-time updates through Server-Sent Events (SSE).

## Prerequisites

---

- Docker engine - make sure to follow the [installation guide](https://docs.docker.com/engine/install/)
  and the [post-installation steps](https://docs.docker.com/engine/install/linux-postinstall/)
- A running Sen REST component

## Example structure

---

```
web_explorer/
├── index.html          # Web page
├── sen_client.js       # JavaScript client
├── nginx.conf          # nginx configuration
├── run_server.sh       # shell script to run nginx Docker container
└── README.md           # This file
```

## How to run

---

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

## Usage

### Start session

First of all, start session with the Client ID of choice and click `Start Session`.
This step is needed in order to interact with the component.

Next to `Start Session` button there is a checkbox with the name `SSE`. This checkbox enables the browser to receive
push notifications from the REST component when Sen objects are updated.

### Create interest

To create an interest, fill both fields in the `Interest` section. The first field corresponds to the interest name and
the second field is for the interest query. The interest query must be written on Sen Query Language. There is a section
called `Sessions` which shows all the available sessions.

Interests are saved until the Sen REST component is ended. To show all the created interests, click `Reload` on the
`Available Interests` section.

NOTE: To show newly created interests after `Reload` has already been clicked, refresh the page and click `Reload`
again.

### Retrieve objects

Once an interest is created, click `Retrieve objects` in the `Available Interests` section to show all the objects in
that interest.

The `Objects` section is where object elements will be shown. These include methods, properties and events. To do that,
click
`Get Object`. This will show all the object elements classified by type.

For each type of element, different options are available:

- In `Object Methods` you can get every method definition and invoke them.
  You can do that by clicking `Definition`
  and `Invoke` buttons respectively. For invoking a method, the arguments must be defined in the `Invoke Method Args`
  section.
- In `Object Properties` you can get every property value and subscribe or unsubscribe to them. To do that, click
  `Get Value`, `Subscribe` and `Unsubscribe` buttons respectively.
- In `Object Events` you can subscribe or unsubscribe to an event by clicking `Subscribe` and `Unsubscribe` buttons
  respectively.
