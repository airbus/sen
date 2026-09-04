#!/usr/bin/env bash
# Deletes the cache entries a save is about to replace. Keys are immutable, so a
# save can only add: without this every merge leaves another copy and the
# repository passes GitHub's 10 GB limit, after which entries are evicted at
# random.
#
# Refuses to run off main, because the saves it accompanies run only there and an
# ungated delete would take the shared entry from any branch. Fails rather than
# reports nothing when it cannot list, because a token without actions: write
# looks exactly like an empty cache.
#
# Judged by id rather than by key. Two lanes share a conan key family on purpose,
# since they build the same dependencies, so they race: one may delete an entry
# before the other reaches it, and either may save a new one meanwhile. What must
# not happen is an entry this run set out to delete surviving.
set -euo pipefail

key="${1:?usage: drop_cache_key.sh <key>}"
: "${GH_TOKEN:?GH_TOKEN must be set}"
repo="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY must be set}"

if [ "${GITHUB_REF:-}" != "refs/heads/main" ]; then
    echo "not on main (${GITHUB_REF:-unset}); leaving '$key' alone"
    exit 0
fi

list_entries() {
    gh api "/repos/$repo/actions/caches" --paginate \
        --jq '.actions_caches[] | "\(.id) \(.key)"'
}

if ! before=$(list_entries 2>&1); then
    echo "cannot list caches, so cannot tell an empty cache from a failed call:" >&2
    echo "$before" | sed 's/^/  /' >&2
    echo "the job needs 'actions: write'." >&2
    exit 1
fi

# The exact key, and anything under it carrying the date a save appends. Matched
# by shape so a family whose name merely extends this one is not caught.
doomed=$(printf '%s\n' "$before" \
    | grep -E " ${key}$| ${key}-([0-9]{4}-[0-9]{2}-[0-9]{2}T|[0-9]{8}$)" || true)

if [ -z "$doomed" ]; then
    echo "nothing under '$key'"
    exit 0
fi

printf '%s\n' "$doomed" | while read -r id name; do
    [ -z "$id" ] && continue
    if gh api -X DELETE "/repos/$repo/actions/caches/$id" >/dev/null 2>&1; then
        echo "  dropped $name"
    else
        echo "  already gone, or another job took it: $name"
    fi
done

if ! after=$(list_entries 2>&1); then
    echo "cannot re-list caches to confirm the deletes landed" >&2
    exit 1
fi

survivors=""
while read -r id _; do
    [ -z "$id" ] && continue
    if printf '%s\n' "$after" | grep -qE "^${id} "; then
        survivors="$survivors $id"
    fi
done <<< "$doomed"

if [ -n "$survivors" ]; then
    echo "entries this run set out to delete are still present:$survivors" >&2
    exit 1
fi
