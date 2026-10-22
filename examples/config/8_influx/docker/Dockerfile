# ======================================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

FROM ubuntu:20.04 AS base

# tell debconf that there's no user interacting
ENV DEBIAN_FRONTEND=noninteractive

# Labels / Metadata.
LABEL decription="Grafana data viewer environment"

# install required packages
RUN apt update && apt-get install -y --no-install-recommends -o=Dpkg::Use-Pty=0 wget adduser libfontconfig1 musl supervisor apt-transport-https wget gnupg ca-certificates

# install telegraf
RUN wget -q https://repos.influxdata.com/influxdata-archive.key
RUN gpg --show-keys --with-fingerprint --with-colons ./influxdata-archive.key 2>&1 | grep -q '^fpr:\+24C975CBA61A024EE1B631787C3D57159FC2F927:$' && cat influxdata-archive.key | gpg --dearmor | tee /etc/apt/trusted.gpg.d/influxdata-archive.gpg > /dev/null
RUN echo 'deb [signed-by=/etc/apt/trusted.gpg.d/influxdata-archive.gpg] https://repos.influxdata.com/debian stable main' | tee /etc/apt/sources.list.d/influxdata.list
RUN apt-get update && apt-get install -y telegraf

# install influx
RUN apt-get install -y influxdb

# install grafana
RUN mkdir -p /etc/apt/keyrings/
RUN wget -q -O - https://apt.grafana.com/gpg.key | gpg --dearmor | tee /etc/apt/keyrings/grafana.gpg > /dev/null
RUN echo "deb [signed-by=/etc/apt/keyrings/grafana.gpg] https://apt.grafana.com stable main" | tee -a /etc/apt/sources.list.d/grafana.list
RUN apt-get update && apt-get install -y grafana

# add configuration
COPY config/telegraf.conf /etc/telegraf/telegraf.conf
COPY config/supervisord.conf /etc/supervisor/conf.d/supervisord.conf

# expose the necessary ports to make it work
EXPOSE 8086 3000 8094

# start all three services
CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/conf.d/supervisord.conf"]
