#!/usr/bin/env bash
set -euo pipefail

git config core.hooksPath .githooks
echo "Installed Aetheris-IUI git hooks from .githooks"
