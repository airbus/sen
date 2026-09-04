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
set -euo pipefail

key="${1:?usage: drop_cache_key.sh <key>}"
: "${GH_TOKEN:?GH_TOKEN must be set}"
repo="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY must be set}"

if [ "${GITHUB_REF:-}" != "refs/heads/main" ]; then
    echo "not on main (${GITHUB_REF:-unset}); leaving '$key' alone"
    exit 0
fi

if ! listing=$(gh api "/repos/$repo/actions/caches" --paginate --jq '.actions_caches[].key' 2>&1); then
    echo "cannot list caches, so cannot tell an empty cache from a failed call:" >&2
    echo "$listing" | sed 's/^/  /' >&2
    echo "the job needs 'actions: write'." >&2
    exit 1
fi

# The exact key, and anything under it carrying the date or timestamp a save
# appends. Matched by shape so a family whose name merely extends this one is
# not caught.
doomed=$(printf '%s\n' "$listing" \
    | grep -E "^${key}$|^${key}-([0-9]{4}-[0-9]{2}-[0-9]{2}T|[0-9]{8}$)" || true)

if [ -z "$doomed" ]; then
    echo "nothing under '$key'"
    exit 0
fi

echo "$doomed" | while IFS= read -r victim; do
    [ -z "$victim" ] && continue
    if gh cache delete "$victim" -R "$repo" >/dev/null 2>&1; then
        echo "  dropped $victim"
    else
        echo "could not delete '$victim'" >&2
        exit 1
    fi
done

# Proves the deletes landed. A save over a surviving key is downgraded to a
# warning, so without this the cache would freeze with nothing red to show.
if ! after=$(gh api "/repos/$repo/actions/caches" --paginate --jq '.actions_caches[].key' 2>&1); then
    echo "cannot re-list caches to confirm" >&2
    exit 1
fi
if printf '%s\n' "$after" | grep -qE "^${key}$|^${key}-([0-9]{4}-[0-9]{2}-[0-9]{2}T|[0-9]{8}$)"; then
    echo "entries under '$key' survived the delete" >&2
    exit 1
fi
