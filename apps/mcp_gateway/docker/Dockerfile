# Reference Dockerfile for the Sen MCP gateway.
#
# Build context: the apps/mcp_gateway/ directory. The build pulls the bundled `.cjs`
# from dist/ and the wrapper from bin/, both produced by `npm run build` (or by a
# full Sen CMake build).
#
#   cd apps/mcp_gateway
#   npm run build
#   docker build -t sen-mcp-gateway -f docker/Dockerfile .
#
# See docs/components/mcp_gateway.md for the recommended `docker run` flags.

FROM node:22-alpine

# python3 is required for the runRecordingScript tool. Skip if SEN_MCP_GATEWAY_NO_RECORDING=1.
RUN apk add --no-cache python3

ARG APP_DIR=/opt/sen-mcp-gateway
WORKDIR ${APP_DIR}

COPY bin/sen-mcp-gateway ${APP_DIR}/bin/sen-mcp-gateway
COPY dist/sen-mcp-gateway.cjs ${APP_DIR}/bin/sen-mcp-gateway.cjs
COPY dist/sen-mcp-gateway.cjs.map ${APP_DIR}/bin/sen-mcp-gateway.cjs.map

RUN chmod +x ${APP_DIR}/bin/sen-mcp-gateway

# Non-root user. The recordings volume should be mounted with this UID in mind.
RUN addgroup -S sen && adduser -S -G sen sen
USER sen

ENTRYPOINT ["/opt/sen-mcp-gateway/bin/sen-mcp-gateway"]
