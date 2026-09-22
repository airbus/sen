#!/usr/bin/env bash
# Runs a script from standard input where the job builds: inside the CI image when
# SEN_CI_IMAGE names one, on the runner when it does not. The Windows and arm legs
# have no image, so one copy of a step's commands serves every leg rather than two
# that drift apart.
set -euo pipefail

if [ -n "${SEN_CI_IMAGE:-}" ]; then
    exec "$(dirname "$0")/in_image.sh"
fi
exec bash -seuo pipefail
