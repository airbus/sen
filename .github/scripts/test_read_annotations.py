# === test_read_annotations.py =========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the annotation grammar the tree actually uses."""

from read_annotations import scan, scan_file

SOURCE = """// a file with the shapes that occur in the tree
/// @test
/// Validates that creating a named provider returns the same instance.
/// @requirements(SEN-363)
TEST(ObjectFilter, NamedProvider)

/// @test Two peers can share one pinned port.
/// @requirements(SEN-351, SEN-583)
TEST_F(EtherFixture, SharesAPort)

/// @test
/// A description that runs
/// over two lines.
TEST_P(Parameterised, HasNoRequirements)

TEST(Bare, NoAnnotationAtAll)
"""


def write(tmp_path, body=SOURCE):
    """Writes a source file and returns its path."""
    path = tmp_path / "some_test.cpp"
    path.write_text(body)
    return path


def test_the_description_and_the_id_are_read(tmp_path):
    """The ordinary shape: @test, a description, then @requirements above the macro."""
    found = {a.key: a for a in scan_file(write(tmp_path))}
    one = found["ObjectFilter.NamedProvider"]
    assert one.description == "Validates that creating a named provider returns the same instance."
    assert one.requirements == ("SEN-363",)


def test_several_ids_on_one_line(tmp_path):
    """Comma separated inside the parentheses, which is how the tree writes two."""
    found = {a.key: a for a in scan_file(write(tmp_path))}
    assert found["EtherFixture.SharesAPort"].requirements == ("SEN-351", "SEN-583")


def test_an_inline_description(tmp_path):
    """One test in the tree writes the text on the @test line itself."""
    found = {a.key: a for a in scan_file(write(tmp_path))}
    assert found["EtherFixture.SharesAPort"].description == "Two peers can share one pinned port."


def test_a_description_over_several_lines_is_joined(tmp_path):
    """Most descriptions wrap, and a report wants one sentence rather than fragments."""
    found = {a.key: a for a in scan_file(write(tmp_path))}
    assert found["Parameterised.HasNoRequirements"].description == "A description that runs over two lines."


def test_a_typed_test_is_found(tmp_path):
    """TYPED_TEST registers one case per type, and was invisible while the pattern knew three macros."""
    body = "/// @test\n/// Holds for every type in the list.\nTYPED_TEST(Guarded, Assign)\n"
    found = {a.key: a for a in scan_file(write(tmp_path, body))}
    assert found["Guarded.Assign"].description == "Holds for every type in the list."


def test_a_test_with_no_annotation_is_still_found(tmp_path):
    """It has to appear in the report as undescribed rather than vanish from it."""
    found = {a.key: a for a in scan_file(write(tmp_path))}
    assert not found["Bare.NoAnnotationAtAll"].description
    assert found["Bare.NoAnnotationAtAll"].requirements == ()


def test_a_block_separated_from_its_macro_does_not_attach(tmp_path):
    """A comment followed by code then a macro belongs to the code, not the macro."""
    body = "/// @test\n/// Belongs to the helper.\nstatic int helper() { return 0; }\n\nTEST(Suite, Case)\n"
    found = {a.key: a for a in scan_file(write(tmp_path, body))}
    assert not found["Suite.Case"].description


def test_build_directories_are_skipped(tmp_path):
    """Generated copies under build/ would double every annotation."""
    (tmp_path / "build").mkdir()
    (tmp_path / "build" / "copy_test.cpp").write_text(SOURCE)
    write(tmp_path)
    assert len(scan(tmp_path)) == 4
