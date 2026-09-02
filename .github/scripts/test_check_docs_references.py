# === test_check_docs_references.py ====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins what counts as a documentation reference, and what does not.

Every "not reported" case here is a false positive the check produced on its
first run against the real tree. A check that cries wolf gets switched off, so
each is worth a test.
"""

import subprocess

from check_docs_references import dangling, hosted_images, snippet_sections


def write(root, relative, text):
    """Create a source file under a scratch repository root."""
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def cited_paths(root):
    """Just the paths reported, so assertions read as intent."""
    return [path for _, _, path in dangling(root)]


def test_reports_a_dangling_reference(tmp_path):
    """The defect this exists for: a comment naming a page that never landed."""
    write(tmp_path, "components/x/vite.config.ts", "// See docs/web-explorer/findings.md.\n")
    assert cited_paths(tmp_path) == ["docs/web-explorer/findings.md"]


def test_accepts_a_reference_that_resolves(tmp_path):
    """A cited page that exists is not a finding."""
    write(tmp_path, "docs/getting_started/install.md", "# Install\n")
    write(tmp_path, "resources/installer/install.sh", "# see docs/getting_started/install.md\n")
    assert cited_paths(tmp_path) == []


def test_reports_a_dangling_cmake_variable_path(tmp_path):
    """`${PROJECT_SOURCE_DIR}/docs/...` really does name the source tree."""
    write(
        tmp_path,
        "components/explorer/CMakeLists.txt",
        "install(FILES ${PROJECT_SOURCE_DIR}/docs/assets/images/logo.svg)\n",
    )
    assert cited_paths(tmp_path) == ["docs/assets/images/logo.svg"]


def test_ignores_a_build_output_path(tmp_path):
    """`build/<compiler>/<config>/docs/site` is an output, not the source tree."""
    write(tmp_path, ".github/workflows/docs_check.yaml", "          path: build/gcc/Release/docs/site/\n")
    assert cited_paths(tmp_path) == []


def test_ignores_a_url_that_contains_docs(tmp_path):
    """A standards-paper URL happens to carry the same three letters."""
    write(
        tmp_path,
        "libs/core/include/span.h",
        "/// Inspired by http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0122r7.pdf\n",
    )
    assert cited_paths(tmp_path) == []


def test_ignores_mkdocs_paths(tmp_path):
    """`mkdocs/` ends with the same four characters as `docs/`."""
    write(tmp_path, "libs/gen/src/mkdocs/generator.cpp", '#include "mkdocs/classdoc_tree.h"\n')
    assert cited_paths(tmp_path) == []


def test_ignores_synthetic_paths_in_tests(tmp_path):
    """Path classifiers are fed paths that are not supposed to exist."""
    write(tmp_path, ".github/scripts/test_classify.py", 'assert not is_code_change(["docs/assets/logo.svg"])\n')
    write(tmp_path, "libs/core/test/thing_test.cpp", "// docs/gone.md\n")
    assert cited_paths(tmp_path) == []


def test_ignores_the_docs_tree_itself(tmp_path):
    """Links inside the site are mkdocs' job, and it is stricter than this."""
    write(tmp_path, "docs/index.md", "[gone](docs/nowhere.md)\n")
    assert cited_paths(tmp_path) == []


def test_ignores_prose_mentioning_the_directory(tmp_path):
    """Prose naming the directory is English, not a path."""
    write(tmp_path, "tools/thing.py", "# Everything under docs/ is generated.\n")
    assert cited_paths(tmp_path) == []


# ----------------------------------------------------------------------------------------------------------------
# Screenshots hosted on a side branch
#
# They live there on purpose: it keeps megabytes of binaries out of the history
# everyone clones, without git-lfs, whose storage some corporate networks block.
# The cost is that nothing validates the links, which is what these cover.
# ----------------------------------------------------------------------------------------------------------------


def image_branch(root, names):
    """A scratch repository whose `fix/images` branch holds the given files."""
    git = ["git", "-c", "user.email=t@t", "-c", "user.name=t"]
    subprocess.run(git + ["init", "-q", "-b", "fix/images"], cwd=root, check=True)
    for name in names:
        (root / name).write_bytes(b"RIFF")
    subprocess.run(git + ["add"] + list(names), cwd=root, check=True)
    subprocess.run(git + ["commit", "-qm", "images"], cwd=root, check=True)


def hotlink(name):
    """A screenshot link in the form the documentation actually uses."""
    return f"![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/fix/images/{name})\n"


def test_accepts_a_hotlink_whose_image_is_on_the_branch(tmp_path):
    """The normal case: the screenshot is where the URL says it is."""
    image_branch(tmp_path, ["tracy.gif"])
    write(tmp_path, "docs/components/tracy.md", hotlink("tracy.gif"))
    missing, skipped = hosted_images(tmp_path)
    assert missing == [] and skipped == []


def test_reports_a_hotlink_whose_image_is_missing(tmp_path):
    """A renamed or deleted screenshot fails only in the browser otherwise."""
    image_branch(tmp_path, ["tracy.gif"])
    write(tmp_path, "docs/components/tracy.md", hotlink("shell_help.gif"))
    missing, _ = hosted_images(tmp_path)
    assert [where for _, _, where in missing] == ["fix/images:shell_help.gif"]


def test_skips_when_the_branch_is_not_fetched(tmp_path):
    """A shallow CI checkout lacks the ref; failing there punishes a correct tree."""
    write(tmp_path, "docs/components/tracy.md", hotlink("tracy.gif"))
    missing, skipped = hosted_images(tmp_path)
    assert missing == [] and skipped == ["fix/images"]


def test_reads_the_repository_being_scanned(tmp_path):
    """Without cwd=root this answers from whichever repository git is standing in."""
    image_branch(tmp_path, ["only_here.gif"])
    write(tmp_path, "docs/x.md", hotlink("tracy.gif"))
    missing, skipped = hosted_images(tmp_path)
    assert [where for _, _, where in missing] == ["fix/images:tracy.gif"]
    assert skipped == []


# ----------------------------------------------------------------------------------------------------------------
# Snippet sections
#
# A page can include a named region of compiled source so the sample cannot
# drift. The cost is a marker living in a file whose author has no reason to
# expect one, so the pair has to be checked.
# ----------------------------------------------------------------------------------------------------------------


def test_accepts_an_include_whose_section_is_marked(tmp_path):
    """The normal case: the region named by the page exists in the source."""
    write(tmp_path, "docs/page.md", '--8<-- "libs/thing.cpp:setup"\n')
    write(tmp_path, "libs/thing.cpp", "// --8<-- [start:setup]\nint x;\n// --8<-- [end:setup]\n")
    assert snippet_sections(tmp_path) == []


def test_reports_an_include_whose_marker_was_removed(tmp_path):
    """Refactoring the source silently empties the page otherwise."""
    write(tmp_path, "docs/page.md", '--8<-- "libs/thing.cpp:setup"\n')
    write(tmp_path, "libs/thing.cpp", "int x;\n")
    assert [ref for _, _, ref in snippet_sections(tmp_path)] == ["libs/thing.cpp:setup"]


def test_reports_an_include_whose_file_is_gone(tmp_path):
    """A moved source file is the same failure, one step earlier."""
    write(tmp_path, "docs/page.md", '--8<-- "libs/moved.cpp:setup"\n')
    assert "no such file" in snippet_sections(tmp_path)[0][2]


def test_resolves_includes_relative_to_docs_first(tmp_path):
    """Mkdocs searches docs/ before the repository root, and so must this."""
    write(tmp_path, "docs/page.md", '--8<-- "snippets/thing.cpp:setup"\n')
    write(tmp_path, "docs/snippets/thing.cpp", "// --8<-- [start:setup]\nint x;\n")
    assert snippet_sections(tmp_path) == []
