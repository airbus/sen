# CI and release pipeline -- Architecture

This page explains how the CI pipeline is organized and why. The workflow
definitions live in [`.github/workflows/`](../../.github/workflows). The build
image is defined next to this file. The job matrix comes from
[`generate_matrix_jobs.py`](../../.github/scripts/generate_matrix_jobs.py).

## Purpose

Pull requests get a fast and reliable test result on free GitHub-hosted
runners. The slow checks (sanitizers, clang-tidy over the whole tree, repeated
test runs, benchmarks) run every night instead of on every pull request.
Releases are built from locked, reproducible inputs. Third-party code is
compiled as rarely as possible. Every input that can change (dependencies,
actions, tools) is pinned to a fixed version, so a version change always
appears as a reviewable diff.

## Workflow inventory

| File                 | Trigger                                    | Purpose                                     |
| -------------------- | ------------------------------------------ | ------------------------------------------- |
| `main.yaml`          | pull_request, push to main, merge_group, nightly cron, dispatch | starts the checks, tests and packaging |
| `pre-commit.yaml`    | called by main.yaml                        | lint and format checks on the changed files |
| `standard_test.yaml` | called by main.yaml                        | the build-and-test matrix                   |
| `conan.yaml`         | called by main.yaml                        | the `conan create` packaging jobs           |
| `nightly.yaml`       | nightly cron, dispatch                     | sanitizers, clang-tidy, repeated test runs, benchmarks, full lint, documentation build, coverage report, newest compiler |
| `docs_check.yaml`    | called by main.yaml and nightly.yaml       | builds the documentation without publishing it |
| `ci-image.yaml`      | called by main.yaml, dispatch              | builds the build-environment image and checks the tools inside it |
| `build_release.yaml` | version tags                               | release artifacts and the GitHub release draft |
| `build_docs.yaml`    | push to main, version tags                 | mkdocs + doxygen, published to gh-pages     |

## What runs on a pull request

`main.yaml` starts three cheap jobs on every push:

- pre-commit, which lints and formats only the changed files,
- a check that `conan.lock` still matches `conanfile.py`,
- a python-checks job, which tests the repository scripts and checks the
  commit messages and the pull request title (after a squash merge, the
  title becomes the commit subject on main).

A fourth job looks at which paths changed. The expensive build-and-test
matrix only runs when it is needed:

- Draft pull requests stop after the cheap checks. Marking the pull request
  ready runs everything. Converting it back to a draft cancels a run that
  is still in progress.
- Pull requests that only change documentation (`docs/` and `*.md` paths)
  also stop after the cheap checks. They still run the documentation build
  described below.
- Every other pull request runs the matrix from `generate_matrix_jobs.py`.

The matrix legs build the examples together with the production code and
run the example smoke tests. The shipping leg (Linux gcc Release) also
builds the CPack archive and checks that it still holds the binaries, the
public headers and the exported cmake configuration, and that its name
still matches what the release upload expects. That archive is what the
release attaches and what the installer unpacks, so a dropped install rule
would otherwise surface only at release time. The Windows legs build
neither; building the examples there is still open.

The clang leg is the one that measures coverage. It builds the report as it
runs the suite, uploads it, prints the total in the job summary, and fails if
the share of covered lines falls below the floor recorded in
`standard_test.yaml`. The floor sits a few points under the measured figure,
because the number moves slightly between runs; the browsable report is
published by the nightly run.

The two x86 gcc legs also build a small runtime image
(`tools/ci/runtime.Dockerfile`) and register the container-based
`object_sync` suite, which then runs inside `run_tests` like every other
test. The image is built before the compile, so a problem with docker costs
seconds rather than a whole build, and a step right after the compile
asserts that the suite really registered: its tests are labelled flaky, a
selection ctest is allowed to leave empty, so a broken handoff would
otherwise pass silently. The clang and arm legs do not run the suite; they
would add container startups without covering anything these two legs miss.

A separate job builds the documentation (the API reference and the
handbook) without publishing it. It runs when a changed path is an input
of the documentation build: `docs/`, `examples/` (the handbook copies its
snippets from there), `mkdocs.yml`, or any markdown file. A broken
documentation change is caught on the pull request instead of after the
merge.

Building and publishing are separate workflows on purpose. Publishing
needs write access to the `gh-pages` branch, and the documentation build
runs unreviewed pull-request content (the CMake files, the conan recipe
and the mkdocs configuration all execute during it). Pull requests
therefore call `docs_check.yaml`, which is read-only;
`build_docs.yaml` keeps the write permissions and runs only on pushes to
main and on version tags. The build steps themselves live in one place,
the `build_documentation` composite action, so both paths build the
documentation the same way.

The last job is `CI OK`, and branch protection requires only this one check.
The other jobs are skipped on purpose in normal situations (drafts,
documentation-only changes, a packaging lane that does not run on pull
requests), and a required check that never reports would block the merge
forever, so the aggregator is the one job that always runs and always
reports.

Because GitHub reports a **skipped** job as a successful check, the
aggregator has to decide explicitly rather than simply exist. It fails when
a job it waited for failed or was cancelled, when the run itself was
cancelled, and when the test matrix was skipped although the change was
classified as code. Without those conditions, cancelling a run would satisfy
the required check with nothing built. A test in `test_main_yaml.py` makes
sure every job appears in the `needs` list; a job missing there could fail
without blocking the merge.

A draft is deliberately not failed. Its run did what a draft run is meant to
do, and GitHub does not allow a draft to be merged, so failing it would make
the check report "unfinished" as well as "broken" -- and a check that cries
failure for work that never started is a check people learn to ignore. The
cost is one narrow race: the moment a draft is marked ready, the green
result of the draft run still sits on that commit until the new run reports.
Auto-merge is off for exactly this reason; do not merge in the seconds
between marking ready and the new run appearing.

Two rules keep this setup working:

- A job that must not always run is skipped with a job-level `if:`
  condition, never with a workflow-level `paths:` filter. A skipped job
  counts as passed, which the aggregator handles explicitly. A workflow that
  never starts reports nothing, and the pull request can then never be
  merged.
- Auto-merge stays off. When a draft is marked ready, there is a short
  moment before the new run appears in which the green result from the
  draft phase would still allow a merge.

Only one run per branch and trigger kind exists at a time (the
`concurrency` group): a new push to a pull request cancels the run that is
still in progress. Cancellation is limited to pull requests, so two merges
landing minutes apart cannot cancel each other's post-merge validation. The
event name is part of the group key, so a manual run and a push to main do
not collide.

**Packaging on a pull request builds one configuration.** Packaging is the
only check that builds Sen as a package and then builds a consumer against
it, so a broken conan recipe or an unusable exported configuration shows up
there and nowhere else. (Whether every install rule still ships what it
should is a different question, answered by the package-archive check
below.) Running every configuration on every push flooded the runner queue,
so a pull request runs the configuration that ships (Linux gcc Release) and
the full set runs after a merge, in the merge queue, every night and on
manual dispatch. It runs in parallel with the test matrix.

## The job matrix

`generate_matrix_jobs.py` decides which compiler, architecture and
build-type combinations exist and which workflow runs them. Each job
description is a dataclass that checks its own fields when it is created. A
selection that cannot be classified raises an error instead of silently
producing an empty matrix. `test_generate_matrix_jobs.py` pins the exact
list of jobs, so changing the matrix also means updating the expected list
in the same commit. These tests run in stage 0 and as a local pre-commit
hook, and one of them asserts that every declared leg is selected by some
workflow, so a leg that reads as coverage but can never run does not
survive.

Besides compiler, build type and language standard, each leg carries three
switches: whether it builds the examples, the docker base image for the
container-based integration tests (empty means the leg does not run them;
the base must match the runner's OS so the binaries mounted into the
containers find a matching runtime), and whether it builds and checks the
package archive. The
standard is passed to the compiler, so adding a leg for a different one
tests what its name claims; every leg currently builds C++17, which is what
the project promises its consumers.

## Dependencies

**`conan.lock` pins every dependency by version and recipe revision.** One
lockfile covers all graph shapes (Linux and Windows resolve differently;
the build type does not change the graph). Conan picks the file up
automatically. `conan_lock.py check` resolves the graph with the lockfile
as input, so changes in upstream recipes stay invisible and only changes in
our own conanfile show up. `conan_lock.py update` regenerates the lockfile.
The conan version itself is pinned in three places that must stay
identical: the image (`CONAN_VERSION` build argument),
`setup_build_context`, and the lockfile check job.

**Dependencies always build as Release, independent of the build type of
the sen package** (`-s "&:build_type=..."`, or `sen/*:` in `conan create`,
whose test_package graph has a different consumer). This way one set of
dependency binaries serves both the Debug and the Release jobs of a
compiler, and it matches what binary remotes provide. There is one
exception: an MSVC Debug test job must not use Release dependencies,
because MSVC links different C runtimes in Debug and Release (MDd/MD). All
current Windows jobs are Release, so the rule is safe today; revisit it
when Windows tests are enabled. A stage-0 check resolves a Debug consumer
graph and fails if any dependency left the Release default.

**Caches.** Conan packages: one Actions cache entry per compiler
(`conanp-<os>-<compiler>-<version>-<std>-<date>`). Both workflows and all
build types share it. It is restored by prefix, so the newest snapshot
wins. Sources and build folders are cleaned before saving. ccache: one
cache entry per compiler and build type. The nightly sanitizer runs reuse
the conan cache (dependencies build without sanitizer flags) but do not use
ccache: object files built with sanitizer flags must not end up in the
normal cache entries.

## The build image

[`Dockerfile`](Dockerfile) defines the build environment: gcc-12, clang-20
and clang-tidy-20 from a pinned apt.llvm.org repository, ccache, pinned
versions of conan, junitparser and pytest, and the coverage tools
(gcovr, lcov and the LLVM equivalents). cmake, ninja and node are not
installed in the image: Conan provides them as tool_requires. Anything that calls cmake
directly must source the generated `conanbuild.sh` first.
`setup_build_context` installs the same toolchain directly on hosted
runners; keep the two in sync.

The Dockerfile has two stages. `base` is the image described above, the one
the pipeline is measured against. `dev` adds cmake, ninja, gdb and graphviz,
because an editor detects a toolchain by looking for the first three and the
dependency-graph target needs the fourth, and the devcontainer builds that
stage. Its cmake comes from pip rather than the distribution:
Conan writes `CMakeUserPresets.json` at version 4, which Ubuntu 22.04's cmake
3.22 refuses to read. Builds still use the cmake and ninja that Conan pins, so
the rule above is unchanged; the editor tools exist for the editor.

`ci-image.yaml` builds both stages and then checks what is inside them. For
`base`: the expected tools are present, cmake and ninja are absent, clang can
link a sanitizer binary, and the image does not run as root. For `dev`: cmake,
ninja, gdb and dot are present and cmake is new enough to read the presets. A Dockerfile
that still builds but lost a tool therefore fails in the pull request that
broke it. `main.yaml` calls it when a change touches the build environment,
and `ci-ok` needs it, so a broken image blocks the merge instead of showing up
as a red mark beside it. Documentation under `tools/ci/` does not trigger it:
the Dockerfile copies nothing out of the repository. Docker layer caching in the Actions cache keeps
rebuilds without changes fast. No registry hosts the image: the
devcontainer and any self-hosted machine build it from this file, and the
revision of the file identifies the environment. (The apt packages inside
the Dockerfile stay unpinned on purpose: the Ubuntu archive removes old
package versions, so exact pins would break within weeks.)

The devcontainer builds this same image, keeps the conan and ccache state
in named volumes, and installs the repository profiles with a default that
matches the machine architecture.

## Where this is heading

The pieces above describe the pipeline as it is. This section describes the
shape it is being moved towards, so that changes can be judged against it.

**The environment should have one definition.** Today it has three: the
Dockerfile, the `setup_build_context` action that installs the toolchain on
runners, and packages installed by individual workflow steps. Nothing keeps
them in agreement, and every difference between them has produced a real
defect: clang-tidy present in one and missing in the other, coverage tools
missing from both, conan pinned in one place and floating in another, a
devcontainer that could not finish a build. The target is a single image,
built from the file in this repository, published to the GitHub container
registry when a change lands on main, and used by both the pull-request jobs
and the devcontainer. Then a tool either exists for everyone or for nobody,
and "a check that quietly analysed nothing" stops being possible.

Publishing to that registry has been confirmed to work for this repository
with the ordinary workflow token, so it needs no organisation-level change.
Two pieces of work remain before jobs can run inside the image: the image
must be built for both x86 and arm, because one test configuration runs on
arm hardware, and workflows must refer to an exact image rather than a
moving label. Images are private when first published, and a public image is
what allows somebody who has just cloned the repository to pull it without
credentials.

**One cmake, not two.** Conan provides cmake and ninja as tool requirements,
so they stay out of the image. The rule that follows is that any step
invoking cmake must do so inside the environment Conan generates. When a step
calls the cmake that happens to be installed on the runner instead, it
disagrees with the one that configured the build, and the whole project
relinks; that is a real cost that was measured, not a theoretical concern.

**One compiler version across the Linux configurations.** All of them build
with gcc-12, including the arm configuration, so the profiles no longer
differ by architecture and the image needs a single compiler. Coverage of a
newer compiler moves to the nightly run, where a build with the newest gcc
catches the stricter diagnostics that a newer release brings.

**C++17 is what every pull request checks**, because that is what the project
promises. Newer standards are checked nightly: the library compiled as C++20,
and a small consumer compiled against the installed package as C++20 and
C++23. Consumers compile these headers in their own dialect, so that
consumer-side check is the one that matches the promise.

**Coverage is measured with clang**, which is the only configuration where it
is switched on. Sending coverage to an external service stays a separate
decision, because it means data about this code leaving Airbus. What exists
instead is described under the pull-request flow and the nightly runs: the
pull request measures coverage, prints the figure and holds a floor, and the
nightly run publishes the browsable report beside the documentation.

**A requested check must never quietly do nothing.** Where a tool is missing,
the build fails rather than warning, whenever the feature was explicitly
asked for. This is the rule that the coverage and clang-tidy lanes broke: the
tools were absent, the build warned, and the lanes passed having measured and
analysed nothing.

**Windows stays outside the image.** MSVC cannot be containerised, so that
configuration keeps its own setup, and the documentation says so rather than
implying the container covers every platform.

## Nightly runs

| Job               | What it does                                                | Why                                         |
| ----------------- | ----------------------------------------------------------- | ------------------------------------------- |
| TSan / ASan+UBSan | clang Debug build of sen with `-o sen/*:sanitizer=thread` / `=address`, full test suite | finds data races and undefined behavior; the Debug build also compiles the `SEN_DEBUG_ASSERT` checks |
| clang-tidy        | full build with `-o sen/*:with_clang_tidy=True`             | pull requests never run clang-tidy; this covers the whole tree |
| repeated tests    | unit test suite with `--repeat until-fail:5`                | repetition exposes tests that fail only sometimes, so they get fixed instead of retried |
| benchmarks        | `run_benchmarks`, JSON results stored per run               | performance history; see below              |
| pre-commit full   | every hook over every file                                  | the pull-request job only checks changed files |
| documentation     | full `doxydocs` + `mkdocs` build, without publishing        | covers documentation inputs the pull-request gate does not watch; catches a failing build, not yet a warning (see the limitations) |
| packaging         | `conan create` for every packaged configuration             | the pull request builds only the shipping configuration |
| public headers under C++20 | sen itself built at C++20                            | its own sources have to compile under a newer standard, not only C++17 |
| newest gcc        | whole tree with the newest gcc on a runner, then the sample consumer at C++20 and C++23 | pull requests build with the oldest supported compiler and only C++17 |
| coverage          | clang Debug build with coverage, report published to `<site>/coverage/` | the clang leg does not run on main, so the published report needs its own lane |

When a nightly job fails, it creates or updates a single tracking issue, so
all nightly failures are collected in one place.

**The coverage report** is published to `<site>/coverage/` on the `gh-pages`
branch, beside the versioned documentation that `mike` manages. The lane that
measures it and the job that publishes it are separate, so that a build never
holds a token which can write to the repository. Publishing shares the
`gh-pages-deploy` concurrency group with the documentation deploy, and if a
deploy still lands first, the report is replayed on top of it rather than
merged: that branch is a published directory, not a line of development.
The pull-request pipeline measures the same figure on its clang leg, prints it
in the job summary and fails if it falls below the floor recorded in
`standard_test.yaml`.

**Benchmarks** come with the full mode and stay out of the default target,
so they cost nothing until `run_benchmarks` asks for them: a library adds a
`benchmark/` directory next to its
`test/` directory and registers its executables with `add_sen_benchmark`
([`cmake/util/benchmark.cmake`](../../cmake/util/benchmark.cmake)).
`libs/core/benchmark/` is the working example.

## Releases

A version tag (`[0-9]+.[0-9]+.[0-9]+`, optional `-rc` suffix) builds the
release jobs of the matrix, packages them, and attaches the artifacts to a
draft GitHub release. Release builds do not restore any caches: release
artifacts are built only from the locked sources.

A release tag (not a release candidate) also publishes the documentation.
The site keeps one frozen copy per release next to `latest`, which follows
main. The name `stable` always points at the newest release, and the
site's front page opens it, so visitors land on the documentation for what
they can actually install.

**How a release is made.** Releases are cut from a release branch
(`release/0.6.x`), not from main. Fixes that a release needs are written
on main first and then cherry-picked onto the release branch. Tagging that
branch starts everything else: the version comes from `git describe`, so
there is no version number to edit anywhere. Candidates are tagged
`X.Y.Z-rc1` and up; they build artifacts and a prerelease draft, but no
documentation. The final tag `X.Y.Z` builds the artifacts, generates the
changelog from the commit messages since the previous release branch, and
opens a **draft** GitHub release. A person reviews that draft and presses
publish; nothing reaches users automatically.

## Static checks

Pre-commit owns linting and formatting: clang-format (its version follows
the LLVM major version of the toolchain; do not bump it on its own),
cmake-format, ruff + ruff-format + mypy, pymarkdown, check-yaml, actionlint
(with shellcheck), hadolint, gitleaks, gitlint (commit messages, rules in
`.gitlint`; `pre-commit install` also sets up the commit-msg stage), and
the merge-conflict and Windows-filename checks. Pull requests check only
the changed files; the nightly run checks everything. The nightly run is
what catches problems caused by updated hooks or rules.

Gitlint exempts `chore(deps)` commits from the body line limit, and only
that rule. Dependabot cites compare URLs that run past the limit and cannot
be wrapped, so without the exemption none of its pull requests can merge at
all. The exemption is written narrowly on purpose: `ci(deps)` is not
`chore(deps)`, so a human writing about dependencies is still checked.

Exceptions are always written down where they apply: `.hadolint.yaml`
(DL3008, see the image section), `.pymarkdown.yaml` (MD046, upstream
crash), `.ruff.toml` per-file ignores (tutorial scripts),
`.gitleaksignore` (documentation placeholders).

## Maintenance

- **Add or update a dependency**: edit `conanfile.py`, run
  `python .github/scripts/conan_lock.py update`, commit both files. The
  stage-0 check fails if you forget.
- **Change the matrix**: edit `generate_matrix_jobs.py` and its tests
  together.
- **Update a tool**: conan is pinned in the three places listed above;
  actions are pinned by commit SHA and dependabot updates them monthly;
  pre-commit versions are updated with `pre-commit autoupdate`, except
  clang-format.
- **Dependency updates arrive in two shapes.** Minor and patch bumps are
  grouped into one pull request, because they carry no decision. Majors come
  separately, because each has to be checked against how this repository uses
  the action, and holding one must not hold the rest. The base OS is ignored
  outright: `runtime.Dockerfile` has to match the OS the binaries were built
  on, so moving it is a coordinated change to both images and is done by hand.
- **Add a benchmark or a nightly job**: follow the existing shape; a new
  nightly job joins the `needs` list of the tracking-issue job, so its
  failures are reported.

## Landing a change

`main` allows squash only and requires linear history, and required checks are
strict, so a pull request has to contain the tip of `main` to merge. One merge
therefore makes every other open pull request stale.

Chained pull requests are a stack, and **`gh pr merge` refuses them**: it
answers that the pull request "must be merged using the asynchronous merge
REST API". Use `gh stack merge <n> --yes --squash`, which merges every
unmerged pull request up to and including the one named, in one all-or-nothing
operation. The resulting history is the same as merging each from the bottom
up: one commit per pull request, nothing collapsed.

Two consequences follow, and neither is visible from the workflows:

- **Merging rebases everything above it.** The branches are rewritten onto the
  new `main`, so their commits get new SHAs and their pipelines run again.
  This is inherent to stacked merging, not a setting that can be turned off.
- **Cancelled runs leave a red `CI OK` behind.** When a rebase supersedes a
  run in flight, its aggregate check reports failure and stays in the rollup.
  Look for a later successful `CI OK` on the same commit before treating one
  as a real failure; GitHub enforces the most recent.

`main` also requires review threads to be resolved. Because the stack merge is
all-or-nothing, **one unresolved comment on one pull request blocks the entire
stack**, and the pull request it blocks is not necessarily the one being merged.

## Known limitations

- MSVC jobs build but do not run tests (SEN-1725). Uploading coverage to an
  external service is wired but disabled (SEN-1726); the published report and
  the floor cover the same ground without sending anything outside.
- `object_sync` is the only suite that uses containers, and it runs only on the
  two x86 gcc legs, because they are the ones that set a runtime image.
  Transport, runtime compatibility, crash report and type clash are a different
  thing: they drive several `sen run` processes through `runner.py` and need no
  container at all, only the ether and py components.
- The Windows legs do not build the examples yet, and are excluded from the
  standard test workflow entirely, so they run no tests on a pull request.
- What runs on a pull request is decided by the pull request's own copy of
  the workflows and of `classify_changes.py`, because that is how GitHub
  runs `pull_request` workflows. A green `CI OK` therefore means "the checks
  this branch asked for passed"; reading the diff of anything under
  `.github/` is part of reviewing a pull request.
- The arm leg builds Debug only and is neither packaged nor released, so
  failures that need optimisation to appear are invisible on that
  architecture and there is no arm artifact.
- The TypeScript client and the web frontend have their type checks and
  bundle builds in the test suite, but their own unit suites are not
  registered as tests and therefore do not run.
- The documentation build fails only when doxygen or mkdocs fail, not when
  they warn: doxygen prints its warnings but `WARN_AS_ERROR` is off, and
  mkdocs does not run with `--strict`. So a malformed doc comment or a
  broken internal link still reaches the published site. The remaining
  warnings, and a doxygen version difference that has to be resolved first,
  are written down in the backlog.
- mypy covers `.github/scripts/` only: duplicate test module names under
  `apps/cli_gen` break a repo-wide run.
- A fresh runner builds all dependencies from source once per cache
  lifetime; the first run of a day (or after the cache was removed) is the
  slow one.
- clang-tidy and the sanitizers run only at night. Planned pull-request
  additions -- a combined ASan+UBSan build, TSan for tests with the
  `threading` label, and clang-tidy on the changed lines -- are tracked in
  the backlog and should exist before the concurrency work starts.
