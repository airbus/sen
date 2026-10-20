#!/bin/bash

# =============================================================================================================
#                                               Sen Infrastructure
#                                        Copyright Airbus Defence and Space
# =============================================================================================================

docker run -d \
  -p 3001:3000 \
  -p 8095:8094 \
  -p 8095:8094/udp \
  -p 8186:8186 \
  -e GF_SECURITY_ADMIN_PASSWORD=admin \
  -v $PWD/grafana:/usr/share/grafana/data \
  -v $PWD/config/telegraf.conf:/etc/telegraf/telegraf.conf \
  --name datavis \
  grafana
