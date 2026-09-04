#!/usr/bin/env bash
# Deletes the cache entry a save is about to replace. Keys are immutable, so a save
# can only add: without this every merge leaves another copy and the repository
# passes GitHub's 10 GB limit, after which entries are evicted at random.
#
# Refuses to run off main, because the saves it accompanies run only there and an
# ungated delete would take the shared entry from any branch. Checks the entry is
# gone afterwards, because a failed save is only a warning.

set -euo pipefail

key="${1:?usage: drop_cache_key.sh <key>}"
: "${GH_TOKEN:?GH_TOKEN must be set}"
repo="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY must be set}"

if [ "${GITHUB_REF:-}" != "refs/heads/main" ]; then
    echo "not on main (${GITHUB_REF:-unset}); leaving '$key' alone"
    exit 0
fi

keys_matching() {
    gh api "/repos/$repo/actions/caches" --paginate \
        --jq ".actions_caches[] | select(.key | startswith(\"$1\")) | .key" 2>/dev/null || true
}

# Leftovers from when the key carried a date. The suffix shape is matched exactly,
# so a family whose name begins with this one is not caught by it.
siblings=$(keys_matching "$key" \
    | grep -E "^${key}-([0-9]{4}-[0-9]{2}-[0-9]{2}T|[0-9]{8}$)" || true)
if [ -n "$siblings" ]; then
    echo "dated leftovers under '$key', removing:"
    while IFS= read -r old_key; do
        [ -z "$old_key" ] && continue
        echo "  $old_key"
        gh cache delete "$old_key" -R "$repo" >/dev/null 2>&1 || true
    done <<< "$siblings"
fi

# Shares the prefix and is not a dated leftover. Stop and let someone look.
unexpected=$(keys_matching "$key" | grep -vx "$key" \
    | grep -Ev "^${key}-([0-9]{4}-[0-9]{2}-[0-9]{2}T|[0-9]{8}$)" || true)
if [ -n "$unexpected" ]; then
    echo "unexpected entries sharing the prefix '$key':" >&2
    echo "$unexpected" | sed 's/^/  /' >&2
    exit 1
fi

if [ -z "$(keys_matching "$key" | grep -x "$key" || true)" ]; then
    echo "no entry under '$key'; nothing to drop"
    exit 0
fi

gh cache delete "$key" -R "$repo"

if [ -n "$(keys_matching "$key" | grep -x "$key" || true)" ]; then
    echo "'$key' survived the delete; the save after it would do nothing" >&2
    exit 1
fi

echo "dropped '$key'"
