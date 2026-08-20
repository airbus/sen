# === test_check_dependency_alignment.py ===============================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the graph verdicts of the dependency build_type alignment check."""

from check_dependency_alignment import misaligned_nodes


def graph(nodes: dict) -> dict:
    """Wraps node entries in the conan graph info json shape."""
    return {"graph": {"nodes": nodes}}


def test_aligned_graph_passes():
    """A Debug consumer over Release dependencies is the expected shape."""
    nodes = {
        "0": {"name": "sen", "settings": {"build_type": "Debug"}},
        "1": {"name": "zstd", "settings": {"build_type": "Release"}},
        "2": {"name": "cmake", "settings": {}},
    }
    assert misaligned_nodes(graph(nodes)) == []


def test_dependency_following_the_consumer_fails():
    """A dependency that resolved Debug breaks the shared cache slice."""
    nodes = {
        "0": {"name": "sen", "settings": {"build_type": "Debug"}},
        "1": {"name": "zstd", "settings": {"build_type": "Debug"}},
    }
    assert misaligned_nodes(graph(nodes)) == ["zstd resolved as Debug, expected Release"]


def test_consumer_losing_its_build_type_fails():
    """The sen package must keep the requested build_type."""
    nodes = {"0": {"name": "sen", "settings": {"build_type": "Release"}}}
    assert misaligned_nodes(graph(nodes)) == ["sen resolved as Release, expected Debug"]


def test_settings_free_nodes_are_ignored():
    """Tool packages without a build_type setting are fine."""
    nodes = {
        "0": {"name": "sen", "settings": {"build_type": "Debug"}},
        "1": {"name": "meson", "settings": None},
    }
    assert misaligned_nodes(graph(nodes)) == []
