#!/usr/bin/env bash
# Prints the full reference for one of this repository's CI image variants.
#
# The repository name lives here and nowhere else. It was written by hand in
# four tags across three files, so publishing to a registry would have meant
# finding every one -- and a missed reference still pulls, from the old place.
set -euo pipefail

# Empty keeps the reference a local tag, which is what a developer building the
# image gets. CI sets it to publish and to pull. It must end in a slash when set.
SEN_CI_REGISTRY="${SEN_CI_REGISTRY-}"
SEN_CI_REPOSITORY="sen-ci"

variant="${1:?usage: image_name.sh <variant> <ubuntu>   base, dev, or buildcache}"
# Required rather than defaulted: one Dockerfile builds both bases, so its content cannot
# tell them apart and a caller that forgot which it wanted would silently get the other.
ubuntu="${2:?usage: image_name.sh <variant> <ubuntu>   22.04 or 24.04}"

case "$SEN_CI_REGISTRY" in
    "" | */) ;;
    *) echo "SEN_CI_REGISTRY must end in a slash: $SEN_CI_REGISTRY" >&2; exit 2 ;;
esac

# The layer cache is the one reference that must NOT carry the content: a build
# imports it to avoid repeating work the last content already did, so tagging it
# by content would mean never reusing it.
# It still carries the base, because two bases share no layers worth importing.
if [ "$variant" = buildcache ]; then
    printf '%s%s:buildcache-%s\n' "$SEN_CI_REGISTRY" "$SEN_CI_REPOSITORY" "$ubuntu"
    exit 0
fi

# Every image tag carries the Dockerfile's content, so an edit to it misses the
# pull and the consumer rebuilds. Sharing one tag across contents would let a lane
# run the previous environment and say nothing. git hash-object rather than
# sha256sum: git is the one tool every platform here already has.
root=$(git rev-parse --show-toplevel)
content=$(git hash-object "$root/tools/ci/Dockerfile" | cut -c1-12)

printf '%s%s:%s-%s-%s\n' "$SEN_CI_REGISTRY" "$SEN_CI_REPOSITORY" "$variant" "$ubuntu" "$content"
