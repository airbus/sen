# === test_the_package_id_ignores_options_that_change_nothing.py =======================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the recipe drops the options which fork its identity without changing it.

`with_clang_tidy`, `with_docs` and `with_examples` leave the installed tree identical, so flipping
one should not ask for a different package.

Parsed rather than imported: the hook that runs these tests has only pytest and pyyaml, so
`from conan import ConanFile` fails there.
"""

import ast
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RECIPE = ROOT / "conanfile.py"
UNCHANGED_BY = ("with_clang_tidy", "with_docs", "with_examples")


def recipe_tree() -> ast.Module:
    """The recipe, parsed. No import, so this needs none of Sen's dependencies."""
    return ast.parse(RECIPE.read_text(encoding="utf-8"))


def method(name: str) -> ast.FunctionDef | None:
    """The named method of the recipe class, or None."""
    for node in ast.walk(recipe_tree()):
        if isinstance(node, ast.FunctionDef) and node.name == name:
            return node
    return None


def declared_options() -> list[str]:
    """The keys of the recipe's `options` dict."""
    for node in ast.walk(recipe_tree()):
        if not isinstance(node, ast.Assign) or not isinstance(node.value, ast.Dict):
            continue
        if not any(getattr(target, "id", "") == "options" for target in node.targets):
            continue
        return [key.value for key in node.value.keys if isinstance(key, ast.Constant) and isinstance(key.value, str)]
    return []


def dropped_names() -> set[str]:
    """Every string literal inside `package_id`."""
    body = method("package_id")
    assert body is not None, "the recipe declares no package_id"
    found = set()
    for node in ast.walk(body):
        if isinstance(node, ast.Constant) and isinstance(node.value, str):
            found.add(node.value)
    return found


def test_the_recipe_declares_the_method():
    """The recipe declares a package_id method."""
    assert method("package_id") is not None, "the recipe declares no package_id"


def test_every_option_that_changes_nothing_is_dropped():
    """package_id names every option that leaves the package unchanged."""
    missing = [name for name in UNCHANGED_BY if name not in dropped_names()]
    assert not missing, f"package_id does not name {missing}, so flipping one rebuilds Sen for nothing"


def test_the_names_this_test_reasons_about_still_exist():
    """The options this test names are still declared by the recipe."""
    unknown = [name for name in UNCHANGED_BY if name not in declared_options()]
    assert not unknown, f"this test names {unknown}, which the recipe does not declare as an option"
