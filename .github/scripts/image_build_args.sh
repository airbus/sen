#!/usr/bin/env bash
# Prints the build arguments that pin one Ubuntu base, as NAME=VALUE, one per line.
#
# The digest and the codename must agree, and a digest silently overrides the tag written beside
# it, so a mismatched pair builds the other release and says nothing. They are written together
# here and nowhere else; the Dockerfile's own defaults cover a bare `docker build`.
set -euo pipefail

ubuntu="${1:?usage: image_build_args.sh <ubuntu>   22.04 or 24.04}"

# Compilers beyond the pair every image carries. The nightly's newest-compiler lane
# wants gcc-14, which is in noble's archive and not in jammy's, so it belongs to one
# image rather than to the Dockerfile.
case "$ubuntu" in
    22.04)
        ref="ubuntu:22.04@sha256:2edbbc5dc405e9612ba3584ce95480277e3eb374407b5505fe26f17df77c7dbc"
        codename=jammy
        extra_compilers=""
        ;;
    24.04)
        ref="ubuntu:24.04@sha256:008173c23f95b170204355c12626cb5a965d779a7e1283b09e9cffbb1bf33ca3"
        codename=noble
        extra_compilers="gcc-14 g++-14"
        ;;
    *)
        echo "unknown ubuntu version: $ubuntu" >&2
        exit 2
        ;;
esac

printf 'UBUNTU_REF=%s\nSEN_UBUNTU_CODENAME=%s\nSEN_EXTRA_COMPILERS=%s\n' \
    "$ref" "$codename" "$extra_compilers"
