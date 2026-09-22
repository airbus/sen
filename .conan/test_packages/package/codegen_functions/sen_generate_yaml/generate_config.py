# === generate_config.py ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Generate a YAML fixture using the module produced by sen_generate_python()."""

import importlib
import sys
import types
from pathlib import Path

# Generated modules import yaml, but this fixture only needs their dataclass definitions.
# TODO SEN-1844 remove unused yaml import
sys.modules.setdefault("yaml", types.ModuleType("yaml"))
generated_module = importlib.import_module("generated_python")
generated_config_type = generated_module.GeneratedConfig
config = generated_config_type(enabled=True, name="generated-by-sen_generate_yaml")

output_file = Path(sys.argv[1])
output_file.write_text(
    f"enabled: {str(config.enabled).lower()}\nname: {config.name}\ntype: {config.__class__.__name__}\n",
    encoding="utf-8",
)
