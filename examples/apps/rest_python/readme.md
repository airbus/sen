# Python client example for the Sen REST component

This example demonstrate how to make requests to the REST component using Python.

## Prerequisites

- venv
- python3

## How to run

Create a **sen** configuration file (e.g. config.yaml) to load the REST component:

```yaml
load:
  - name: rest
    group: 3
    address: "0.0.0.0"
    port: 8080
```

Now start **sen** with your new configuration:

```bash
sen run config.yaml
```

Prepare the Python environment

```bash
python3 -m venv .env
source .env/bin/activate
pip install -r requirements.txt
```

Now run the client example:

```bash
python sen_client.py
```

This example works as a simple python client for the Sen REST component. It instantiates a SenClient object that starts
a session, enable notifications and does some basic operations to exemplify how it can be used.
