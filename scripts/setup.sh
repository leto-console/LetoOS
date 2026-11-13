#!/bin/bash

echo "[LetoOS] Собираем проект..."
set -e

(
    cd "$(dirname "${BASH_SOURCE}")" || exit 1

    ./preset_setup.sh LetoOS linux-debug
    ./preset_setup.sh LetoOS stm32f411xe-debug
)
