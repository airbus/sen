# === conftest.py ======================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Keeps the tests that shell out to git inside their own temporary repositories.

Committing from a linked worktree hands the hook an absolute GIT_DIR and
GIT_INDEX_FILE, so a test that builds a fixture repository would reinitialise
the one running the hook instead.
"""

import pytest

# Each of these redirects git on its own, whether or not a hook sets it.
REDIRECTING_VARIABLES = ("GIT_DIR", "GIT_INDEX_FILE", "GIT_WORK_TREE", "GIT_COMMON_DIR")


@pytest.fixture(autouse=True)
def keep_git_inside_the_test_repository(monkeypatch):
    """Drops the inherited repository location from every test's environment."""
    for name in REDIRECTING_VARIABLES:
        monkeypatch.delenv(name, raising=False)
