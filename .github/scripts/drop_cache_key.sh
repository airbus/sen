#!/usr/bin/env bash
# Removes the cache entry a save is about to replace, so a family keeps one entry
# rather than accumulating a copy per merge.
#
# Cache keys are immutable: a save can only add, never update. Anything that saves
# on every merge therefore grows until the repository passes GitHub's 10 GB limit
# and eviction starts removing entries nobody chose -- the conan ones first, since
# they are written least often and are what decides whether a lane takes five
# minutes or twenty-eight.
#
# Two things here are load-bearing rather than defensive.
#
# It refuses to run anywhere but main. Every save this accompanies is gated to
# main, and a delete that is not gated the same way removes the shared entry from
# any branch and puts nothing back -- measured, and the run reports success, so
# there is no red check to catch it. Enforcing that here means a workflow that
# forgets the condition is harmless instead of destructive.
#
# It confirms the entry is gone. actions/cache downgrades a failed save to a
# warning, so a delete that quietly stopped working would leave every later save a
# silent no-op and freeze the cache -- builds slowly getting worse with nothing
# red. Failing here is what turns that into something visible.
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

# A stable key has one entry. Anything else sharing its prefix is a leftover from
# when the key carried a timestamp or a date, so this drops them too -- otherwise
# the first run after the change would trip over entries the change exists to stop
# creating. The suffix is matched exactly rather than by prefix alone, so a family
# whose name merely begins with this one cannot be caught by it.
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

# Anything else sharing the prefix is not a leftover and not expected. Report it
# rather than guess: a key nobody predicted is worth a human reading it.
unexpected=$(keys_matching "$key" | grep -vx "$key" \
    | grep -Ev "^${key}-([0-9]{4}-[0-9]{2}-[0-9]{2}T|[0-9]{8}$)" || true)
if [ -n "$unexpected" ]; then
    echo "entries share the prefix '$key' and are neither it nor dated leftovers:" >&2
    echo "$unexpected" | sed 's/^/  /' >&2
    exit 1
fi

if [ -z "$(keys_matching "$key" | grep -x "$key" || true)" ]; then
    echo "no entry under '$key'; nothing to drop"
    exit 0
fi

gh cache delete "$key" -R "$repo"

if [ -n "$(keys_matching "$key" | grep -x "$key" || true)" ]; then
    echo "'$key' survived the delete: the save that follows would be a silent" >&2
    echo "no-op and the cache would freeze. Failing rather than continuing." >&2
    exit 1
fi

echo "dropped '$key'"
