# Contributing to Sen

First off, thanks for taking the time to contribute! ❤️

All types of contributions are encouraged and valued. Please make sure to read the relevant section
before making your contribution. It will make it a lot easier for us maintainers and smooth out the
experience for all involved.

## I Have a Question

> If you want to ask a question, we assume that you have read the available documentation.

Before you ask a question, it is best to search for existing issues that might help you. In case you
have found a suitable issue and still need clarification, you can write your question in this issue.

## Code contributions

During this initial publication phase, we are limiting external code contributions. Feedback, issue reports, and
feature suggestions are welcome and will be evaluated. The contribution policy may be reviewed and updated
at a later stage.

## Building and Testing

The [README](README.md) covers the build. For a contribution, build with the tests enabled and
run them before opening the pull request:

```shell
conan install . --profile:all=sen_gcc_x86 --build=missing -o "sen/*:with_tests=True"
conan build . --profile:all=sen_gcc_x86              # sen_gcc_arm on arm hardware
pip install junitparser                                  # run_tests merges the ctest reports with it
cmake --build build/gcc/Release --target run_tests
```

`run_tests` runs the suite in two passes: the ordinary tests, then the ones marked flaky, which
are retried until they pass. While working on one library, `run_unit_tests` is quicker.

If your editor supports devcontainers, the one under `.devcontainer/` already has the compilers,
Conan and the test tools installed.

## Editors

`conan install` writes `CMakeUserPresets.json` at the repository root, pointing at the presets
Conan generated for the build folder. Run it once, then open the repository folder in your
editor and pick the preset it lists: `conan-gcc-release` for the profiles above, or
`conan-msvc-release` when you build with `sen_msvc_x86` on Windows. VS Code reads presets
through the CMake Tools extension; CLion and Visual Studio read them natively, pointed at the
folder rather than at `CMakeLists.txt`.

Two things decide what your editor shows you, and both are set before CMake ever runs:

- `-o "sen/*:with_tests=True"` and `-o "sen/*:with_examples=True"` on the `conan install` line.
  Without them the
  project has no test or example targets at all, and the editor is not hiding them.
- The environment you ran `conan install` in. The generated files hold absolute paths, so a
  build folder belongs to the environment that configured it.

### In a container

The devcontainer is the least work: its image already carries the compilers, Conan and the
editor tools, and its setup step installs the profiles and selects the one matching the
container's architecture. In its terminal, the whole build is:

```shell
conan install . --build=missing -o "sen/*:with_tests=True"
cmake --build --preset conan-gcc-release --target run_unit_tests
```

No package-manager settings are needed there, unlike on a bare machine: the image already has
the system libraries, and inside the container you are not root.

Editors mount the sources at different paths, `/workspaces/<name>` and `/IdeaProjects/<name>`
among them, and Conan writes that path into the presets. So configure where you build. If you
move between a container and your machine, delete `CMakeUserPresets.json` and re-run
`conan install`: Conan keeps entries for build folders that no longer exist, and CMake then
refuses to read the file at all. A build folder configured in a container also cannot be built
on the host, and the other way round; the error names a directory that cannot be created.

CLion builds the profile from the preset by itself, so there is no need to add one by hand. If
its run widget stays empty after a reload, the project model is still loading; the commands
above work from the terminal regardless.

## Commit Messages and Pull Requests

Commit subjects follow the conventional format `type(scope): summary`:

- Types in use: `feat`, `fix`, `test`, `chore`, `refactor`, `docs`, `build`, `ci`, `perf`,
  `revert`. The scope is optional, lowercase and without spaces (`fix(core,kernel): ...`);
  a `!` before the colon marks a breaking change.
- Keep the subject under 72 characters, without a trailing period.
- A body is optional. When one helps, one to three sentences explaining why the change is
  needed are enough; the diff shows what changed.

gitlint checks these rules. Run `pre-commit install` once after cloning: it installs the
file hooks and the commit message hook together.

Pull requests are squash-merged, so **the pull request title becomes the commit subject on
main** and follows the same format. Keep each pull request to a single logical change:
whatever lands is one commit.

## Reporting Bugs

A good bug report shouldn't leave others needing to chase you up for more information. Therefore, we
ask you to investigate carefully, collect information and describe the issue in detail in your
report. Please complete the following steps in advance to help us fix any potential bug as fast as
possible.

- Determine if your bug is really a bug and not an error on your side e.g. using incompatible
  environment components/versions (Make sure that you have read the documentation.
- To see if other users have experienced (and potentially already solved) the same issue you are
  having, check if there is not already a bug report existing for your bug.
- Collect information about the bug:
  - Stack trace (Traceback)
  - OS, Platform and Version
  - Possibly your input and the output
  - Can you reliably reproduce the issue? And can you also reproduce it with older versions?
  - Expected behavior vs Current behavior

If you run into an issue with the project:

- Open an issue. (Since we can't be sure at this point whether it is a bug or not, we ask you not to
  talk about a bug yet and not to label the issue.)
- Explain the behavior you would expect and the actual behavior.
- Please provide as much context as possible and describe the *reproduction steps* that someone else
  can follow to recreate the issue on their own. This usually includes your code. For good bug
  reports you should isolate the problem and create a reduced test case.
- Provide the information you collected in the previous section.

## Suggesting Enhancements

Before submitting a suggestion of enhancement:

- Read the documentation carefully and find out if the functionality is already covered, maybe by an
  individual configuration.
- Perform a search to see if the enhancement has already been suggested. If it has, add a comment to
  the existing issue instead of opening a new one.
- Find out whether your idea fits with the scope and aims of the project. It's up to you to make a
  strong case to convince the project's developers of the merits of this feature. Keep in mind that
  we want features that will be useful to the majority of our users and not just a small subset.

Enhancement suggestions:

- Use a **clear and descriptive title** for the issue to identify the suggestion.
- Provide a **step-by-step description of the suggested enhancement** in as many details as
  possible.
- **Describe the current behavior** and **explain which behavior you expected to see instead** and
  why. At this point you can also tell which alternatives do not work for you.
- You may want to **include screenshots and animated GIFs** which help you demonstrate the steps or
  point out the part which the suggestion is related to.
- **Explain why this enhancement would be useful** to most Sen users. You may also want to point out
  the other projects that solved it better and which could serve as inspiration.
