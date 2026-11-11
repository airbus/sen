# === hooks.py =========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""MkDocs hooks to dynamically inject metadata and handle documentation post-processing."""

import logging
import subprocess
from datetime import datetime, timezone

logger = logging.getLogger(__name__)


def on_page_markdown(markdown, page, config, files):  # noqa: ARG001
    """Injects the last Git modification date into the metadata of snippet pages and licenses."""
    src_path = page.file.src_path
    original_path = None

    if src_path.startswith("snippets/examples/"):
        original_path = src_path.replace("snippets/examples/", "examples/", 1)

    if original_path:
        try:
            git_date = (
                subprocess.check_output(
                    ["git", "log", "-1", "--format=%at", original_path],
                    stderr=subprocess.DEVNULL,
                )
                .decode("utf-8")
                .strip()
            )

            if git_date:
                date_obj = datetime.fromtimestamp(int(git_date), tz=timezone.utc)
                page.meta["git_revision_date_localized"] = date_obj.strftime("%B %d, %Y")
        except subprocess.SubprocessError:
            logger.warning("Failed to get git log for %s", original_path)
        except ValueError:
            logger.warning("Failed to parse git timestamp for %s", original_path)

    if "git_revision_date_localized" not in page.meta and src_path == "licenses/licensed_code_in_sen.md":
        page.meta["git_revision_date_localized"] = datetime.now(timezone.utc).strftime("%B %d, %Y")

    return markdown
