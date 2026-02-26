# === color_cmake_target_graph.py ======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import re
import sys

input_path, output_path = sys.argv[1], sys.argv[2]

COLORS_DATA = {
    "egg": {"outline": "#f9a825", "fill": "#fbc02d"},  # executables
    "doubleoctagon": {"outline": "#7e57c2", "fill": "#9575cd"},  # shared libs
    "octagon": {"outline": "#039be5", "fill": "#afeeee"},  # static libs
    "hexagon": {"outline": "#e57373", "fill": "#ef9a9a"},  # object libs
    "tripleoctagon": {"outline": "#fb8c00", "fill": "#ffcc80"},  # module libs
    "pentagon": {"outline": "#4caf50", "fill": "#81c784"},  # interface libs
    "box": {"outline": "#66bb6a", "fill": "#a5d6a7"}  # custom target
}

shape_re = re.compile(r'(shape\s*=\s*\"?([a-zA-z0-9_]+)\"?)')

with open(input_path, "r", encoding="utf-8") as f:
    lines = f.readlines()

out_lines = []

for line in lines:
    if "[" in line and "shape" in line and "]" in line:
        m = shape_re.search(line)
        if m:
            shape = m.group(2)
            data = COLORS_DATA.get(shape)
            if data:
                color = data.get("outline")
                fillcolor = data.get("fill")
                line = line.replace(']', f', color="{color}", fillcolor="{fillcolor}"]', 1)
    out_lines.append(line)

with open(output_path, "w", encoding="utf-8") as f:
    f.writelines(out_lines)
