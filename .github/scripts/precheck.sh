#!/usr/bin/env bash
# Every check in main.yaml's "Python checks" job, run locally.
#
# The commit-msg hook lints the bare subject; GitHub lints it with " (#<number>)" appended,
# because a squash merge lands the pull request under it. 69 characters passes here and
# fails at 76 in the queue, and the number is unknown until the pull request exists, so the
# worst case is what gets checked. Fixing it afterwards costs a whole pipeline run: there is
# no `edited` in main.yaml's pull_request types, and a re-run replays the original payload.
#
# Usage:  precheck.sh [pull-request-title]     (no argument: everything but the title)
set -uo pipefail

cd "$(git rev-parse --show-toplevel)" || exit 2
log=$(mktemp -d) || exit 2
trap 'rm -rf "$log"' EXIT
fail=0

# -perm -u+x, not -perm +111: the latter is BSD-only and errors on Linux, where CI and most
# of the developers are.
gitlint=$(command -v gitlint || find "$HOME/.cache/pre-commit" -name gitlint -type f -perm -u+x 2>/dev/null | head -1)
[ -x "$gitlint" ] || { echo "no gitlint found; pip install gitlint or run pre-commit once"; exit 2; }

# GitHub appends " (#N)". Four digits is the worst case this repository will reach.
SUFFIX_BUDGET=8

say() { printf '%-46s %s\n' "$1" "$2"; }

echo "== the checks main.yaml runs, run here =="

# 1. The tests, with the same selection CI uses -- not just .github/scripts.
if python3 -m pytest .github/scripts/ libs/kernel/test/integration/ -q >"$log/pytest" 2>&1; then
  say "pytest (scripts + kernel integration)" "ok  $(tail -1 "$log/pytest")"
else
  say "pytest (scripts + kernel integration)" "FAILED"; tail -5 "$log/pytest" | sed 's/^/    /'; fail=1
fi

# 2. Commit messages, exactly as the job invokes it. The job uses the pull request's own
#    base rather than main, so a stacked branch does not re-lint what it sits on; there is
#    no base here, so main is the closest available.
if "$gitlint" --commits "origin/main..HEAD" >"$log/commits" 2>&1; then
  say "gitlint --commits origin/main..HEAD" "ok"
else
  say "gitlint --commits origin/main..HEAD" "FAILED"; sed 's/^/    /' "$log/commits"; fail=1
fi

# 3. What the merge queue will actually lint: each subject as a squash subject.
#    This is the check with no local equivalent, and the one that cost the pipeline runs.
while read -r sha; do
  [ -z "$sha" ] && continue
  subj=$(git log -1 --format='%s' "$sha")
  if printf '%s (#9999)\n' "$subj" | "$gitlint" >"$log/squash" 2>&1; then
    say "squash subject $(git log -1 --format=%h "$sha")" "ok  $(( ${#subj} + SUFFIX_BUDGET )) chars worst case"
  else
    say "squash subject $(git log -1 --format=%h "$sha")" "FAILED at $(( ${#subj} + SUFFIX_BUDGET )) chars"
    sed 's/^/    /' "$log/squash"
    echo "    trim the subject to $(( 72 - SUFFIX_BUDGET )) characters or fewer"
    fail=1
  fi
done < <(git rev-list origin/main..HEAD)

# 4. The pull request title, if one was given, the same way.
if [ $# -ge 1 ] && [ -n "$1" ]; then
  if printf '%s (#9999)\n' "$1" | "$gitlint" >"$log/title" 2>&1; then
    say "pull request title" "ok  $(( ${#1} + SUFFIX_BUDGET )) chars worst case"
  else
    say "pull request title" "FAILED at $(( ${#1} + SUFFIX_BUDGET )) chars"
    sed 's/^/    /' "$log/title"; fail=1
  fi
else
  say "pull request title" "not checked: pass it as an argument"
fi

echo
[ $fail -eq 0 ] && echo "All clear." || echo "Fix the above before pushing; a merge queue slot costs an approval."
exit $fail
