#!/bin/bash

# =============================================================================================================
#                                               Sen Infrastructure
#                                        Copyright Airbus Defence and Space
# =============================================================================================================

docker run -d \
  -p 3001:3000 \
  -p 8095:8094 \
  -v $PWD/grafana:/usr/share/grafana/data \
  --name datavis \
  grafana
