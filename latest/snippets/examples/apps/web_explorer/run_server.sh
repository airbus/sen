#!/bin/bash
# Usage: ./run_server.sh <port>

PORT=${1:-8000}

WEB_ROOT=$(pwd)
NGINX_CONF="$(pwd)/nginx.conf"

docker run --rm -p ${PORT}:8000 \
  --add-host=host.docker.internal:host-gateway \
  -v "${WEB_ROOT}":/usr/share/nginx/html:ro \
  -v "${NGINX_CONF}":/etc/nginx/nginx.conf:ro \
  nginx:alpine
