# Influx component example

Here you will find a container that hosts Grafana, InfluxDB and Telegraf. Your Sen instance will
connect to Telegraf and you can then connect to Grafana to see the data.

## 1. Build and start the container

```shell
cd config/8_influx/docker
./build.sh
./start.sh
```

## 2. Log into Grafana and set the password.

Upon deploying the Docker containers for **Grafana, InfluxDB, and Telegraf**, you'll need to
configure the connection using Grafana's interface. This is a one-time setup.

Open your web browser and navigate to 'http://localhost:3001' (or the appropriate URL for your
deployment).

Log-in using the default credentials:

- Username: **'admin'**
- Password: **'admin'**

After logging-in, you'll be prompted to change your password. Follow the on-screen instructions to
set a new password.

## 3. Add new data source

1. In the left sidebar, click on "Connections" -> "Add new connection".
2. Select "InfluxDB" from the list and click on "Add new data source"
3. In the "HTTP" section, fill in the details as follows: URL: 'http://127.0.0.1:8086'
4. In the **"InfluxDB Details"** section, fill the database wih "influx"
5. Click "Save & Test" to save the data source configuration.
6. In a terminal, start the Sen instance with `sen run config/8_influx/1_influx_school_exp.yaml`

Finally, you will need to build your dashboard.

1. Go to Home->Dashboards and click on Create Dashboard
2. Click on "Add Visualization"
3. Select InfluxDB as the datasource
4. Configure your query by selecting the measurement to be school.Student.brainActivity
5. In the "Group by", use "object:tag" (remove any other criteria)
6. In the time range panel: in the "from" box enter now-1m. In the "to" box enter now.
7. Save the dashboard
8. Play around with other parameters and settings. Create more dashboards and experiment with
   Grafana.
