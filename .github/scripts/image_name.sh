#!/usr/bin/env bash
# Prints the full reference for one of this repository's CI image variants.
#
# The repository name lives here and nowhere else. It was written by hand in
# four tags across three files, so publishing to a registry would have meant
# finding every one -- and a missed reference still pulls, from the old place.
set -euo pipefail

# Empty while no registry hosts the image, so the reference stays a local tag.
# Publishing is one edit here: set it to "ghcr.io/airbus/sen/" and every caller
# follows. It must end in a slash when set.
SEN_CI_REGISTRY="${SEN_CI_REGISTRY-}"
SEN_CI_REPOSITORY="sen-ci"

variant="${1:?usage: image_name.sh <variant>   e.g. dev, probe, docs, lane}"

case "$SEN_CI_REGISTRY" in
    "" | */) ;;
    *) echo "SEN_CI_REGISTRY must end in a slash: $SEN_CI_REGISTRY" >&2; exit 2 ;;
esac

printf '%s%s:%s\n' "$SEN_CI_REGISTRY" "$SEN_CI_REPOSITORY" "$variant"
