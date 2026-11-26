#!/usr/bin/env bash
# Compatibility entry point: the canonical build system is the GRUB-based Makefile.
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "$ROOT"
exec make "$@"
